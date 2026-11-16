#include <regex>
#include "nspanel_lovelace.h"

#include "esphome/core/alloc_helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

#include <cinttypes>
#include <esp_heap_caps.h>

namespace esphome {
namespace nspanel_lovelace {
static const char *const TAG = "nspanel_lovelace_upload";

// Followed guide
// https://unofficialnextion.com/t/nextion-upload-protocol-v1-2-the-fast-one/1044/2

int NSPanelLovelace::upload_by_chunks_(const std::string &url, int range_start) {
  ESP_LOGVV(TAG, "url: %s", url.c_str());
  uint range_size = this->tft_size_ - range_start;
  ESP_LOGVV(TAG, "tft_size_: %i", this->tft_size_);
  ESP_LOGV(TAG, "Available heap: %" PRIu32, esp_get_free_heap_size());
  int range_end = (range_start == 0) ? std::min(this->tft_size_, 16383) : this->tft_size_;
  if (range_size <= 0 or range_end <= range_start) {
    ESP_LOGE(TAG, "Invalid range");
    ESP_LOGD(TAG, "Range start: %i", range_start);
    ESP_LOGD(TAG, "Range end: %i", range_end);
    ESP_LOGD(TAG, "Range size: %i", range_size);
    return -1;
  }

  esp_http_client_config_t config = {
      .url = url.c_str(),
      .cert_pem = nullptr,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);

  char range_header[64];
  sprintf(range_header, "bytes=%d-%d", range_start, range_end);
  ESP_LOGV(TAG, "Requesting range: %s", range_header);
  esp_http_client_set_header(client, "Range", range_header);
  ESP_LOGVV(TAG, "Available heap: %" PRIu32, esp_get_free_heap_size());

  ESP_LOGV(TAG, "Opening http connetion");
  esp_err_t err;
  if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return -1;
  }

  ESP_LOGV(TAG, "Fetch content length");
  int content_length = esp_http_client_fetch_headers(client);
  ESP_LOGV(TAG, "content_length = %d", content_length);
  if (content_length <= 0) {
    ESP_LOGE(TAG, "Failed to get content length: %d", content_length);
    esp_http_client_cleanup(client);
    return -1;
  }

  ESP_LOGV(TAG, "Allocate buffer");
  uint8_t *buffer = new uint8_t[4096];
  std::string recv_string;
  if (buffer == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate memory for buffer");
    ESP_LOGV(TAG, "Available heap: %" PRIu32, esp_get_free_heap_size());
  } else {
    ESP_LOGV(TAG, "Memory for buffer allocated successfully");

    while (true) {
      App.feed_wdt();
      ESP_LOGVV(TAG, "Available heap: %" PRIu32, esp_get_free_heap_size());
      int read_len = esp_http_client_read(client, reinterpret_cast<char *>(buffer), 4096);
      ESP_LOGVV(TAG, "Read %d bytes from HTTP client, writing to UART", read_len);
      if (read_len > 0) {
        this->write_array(buffer, read_len);
        ESP_LOGVV(TAG, "Write to UART successful");
        this->recv_ret_string_(recv_string, 5000, true);
        this->content_length_ -= read_len;
        ESP_LOGD(TAG, "Uploaded %0.2f %%, remaining %d bytes",
                 100.0 * (this->tft_size_ - this->content_length_) / this->tft_size_, this->content_length_);
        if (recv_string[0] != 0x05) {  // 0x05 == "ok"
          ESP_LOGD(
              TAG, "recv_string [%s]",
              format_hex_pretty(reinterpret_cast<const uint8_t *>(recv_string.data()), recv_string.size()).c_str());
        }
        // handle partial upload request
        if (recv_string[0] == 0x08 && recv_string.size() == 5) {
          uint32_t result = 0;
          for (int j = 0; j < 4; ++j) {
            result += static_cast<uint8_t>(recv_string[j + 1]) << (8 * j);
          }
          if (result > 0) {
            ESP_LOGI(TAG, "Nextion reported new range %" PRIu32, result);
            this->content_length_ = this->tft_size_ - result;
            // Deallocate the buffer when done
            delete[] buffer;
            ESP_LOGVV(TAG, "Memory for buffer deallocated");
            esp_http_client_cleanup(client);
            return result;
          }
        }
        recv_string.clear();
      } else if (read_len == 0) {
        ESP_LOGV(TAG, "End of HTTP response reached");
        break;  // Exit the loop if there is no more data to read
      } else {
        ESP_LOGE(TAG, "Failed to read from HTTP client, error code: %d", read_len);
        break;  // Exit the loop on error
      }
    }

    // Deallocate the buffer when done
    delete[] buffer;
    ESP_LOGVV(TAG, "Memory for buffer deallocated");
  }
  esp_http_client_cleanup(client);
  return range_end + 1;
}

void NSPanelLovelace::init_upload(const std::string &url) {
  // Define the configuration for the HTTP client
  ESP_LOGV(TAG, "Establishing connection to HTTP server");
  ESP_LOGVV(TAG, "Available heap: %" PRIu32, esp_get_free_heap_size());
  esp_http_client_config_t config = {
      .url = url.c_str(),
      .cert_pem = nullptr,
      .method = HTTP_METHOD_HEAD,
      .timeout_ms = 15000,
  };

  // Initialize the HTTP client with the configuration
  ESP_LOGV(TAG, "Initializing HTTP client");
  ESP_LOGV(TAG, "Available heap: %" PRIu32, esp_get_free_heap_size());
  esp_http_client_handle_t http = esp_http_client_init(&config);
  if (!http) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client.");
    return this->upload_end_();
  }

