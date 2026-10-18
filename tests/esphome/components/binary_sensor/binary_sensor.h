#pragma once
#include "esphome/core/helpers.h"
namespace esphome::binary_sensor {
struct BinarySensor : Entity {
  bool state = false;
  void publish_state(bool x) {
    state = x;
    flags_.has_state = true;
  }
  void invalidate_state() { flags_.has_state = false; }
};
}  // namespace esphome::binary_sensor
