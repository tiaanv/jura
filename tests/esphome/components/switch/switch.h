#pragma once
#include "esphome/core/helpers.h"
namespace esphome::switch_ {
struct Switch : Entity {
  bool state{false};
  void publish_state(bool value) { state = value; flags_.has_state = true; }
  void turn_on() { write_state(true); }
  void turn_off() { write_state(false); }
 protected:
  virtual void write_state(bool value) = 0;
};
}
