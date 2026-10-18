#pragma once
#include "esphome/core/helpers.h"
namespace esphome::text_sensor {
struct TextSensor : Entity {
  std::string state;
  void publish_state(const std::string &x) {
    state = x;
    flags_.has_state = true;
  }
};
}  // namespace esphome::text_sensor
