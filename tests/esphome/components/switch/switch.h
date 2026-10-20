#pragma once
#include <optional>
#include "esphome/core/helpers.h"
namespace esphome::switch_ {
struct Switch : Entity {
  bool state{false};
  std::optional<bool> saved_state;
  std::optional<bool> get_initial_state_with_restore_mode() { return saved_state.value_or(false); }
  void publish_state(bool value) { state = value; saved_state = value; flags_.has_state = true; }
  void turn_on() { write_state(true); }
  void turn_off() { write_state(false); }
 protected:
  virtual void write_state(bool value) = 0;
};
}
