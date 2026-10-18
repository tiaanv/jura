#pragma once
#include "esphome/core/helpers.h"
namespace esphome::button {
struct Button : Entity {
 protected:
  virtual void press_action() {};
};
}  // namespace esphome::button