  // Perform the HTTP request
  ESP_LOGV(TAG, "Check if the client could connect");
  ESP_LOGV(TAG, "Available heap: %" PRIu32, esp_get_free_heap_size());
  esp_err_t err = esp_http_client_perform(http);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(http);
    return this->upload_end_();
  }

  // Check the HTTP Status Code
  int status_code = esp_http_client_get_status_code(http);
  ESP_LOGV(TAG, "HTTP Status Code: %d", status_code);
  size_t tft_file_size = esp_http_client_get_content_length(http);
  ESP_LOGD(TAG, "TFT file size: %zu", tft_file_size);

  if (tft_file_size < 4096) {
    ESP_LOGE(TAG, "File size check failed. Size: %zu", tft_file_size);
    esp_http_client_cleanup(http);
    return this->upload_end_();
  } else {
    ESP_LOGV(TAG, "File size check passed. Proceeding...");
  }
  esp_http_client_cleanup(http);
  this->content_length_ = tft_file_size;
  this->tft_size_ = tft_file_size;
}

bool NSPanelLovelace::wait_for_comok_() {
  static const uint8_t ATTEMPTS = 3;
  static const uint32_t TIMEOUT_MS = 1000;

  for (uint8_t attempt = 1; attempt <= ATTEMPTS; attempt++) {
    if (attempt > 1) {
      ESP_LOGD(TAG, "No comok from the display, reconnecting (%u/%u)", attempt, ATTEMPTS);
      this->start_reparse_mode();
    }

    std::string response;
    uint32_t start = millis();
    uint32_t elapsed;
    while ((elapsed = millis() - start) < TIMEOUT_MS) {
      this->recv_ret_string_(response, TIMEOUT_MS - elapsed, false);
      if (response.find("comok") != std::string::npos) {
        ESP_LOGD(TAG, "Display connected after %" PRIu32 " ms", millis() - start);
        return true;
      }
    }
  }
  return false;
}

void NSPanelLovelace::upload_tft(const std::string &url) {
  if (this->is_updating_) {
    ESP_LOGD(TAG, "Currently updating");
    return;
  }

  if (!network::is_connected()) {
    ESP_LOGD(TAG, "network is not connected");
    return;
  }

  if (!this->reparse_mode_) {
    this->start_reparse_mode();
  }
  if (!this->wait_for_comok_()) {
    ESP_LOGW(TAG, "No comok from the display, attempting the upload anyway");
  }

  this->is_updating_ = true;

  // Unsubscribe so incoming MQTT messages don't consume heap needed by the upload.
  // The device restarts when the upload ends, so the subscriptions are never restored.
  this->mqtt_->unsubscribe(this->send_topic_);
  if (this->berry_driver_version_ > 0) {
    this->mqtt_->unsubscribe(std::regex_replace(this->send_topic_, std::regex("CustomSend"), "GetDriverVersion"));
    this->mqtt_->unsubscribe(std::regex_replace(this->send_topic_, std::regex("CustomSend"), "FlashNextion"));
  }

  this->init_upload(url);

  if (this->content_length_ < 4096) {
    ESP_LOGE(TAG, "Failed to get file size");
    this->upload_end_();
  }

  ESP_LOGD(TAG, "Updating Nextion");
  // The Nextion will ignore the update command if it is sleeping

  char command[128];
  // Tells the Nextion the content length of the tft file and baud rate it will be sent at
  // Once the Nextion accepts the command it will wait until the file is successfully uploaded
  // If it fails for any reason a power cycle of the display will be needed
  sprintf(command, "whmi-wris %d,%d,1", this->content_length_, this->update_baud_rate_);

  // Clear serial receive buffer
  uint8_t d;
  while (this->available()) {
    this->read_byte(&d);
  };

  this->send_nextion_command(command);

  // Since 115200 is default and the communication wouldn't work even if it was overridden
  // in the uart component initialization, it should be safe to ignore setting the baud rate
  // if it's set to 115200. It helps to avoid additional unnecessary HardwareSerial re-init.
  if (this->update_baud_rate_ != 115200) {
    this->set_baud_rate_(this->update_baud_rate_);
  }

  std::string response;
  ESP_LOGD(TAG, "Waiting for upgrade response");
  this->recv_ret_string_(response, 2000, true);  // This can take some time to return

  // The Nextion display will, if it's ready to accept data, send a 0x05 byte.
  ESP_LOGD(TAG, "Upgrade response is [%s]",
           format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str());

  if (response.find(0x05) != std::string::npos) {
    ESP_LOGD(TAG, "preparation for tft update done");
  } else {
    ESP_LOGD(TAG, "preparation for tft update failed %d \"%s\"", response[0], response.c_str());
    this->upload_end_();
  }

  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  ESP_LOGD(TAG, "Updating tft from \"%s\" with a file size of %d, Heap Size %" PRIu32, url.c_str(),
           this->content_length_, esp_get_free_heap_size());

  int result = 0;
  while (this->content_length_ > 0) {
    result = this->upload_by_chunks_(url, result);
    if (result < 0) {
      ESP_LOGD(TAG, "Error updating Nextion!");
      this->upload_end_();
    }
    App.feed_wdt();
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    ESP_LOGD(TAG, "Heap Size %" PRIu32 ", Bytes left %d", esp_get_free_heap_size(), this->content_length_);
  }
  ESP_LOGD(TAG, "Successfully updated Nextion!");

  this->upload_end_();
}

void NSPanelLovelace::upload_end_() {
  ESP_LOGD(TAG, "Restarting Nextion");
  this->soft_reset();
  delay(1500);  // NOLINT
  ESP_LOGD(TAG, "Restarting esphome");
  esp_restart();
}
}  // namespace nspanel_lovelace
}  // namespace esphome
