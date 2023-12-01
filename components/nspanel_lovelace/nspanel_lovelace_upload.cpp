#include "nspanel_lovelace.h"

#include "esphome/core/application.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

#include <esp_heap_caps.h>

namespace esphome {
namespace nspanel_lovelace {
static const char *const TAG = "nspanel_lovelace_upload";

// Followed guide
// https://unofficialnextion.com/t/nextion-upload-protocol-v1-2-the-fast-one/1044/2

#ifdef USE_ARDUINO
int NSPanelLovelace::upload_by_chunks_(HTTPClient *http, const std::string &url, int range_start) {
  int range_end;

  if (range_start == 0 && this->transfer_buffer_size_ > 16384) {  // Start small at the first run in case of a big skip
    range_end = 16384 - 1;
  } else {
    range_end = range_start + this->transfer_buffer_size_ - 1;
  }

  if (range_end > this->tft_size_)
    range_end = this->tft_size_;

  char range_header[64];
  sprintf(range_header, "bytes=%d-%d", range_start, range_end);

  ESP_LOGD(TAG, "Requesting range: %s", range_header);

  int tries = 1;
  int code;
  bool begin_status;
  while (tries <= 5) {
    begin_status = http->begin(url.c_str());

    ++tries;
    if (!begin_status) {
      ESP_LOGD(TAG, "upload_by_chunks_: connection failed");
      continue;
    }

    http->addHeader("Range", range_header);

    code = http->GET();
    if (code == 200 || code == 206) {
      break;
    }
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s, retries(%d/5)", url.c_str(),
             HTTPClient::errorToString(code).c_str(), tries);
    http->end();
    delay(500);  // NOLINT
  }

  if (tries > 5) {
    return -1;
  }

  std::string recv_string;
  size_t size;
  int fetched = 0;
  int range = range_end - range_start;
  int write_len;

  // fetch next segment from HTTP stream
  while (fetched < range) {
    size = http->getStreamPtr()->available();
    if (!size) {
      App.feed_wdt();
      delay(2);
      continue;
    }
    int c = http->getStreamPtr()->readBytes(
        &this->transfer_buffer_[fetched], ((size > this->transfer_buffer_size_) ? this->transfer_buffer_size_ : size));
    fetched += c;
  }
  http->end();
  ESP_LOGD(TAG, "fetched %d bytes", fetched);

