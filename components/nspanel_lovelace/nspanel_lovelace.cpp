#include "nspanel_lovelace.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/util.h"

namespace esphome {
namespace nspanel_lovelace {

static const char *const TAG = "nspanel_lovelace";

void NSPanelLovelace::setup() {
}

void NSPanelLovelace::loop() {
  if (this->is_updating_ || this->reparse_mode_) {
    return;
  }

  uint8_t d;

  while (this->available()) {
    this->read_byte(&d);
    this->buffer_.push_back(d);
    if (!this->process_data_()) {
      ESP_LOGD(TAG, "Received: 0x%02x", d);
      this->buffer_.clear();
    }
  }
}

bool NSPanelLovelace::process_data_() {
  uint32_t at = this->buffer_.size() - 1;
  auto *data = &this->buffer_[0];
  uint8_t new_byte = data[at];

  // Byte 0: HEADER1 (always 0x55)
  if (at == 0)
    return new_byte == 0x55;
  // Byte 1: HEADER2 (always 0xBB)
  if (at == 1)
    return new_byte == 0xBB;

  // Byte 3 & 4 - length (little endian)
  if (at == 2 || at == 3){
    return true;
  }
  uint16_t length = encode_uint16(data[3], data[2]);

  // Wait until all data comes in
  if (at - 4 < length){
//    ESP_LOGD(TAG, "Message (%d/%d): 0x%02x", at - 3, length, new_byte);
    return true;
  }

  // Last two bytes: CRC; return after first one
  if (at == 4 + length) {
    return true;
  }

  uint16_t crc16 = encode_uint16(data[4 + length + 1], data[4 + length]);
  uint16_t calculated_crc16 = NSPanelLovelace::crc16(data, 4 + length);

  if (crc16 != calculated_crc16) {
    ESP_LOGW(TAG, "Received invalid message checksum %02X!=%02X", crc16, calculated_crc16);
//    return false; // FIXME: apparently there's a CRC bug in Nextion FW
  } else{
    ESP_LOGD(TAG, "Received valid message checksum %02X==%02X", crc16, calculated_crc16);
  }

  const uint8_t *message_data = data + 4;
  std::string message(message_data, message_data + length);

  ESP_LOGD(TAG, "Received from UI: PAYLOAD=%s RAW=[%s]", message.c_str(),
           format_hex_pretty(data, this->buffer_.size()).c_str());

  this->process_command_(message);
  this->buffer_.clear();
  return true;
}

void NSPanelLovelace::process_command_(const std::string &message) {
  this->incoming_msg_callback_.call(message);
}

void NSPanelLovelace::add_incoming_msg_callback(std::function<void(std::string)> callback) {
  this->incoming_msg_callback_.add(std::move(callback));
}

void NSPanelLovelace::dump_config() { ESP_LOGCONFIG(TAG, "NSPanelLovelace:"); }

void NSPanelLovelace::send_nextion_command(const std::string &command) {
  ESP_LOGD(TAG, "Sending: %s", command.c_str());
  this->write_str(command.c_str());
  const uint8_t to_send[3] = {0xFF, 0xFF, 0xFF};
  this->write_array(to_send, sizeof(to_send));
}

void NSPanelLovelace::send_custom_command(const std::string &command) {
  ESP_LOGD(TAG, "Sending custom command: %s", command.c_str());
  std::vector<uint8_t> data = {0x55, 0xBB};
  data.push_back(command.length() & 0xFF);
  data.push_back((command.length() >> 8) & 0xFF);
  data.insert(data.end(), command.begin(), command.end());
  auto crc = crc16(data.data(), data.size());
  data.push_back(crc & 0xFF);
  data.push_back((crc >> 8) & 0xFF);
  this->write_array(data);

  ESP_LOGD(TAG, "Sent to UI: PAYLOAD=%s RAW=[%s]", command.c_str(),
           format_hex_pretty(&data[0], data.size()).c_str());
}

void NSPanelLovelace::soft_reset() { this->send_nextion_command("rest"); }

uint16_t NSPanelLovelace::recv_ret_string_(std::string &response, uint32_t timeout, bool recv_flag) {
  uint16_t ret = 0;
  uint8_t c = 0;
  uint8_t nr_of_ff_bytes = 0;
  uint64_t start;
  bool exit_flag = false;
  bool ff_flag = false;

  start = millis();

  while ((timeout == 0 && this->available()) || millis() - start <= timeout) {
    this->read_byte(&c);
    if (c == 0xFF) {
      nr_of_ff_bytes++;
    } else {
      nr_of_ff_bytes = 0;
      ff_flag = false;
    }

    if (nr_of_ff_bytes >= 3)
      ff_flag = true;

    response += (char) c;
    if (recv_flag) {
      if (response.find(0x05) != std::string::npos) {
        exit_flag = true;
      }
    }
    App.feed_wdt();
    delay(1);

    if (exit_flag || ff_flag) {
      break;
    }
  }

  if (ff_flag)
    response = response.substr(0, response.length() - 3);  // Remove last 3 0xFF

  ret = response.length();
  return ret;
}

uint16_t NSPanelLovelace::crc16(const uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {

    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      if ((crc & 0x01) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void NSPanelLovelace::start_reparse_mode() {
  this->send_nextion_command("DRAKJHSUYDGBNCJHGJKSHBDN");
  this->send_nextion_command("recmod=0");
  this->send_nextion_command("recmod=0");
  this->send_nextion_command("connect");
  reparse_mode_ = true;
}

void NSPanelLovelace::exit_reparse_mode() {
  this->send_nextion_command("recmod=1");
  reparse_mode_ = false;
}

}  // namespace nspanel_lovelace
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO