#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "nspanel_lovelace.h"

namespace esphome {
namespace nspanel_lovelace {

class NSPanelLovelaceMessageFromNextionTrigger : public Trigger<std::string> {
 public:
  explicit NSPanelLovelaceMessageFromNextionTrigger(NSPanelLovelace *parent) {
    parent->add_message_from_nextion_callback([this](const std::string &value) { this->trigger(value); });
  }
};

class NSPanelLovelaceMessageToNextionTrigger : public Trigger<std::string&> {
 public:
  explicit NSPanelLovelaceMessageToNextionTrigger(NSPanelLovelace *parent) {
    parent->add_message_to_nextion_callback([this](std::string &value) { this->trigger(value); });
  }
};

}  // namespace nspanel_lovelace
}  // namespace esphome
