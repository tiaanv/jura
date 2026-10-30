#pragma once
#include <cmath>
#include <string>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace jura_coolcontrol {
// Status names deliberately preserve the protocol: mn/me physical meanings are unconfirmed.
class JuraCoolcontrol : public PollingComponent, public uart::UARTDevice {
 public:
  void set_level_sensor(sensor::Sensor *s) { level_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { temperature_ = s; }
  void set_status_sensor(text_sensor::TextSensor *s) { status_sensor_ = s; }
  void set_raw_message_sensor(text_sensor::TextSensor *s) { raw_message_sensor_ = s; }
  void set_message_counter_sensor(sensor::Sensor *s) { message_counter_sensor_ = s; }
  void set_bridge_packet_sensor(sensor::Sensor *s) { bridge_packet_sensor_ = s; }
  void set_online_sensor(binary_sensor::BinarySensor *s) { online_sensor_ = s; }
  void set_communication_timeout(uint32_t ms) { communication_timeout_ = ms; }
  void set_measurement_timeout(uint32_t ms) { measurement_timeout_ = ms; }
  void set_startup_probe(const std::string &text, uint32_t delay, uint32_t interval, uint8_t attempts) {
    probe_text_ = text;
    probe_delay_ = delay;
    probe_interval_ = interval;
    probe_max_attempts_ = attempts;
  }
  void setup() override {
    started_ = millis();
    if (message_counter_sensor_) message_counter_sensor_->publish_state(0);
    publish_();
  }
  void loop() override {
    uint8_t value;
    for (size_t n = 0; n < 128 && available(); ++n) {
      if (!read_byte(&value)) break;
      const uint32_t now = millis();
      if ((!line_.empty() || discarding_) && uint32_t(now - last_byte_) > 500) {
        line_.clear();
        discarding_ = false;
      }
      last_byte_ = now;
      if (discarding_) {
        if (value == '\n') discarding_ = false;
        continue;
      }
      if (line_.size() >= 128) {
        line_.clear();
        discarding_ = value != '\n';
        continue;
      }
      line_ += static_cast<char>(value);
      if (line_.size() < 2 || line_.compare(line_.size() - 2, 2, "\r\n") != 0) continue;
      handle_frame_();
      line_.clear();
    }
    // Receive first: even a bare status from an older cooler cancels startup retries.
    const uint32_t now = millis();
    if (!seen_status_ && probe_attempts_ < probe_max_attempts_ &&
        (probe_attempts_ == 0 ? uint32_t(now - started_) >= probe_delay_
                              : uint32_t(now - last_probe_) >= probe_interval_)) {
      write_array(reinterpret_cast<const uint8_t *>(probe_text_.data()), probe_text_.size());
      last_probe_ = now;
      ++probe_attempts_;
    }
    publish_packet_(); // Withdraw promptly if recognized UART traffic stops.
  }
  void update() override { publish_(); }

 private:
  static int hex_(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }
  void handle_frame_() {
    const bool normal = line_.compare(0, 3, "@mo") == 0 || line_.compare(0, 3, "@mn") == 0;
    const bool extended = normal && line_.size() == 10 && line_[3] == ':';
    const bool bare = (normal || line_.compare(0, 3, "@me") == 0) && line_.size() == 5;
    const bool unavailable = line_ == "@me1\r\n";
    if (!bare && !extended && !unavailable) return;
    int level = -1, temp = -1;
    if (extended) {
      for (size_t i = 4; i < 8; ++i) if (hex_(line_[i]) < 0) return;
      level = hex_(line_[4]) * 16 + hex_(line_[5]);
      temp = hex_(line_[6]) * 16 + hex_(line_[7]);
      // FF denotes unavailable telemetry; reject impossible levels without refreshing state.
      if (level > 100 && level != 255) return;
    }
    if (seen_status_ && uint32_t(millis() - last_status_) >= communication_timeout_) seen_measurement_ = false;
    seen_status_ = true;
    last_status_ = millis();
    status_ = unavailable ? "me1" : line_.substr(1, 2);
    if (status_ == "me" || status_ == "me1") {
      seen_measurement_ = false;
      last_level_ = last_temperature_ = NAN;
    } else if (extended) {
      last_level_ = level == 255 ? NAN : float(level);
      last_temperature_ = temp == 255 ? NAN : temp / 10.0f;
      last_measurement_ = millis();
      seen_measurement_ = true;
    }
    // Atomic v1 packet: magic A, status nibble, then optional level/temperature bytes.
    const uint32_t code = extended ? (status_ == "mo" ? 5 : 6)
                                  : status_ == "mo" ? 1 : status_ == "mn" ? 2 : status_ == "me" ? 3 : 4;
    bridge_packet_ = 0xA00000U | (code << 16) | (extended ? uint32_t((level << 8) | temp) : 0U);
    publish_();
    // Publish the exact accepted payload before its sequence marker. Never synthesize raw frames on expiry.
    if (raw_message_sensor_) raw_message_sensor_->publish_state(line_.substr(0, line_.size() - 2));
    // ESPHome numeric sensors use float: wrap within its exact integer range so every frame changes state.
    message_counter_ = (message_counter_ + 1U) & 0xFFFFFFU;
    if (message_counter_sensor_) message_counter_sensor_->publish_state(float(message_counter_));
  }
  void publish_packet_() {
    if (!bridge_packet_sensor_) return;
    const bool online = seen_status_ && uint32_t(millis() - last_status_) < communication_timeout_;
    const float packet = float(online ? bridge_packet_ : 0xA00000U);
    // Packet transport provides periodic repeats. Only changes trigger immediate sends.
    if (!bridge_packet_sensor_->has_state() || bridge_packet_sensor_->state != packet)
      bridge_packet_sensor_->publish_state(packet);
  }
  void publish_() {
    const uint32_t now = millis();
    // Online means recognized serial traffic, even when the reported status is me1.
    const bool online = seen_status_ && uint32_t(now - last_status_) < communication_timeout_;
    if (!online) seen_measurement_ = false;
    const bool fresh = online && seen_measurement_ && uint32_t(now - last_measurement_) < measurement_timeout_;
    publish_packet_();
    if (online_sensor_) online_sensor_->publish_state(online);
    if (status_sensor_) status_sensor_->publish_state(online ? status_ : "unknown");
    if (level_) level_->publish_state(fresh ? last_level_ : NAN);
    if (temperature_) temperature_->publish_state(fresh ? last_temperature_ : NAN);
  }
  sensor::Sensor *level_{nullptr}, *temperature_{nullptr};
  text_sensor::TextSensor *status_sensor_{nullptr}, *raw_message_sensor_{nullptr};
  sensor::Sensor *message_counter_sensor_{nullptr};
  sensor::Sensor *bridge_packet_sensor_{nullptr};
  uint32_t bridge_packet_{0xA00000U};
  uint32_t message_counter_{0};
  binary_sensor::BinarySensor *online_sensor_{nullptr};
  std::string line_, status_{"unknown"}, probe_text_;
  uint32_t last_byte_{0}, last_status_{0}, last_measurement_{0};
  uint32_t communication_timeout_{10000}, measurement_timeout_{10000};
  uint32_t started_{0}, last_probe_{0}, probe_delay_{1000}, probe_interval_{2000};
  uint8_t probe_attempts_{0}, probe_max_attempts_{0};
  bool seen_status_{false}, seen_measurement_{false}, discarding_{false};
  float last_level_{NAN}, last_temperature_{NAN};
};
}  // namespace jura_coolcontrol
}  // namespace esphome
