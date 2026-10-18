#pragma once
#include <cmath>
#include <cstdlib>
#include <string>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace jura_coolcontrol {
// Passive receiver; retains the original component's observed ASCII offsets.
class JuraCoolcontrol : public PollingComponent, public uart::UARTDevice {
 public:
  void set_level_sensor(sensor::Sensor *s) { level_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { temperature_ = s; }
  void loop() override {
    uint8_t value;
    for (size_t n = 0; n < 128 && available(); ++n) {
      if (!read_byte(&value)) break;
      if (!line_.empty() && uint32_t(millis() - last_byte_) > 500) line_.clear();
      last_byte_ = millis();
      if (line_.size() >= 128) {
        line_.clear();
        continue;
      }
      line_ += static_cast<char>(value);
      if (line_.size() < 2 || line_.compare(line_.size() - 2, 2, "\r\n") != 0) continue;
      if (line_.size() >= 10 && is_hex_(line_.substr(4, 4))) {
        const unsigned level = unsigned(std::strtoul(line_.substr(4, 2).c_str(), nullptr, 16));
        const unsigned temp = unsigned(std::strtoul(line_.substr(6, 2).c_str(), nullptr, 16));
        if (level <= 100) {
          last_level_ = float(level);
          last_temperature_ = temp == 255 ? NAN : temp / 10.0f;
          last_frame_ = millis();
          seen_ = true;
        }
      }
      line_.clear();
    }
  }
  void update() override {
    const bool fresh = seen_ && uint32_t(millis() - last_frame_) < 10000;
    if (level_) level_->publish_state(fresh ? last_level_ : NAN);
    if (temperature_) temperature_->publish_state(fresh ? last_temperature_ : NAN);
  }

 private:
  static bool is_hex_(const std::string &s) {
    for (char c : s)
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    return true;
  }
  sensor::Sensor *level_{nullptr}, *temperature_{nullptr};
  std::string line_;
  uint32_t last_byte_{0}, last_frame_{0};
  bool seen_{false};
  float last_level_{NAN}, last_temperature_{NAN};
};
}  // namespace jura_coolcontrol
}  // namespace esphome
