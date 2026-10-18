#pragma once
#include "esphome/core/helpers.h"
namespace esphome::sensor {
struct Sensor : Entity {
  float state = 0;
  void publish_state(float x) {
    state = x;
    flags_.has_state = true;
  }
};
}  // namespace esphome::sensor
