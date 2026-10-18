#pragma once
#include "esphome/core/helpers.h"
namespace esphome::number {
struct Number : Entity {
  struct Traits {
    float low = 0, high = 2000, step = 1;
    void set_min_value(float x) { low = x; }
    void set_max_value(float x) { high = x; }
    void set_step(float x) { step = x; }
  } traits;
  float state = 0;
  void publish_state(float x) {
    state = x;
    flags_.has_state = true;
  }

 protected:
  virtual void control(float) {};
};
}  // namespace esphome::number
