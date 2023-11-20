#pragma once

#include <utility>

#include "esphome/components/uart/uart.h"
#include "esphome/components/uart/uart_component_esp32_arduino.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <HTTPClient.h>

#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif

#ifdef USE_API
#include "esphome/components/api/custom_api_device.h"
#endif

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace nspanel_lovelace {

class NSPanelLovelace : public Component, public uart::UARTDevice, 
  #ifdef USE_API
    public esphome::api::CustomAPIDevice
  #endif 
{
 public:
  void setup() override;
  void loop() override;

  #ifdef USE_MQTT
  void set_mqtt(mqtt::MQTTClientComponent *parent) { mqtt_ = parent; }
  void set_recv_topic(const std::string &topic) { recv_topic_ = topic; }
  void set_send_topic(const std::string &topic) { send_topic_ = topic; }
  #endif
  void set_berry_driver_version(unsigned int value) { berry_driver_version_ = value; }
  void set_missed_updates_workaround(bool value) { use_missed_updates_workaround_ = value; }
  void set_update_baud_rate(unsigned int value) { update_baud_rate_ = value; }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void dump_config() override;

  void send_custom_command(const std::string &command);

  void add_message_from_nextion_callback(std::function<void(std::string)> callback) { this->message_from_nextion_callback_.add(std::move(callback)); }
  void add_message_to_nextion_callback(std::function<void(std::string&)> callback) { this->message_to_nextion_callback_.add(std::move(callback)); }

  void send_nextion_command(const std::string &command);

  void process_command_from_nextion(const std::string &message);

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

  void app_custom_send(const std::string &payload);
  int app_get_driver_version();
  void app_flash_nextion(const std::string &payload);

 protected:
  void set_baud_rate_(int baud_rate);

  static uint16_t crc16(const uint8_t *data, uint16_t len);

  uint16_t recv_ret_string_(std::string &response, uint32_t timeout, bool recv_flag);

  bool process_data_();

  #ifdef USE_MQTT
  mqtt::MQTTClientComponent *mqtt_;
  std::string recv_topic_;
  std::string send_topic_;
  #endif
  unsigned int berry_driver_version_;
  bool use_missed_updates_workaround_ = true;
  unsigned int update_baud_rate_;

  CallbackManager<void(std::string)> message_from_nextion_callback_;
  CallbackManager<void(std::string&)> message_to_nextion_callback_;

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
