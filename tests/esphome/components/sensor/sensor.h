#pragma once
#include "esphome/core/helpers.h"
#include <functional>
namespace esphome::sensor {
struct Sensor : Entity {
  std::vector<std::function<void(float)>> raw_callbacks;
  void add_on_raw_state_callback(std::function<void(float)> callback) { raw_callbacks.push_back(callback); }
  float state = 0;
  void publish_state(float x) {
    for (auto &callback : raw_callbacks) callback(x);
    state = x;
    flags_.has_state = true;
  }
};
}  // namespace esphome::sensor
