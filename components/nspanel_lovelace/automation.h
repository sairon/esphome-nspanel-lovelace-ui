#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "nspanel_lovelace.h"

namespace esphome {
namespace nspanel_lovelace {

class NSPanelLovelaceMsgIncomingTrigger : public Trigger<std::string> {
 public:
  explicit NSPanelLovelaceMsgIncomingTrigger(NSPanelLovelace *parent) {
    parent->add_incoming_msg_callback([this](const std::string &value) { this->trigger(value); });
  }
};

}  // namespace nspanel_lovelace
}  // namespace esphome
