#pragma once
#include "esphome/core/helpers.h"
namespace esphome::select {
struct Select : Entity {
  struct Traits {
    FixedVector<const char *> values;
    void set_options(const FixedVector<const char *> &v) { values = v; }
  } traits;
  std::string state;
  void publish_state(const std::string &x) {
    state = x;
    flags_.has_state = true;
  }

 protected:
  virtual void control(const std::string &) {};
};
}  // namespace esphome::select
