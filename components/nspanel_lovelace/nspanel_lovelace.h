#pragma once

#include <utility>

#include "esphome/core/defines.h"
#include "esphome/components/api/custom_api_device.h"
#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include <esp_http_client.h>

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace nspanel_lovelace {

class NSPanelLovelace : public Component, public uart::UARTDevice, public api::CustomAPIDevice {
 public:
  void setup() override;
  void loop() override;

#ifdef USE_MQTT
  void set_mqtt(mqtt::MQTTClientComponent *parent) { mqtt_ = parent; }
#endif
  void set_connection(bool use_api) { use_api_ = use_api; }
  void set_api_device_id(const std::string &device_id) { api_device_id_ = device_id; }
  void set_recv_topic(const std::string &topic) { recv_topic_ = topic; }
  void set_send_topic(const std::string &topic) { send_topic_ = topic; }
  void set_berry_driver_version(unsigned int value) { berry_driver_version_ = value; }
  void set_missed_updates_workaround(bool value) { use_missed_updates_workaround_ = value; }
  void set_update_baud_rate(unsigned int value) { update_baud_rate_ = value; }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void dump_config() override;

  void send_custom_command(const std::string &command);

  void nspanelui_api_call(std::string data, int32_t command);

  void add_incoming_msg_callback(std::function<void(std::string)> callback);

  void send_nextion_command(const std::string &command);

  /**
   * Softreset the Nextion
   */
  void soft_reset();

  void start_reparse_mode();

  void exit_reparse_mode();

  /**
   * Upload the tft file and softreset the Nextion
   */
  void upload_tft(const std::string &url);

 protected:
  void set_baud_rate_(int baud_rate);
  bool wait_for_comok_();

  static uint16_t crc16(const uint8_t *data, uint16_t len);

  uint16_t recv_ret_string_(std::string &response, uint32_t timeout, bool recv_flag);

  bool process_data_();
  void process_command_(const std::string &message);

#ifdef USE_MQTT
  mqtt::MQTTClientComponent *mqtt_{nullptr};
#endif
  bool use_api_{false};
  std::string api_device_id_;
  std::string recv_topic_;
  std::string send_topic_;
  unsigned int berry_driver_version_;
  bool use_missed_updates_workaround_ = true;
  unsigned int update_baud_rate_;

  CallbackManager<void(std::string)> incoming_msg_callback_;

  std::vector<uint8_t> buffer_;

  bool is_updating_ = false;
  bool reparse_mode_ = false;

  int content_length_ = 0;
  int tft_size_ = 0;

  void init_upload(const std::string &url);

  int upload_by_chunks_(const std::string &url, int range_start);

  void upload_end_();
};


}  // namespace nspanel_lovelace
}  // namespace esphome
