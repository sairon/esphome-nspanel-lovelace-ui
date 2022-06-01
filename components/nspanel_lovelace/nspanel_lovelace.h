#pragma once

#include <utility>

#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <HTTPClient.h>

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace nspanel_lovelace {

class NSPanelLovelace : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;

  void set_mqtt(mqtt::MQTTClientComponent *parent) { mqtt_ = parent; }
  void set_recv_topic(const std::string &topic) { recv_topic_ = topic; }
  void set_send_topic(const std::string &topic) { send_topic_ = topic; }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void dump_config() override;

  void send_custom_command(const std::string &command);

  void add_incoming_msg_callback(std::function<void(std::string)> callback);

  void send_nextion_command(const std::string &command);

  /**
   * Softreset the Nextion
   */
  void soft_reset();

  void start_reparse_mode();

  void exit_reparse_mode();

  /**
   * Set the tft file URL. https seems problamtic with arduino..
   */
  void set_tft_url(const std::string &tft_url) { this->tft_url_ = tft_url; }

  /**
   * Upload the tft file and softreset the Nextion
   */
  void upload_tft(const std::string &url);

 protected:
  static uint16_t crc16(const uint8_t *data, uint16_t len);

  uint16_t recv_ret_string_(std::string &response, uint32_t timeout, bool recv_flag);

  bool process_data_();
  void process_command_(const std::string &message);

  mqtt::MQTTClientComponent *mqtt_;
  std::string recv_topic_;
  std::string send_topic_;

  CallbackManager<void(std::string)> incoming_msg_callback_;

  std::vector<uint8_t> buffer_;

  bool is_updating_ = false;
  bool reparse_mode_ = false;

  std::string tft_url_;
  uint8_t *transfer_buffer_{nullptr};
  size_t transfer_buffer_size_;
  bool upload_first_chunk_sent_ = false;

  /**
   * will request chunk_size chunks from the web server
   * and send each to the nextion
   * @param int contentLength Total size of the file
   * @param uint32_t chunk_size
   * @return true if success, false for failure.
   */
  int content_length_ = 0;
  int tft_size_ = 0;
  int upload_by_chunks_(HTTPClient *http, const std::string &url, int range_start);

  void upload_end_();
};


}  // namespace nspanel_lovelace
}  // namespace esphome