  // upload fetched segments to the display in 4KB chunks
  for (int i = 0; i < range; i += 4096) {
    App.feed_wdt();
    write_len = this->content_length_ < 4096 ? this->content_length_ : 4096;
    this->write_array(&this->transfer_buffer_[i], write_len);
    this->content_length_ -= write_len;
    ESP_LOGD(TAG, "Uploaded %0.1f %%, remaining %d B",
             100.0 * (this->tft_size_ - this->content_length_) / this->tft_size_, this->content_length_);

    if (!this->upload_first_chunk_sent_) {
      this->upload_first_chunk_sent_ = true;
      delay(500);  // NOLINT
    }

    this->recv_ret_string_(recv_string, 5000, true);
    if (recv_string[0] != 0x05) { // 0x05 == "ok"
      ESP_LOGD(TAG, "recv_string [%s]",
               format_hex_pretty(reinterpret_cast<const uint8_t *>(recv_string.data()), recv_string.size()).c_str());
    }

    // handle partial upload request
    if (recv_string[0] == 0x08 && recv_string.size() == 5) {
      uint32_t result = 0;
      for (int j = 0; j < 4; ++j) {
        result += static_cast<uint8_t>(recv_string[j + 1]) << (8 * j);
      }
      if (result > 0) {
        ESP_LOGD(TAG, "Nextion reported new range %d", result);
        this->content_length_ = this->tft_size_ - result;
        return result;
      }
    }

    recv_string.clear();
  }
  return range_end + 1;
}
#else
int NSPanelLovelace::upload_by_chunks_(const std::string &url, int range_start) {
  ESP_LOGVV(TAG, "url: %s", url.c_str());
  uint range_size = this->tft_size_ - range_start;
  ESP_LOGVV(TAG, "tft_size_: %i", this->tft_size_);
  ESP_LOGV(TAG, "Available heap: %u", esp_get_free_heap_size());
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
  ESP_LOGVV(TAG, "Available heap: %u", esp_get_free_heap_size());

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
    ESP_LOGV(TAG, "Available heap: %u", esp_get_free_heap_size());
  } else {
    ESP_LOGV(TAG, "Memory for buffer allocated successfully");

    while (true) {
      App.feed_wdt();
      ESP_LOGVV(TAG, "Available heap: %u", esp_get_free_heap_size());
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
            esp_http_client_close(client);
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
  esp_http_client_close(client);
  return range_end + 1;
}
#endif

#ifdef USE_ARDUINO
void NSPanelLovelace::init_upload(HTTPClient *http, const std::string &url) {
  http->setTimeout(15000);  // Yes 15 seconds.... Helps 8266s along
  http->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  bool begin_status = http->begin(url.c_str());

  if (!begin_status) {
    this->is_updating_ = false;
    ESP_LOGD(TAG, "connection failed");
    ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    allocator.deallocate(this->transfer_buffer_, this->transfer_buffer_size_);
    return;
  } else {
    ESP_LOGD(TAG, "Connected");
  }

  http->addHeader("Range", "bytes=0-255");
  const char *header_names[] = {"Content-Range"};
  http->collectHeaders(header_names, 1);
  ESP_LOGD(TAG, "Requesting URL: %s", url.c_str());

  http->setReuse(true);
  // try up to 5 times. DNS sometimes needs a second try or so
  int tries = 1;
  int code = http->GET();
  delay(100);  // NOLINT

  while (code != 200 && code != 206 && tries <= 5) {
    ESP_LOGW(TAG, "HTTP Request failed; URL: %s; Error: %s, retrying (%d/5)", url.c_str(),
             HTTPClient::errorToString(code).c_str(), tries);

    delay(250);  // NOLINT
    code = http->GET();
    ++tries;
  }

  if ((code != 200 && code != 206) || tries > 5) {
    this->upload_end_();
  }

  String content_range_string = http->header("Content-Range");
  content_range_string.remove(0, 12);
  this->content_length_ = content_range_string.toInt();
  this->tft_size_ = content_length_;
  http->end();
}
#elif defined(USE_ESP_IDF)
void NSPanelLovelace::init_upload(const std::string &url) {
  // Define the configuration for the HTTP client
  ESP_LOGV(TAG, "Establishing connection to HTTP server");
  ESP_LOGVV(TAG, "Available heap: %u", esp_get_free_heap_size());
  esp_http_client_config_t config = {
      .url = url.c_str(),
      .cert_pem = nullptr,
      .method = HTTP_METHOD_HEAD,
      .timeout_ms = 15000,
  };

  // Initialize the HTTP client with the configuration
  ESP_LOGV(TAG, "Initializing HTTP client");
  ESP_LOGV(TAG, "Available heap: %u", esp_get_free_heap_size());
  esp_http_client_handle_t http = esp_http_client_init(&config);
  if (!http) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client.");
    return this->upload_end_();
  }

  // Perform the HTTP request
  ESP_LOGV(TAG, "Check if the client could connect");
  ESP_LOGV(TAG, "Available heap: %u", esp_get_free_heap_size());
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
  this->content_length_ = tft_file_size;
  this->tft_size_ = tft_file_size;
}
#endif

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

  this->is_updating_ = true;

#ifdef USE_ARDUINO
  HTTPClient http;
  this->init_upload(&http, url);
#elif defined(USE_ESP_IDF)
  this->init_upload(url);
#endif

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

  // Nextion wants 4096 bytes at a time. Make chunk_size a multiple of 4096
  uint32_t chunk_size = 8192;
  if (esp_get_free_heap_size() > 40960) {  // 32K to keep on hand
    chunk_size = esp_get_free_heap_size() - 32768;
    chunk_size = chunk_size > 65536 ? 65536 : chunk_size;
  } else if (esp_get_free_heap_size() < 10240) {
    chunk_size = 4096;
  }


  if (this->transfer_buffer_ == nullptr) {
    ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    ESP_LOGD(TAG, "Allocating buffer size %d, Heap size is %u", chunk_size, esp_get_free_heap_size());
    this->transfer_buffer_ = allocator.allocate(chunk_size);
    if (this->transfer_buffer_ == nullptr) {  // Try a smaller size
      ESP_LOGD(TAG, "Could not allocate buffer size: %d trying 4096 instead", chunk_size);
      chunk_size = 4096;
      ESP_LOGD(TAG, "Allocating %d buffer", chunk_size);
      this->transfer_buffer_ = allocator.allocate(chunk_size);

      if (!this->transfer_buffer_)
        this->upload_end_();
    }

    this->transfer_buffer_size_ = chunk_size;
  }

  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  ESP_LOGD(TAG, "Updating tft from \"%s\" with a file size of %d using %zu chunksize, Heap Size %d",
           url.c_str(), this->content_length_, this->transfer_buffer_size_, esp_get_free_heap_size());

  int result = 0;
  while (this->content_length_ > 0) {
#ifdef USE_ARDUINO
    result = this->upload_by_chunks_(&http, url, result);
#elif defined(USE_ESP_IDF)
    result = this->upload_by_chunks_(url, result);
#endif
    if (result < 0) {
      ESP_LOGD(TAG, "Error updating Nextion!");
      this->upload_end_();
    }
    App.feed_wdt();
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    ESP_LOGD(TAG, "Heap Size %d, Bytes left %d", esp_get_free_heap_size(), this->content_length_);
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
