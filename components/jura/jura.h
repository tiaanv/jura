#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <deque>

namespace esphome {
namespace jura {

class Jura : public PollingComponent, public uart::UARTDevice {
 public:
  void set_model(const std::string &m) { model_ = m; }

  void register_metric_sensor(const std::string &key, sensor::Sensor *s) { numeric_[key] = s; }
  void register_text_sensor(const std::string &key, text_sensor::TextSensor *t) { text_[key] = t; }

  void publish_number(const std::string &key, float value) {
    auto it = numeric_.find(key);
    if (it != numeric_.end() && it->second) it->second->publish_state(value);
  }
  void publish_text(const std::string &key, const std::string &value) {
    auto it = text_.find(key);
    if (it != text_.end() && it->second) it->second->publish_state(value);
  }

  // Returns queue acceptance, not a machine acknowledgement. No automatic retries.
  bool queue_command(const std::string &command) {
    if (command.empty() || command.size() > 32 || command.find_first_of("\r\n") != std::string::npos) {
      ESP_LOGW("jura", "Rejected invalid command");
      return false;
    }
    if (commands_.size() >= 8) {
      ESP_LOGW("jura", "Command queue full; rejected %s", command.c_str());
      return false;
    }
    commands_.push_back(command);
    return true;
  }

  void update() override {
    // Coalesce ticks while a poll is pending or in progress.
    if (poll_rt_pending_ || poll_ic_pending_ ||
        (state_ != State::IDLE && active_kind_ != Kind::ACTION)) return;
    poll_rt_pending_ = poll_ic_pending_ = true;
  }

  void loop() override {
    uint32_t now = millis();
    // Bound work per loop, including while draining stale/late input.
    uint8_t raw;
    for (size_t count = 0; count < 128 && available(); ++count) {
      if (!read_byte(&raw)) break;
      last_rx_ms_ = millis();
      if (state_ == State::IDLE) continue;
      if (response_complete_) continue;
      // The other six bits must remain high in the Jura encoding.
      if ((raw & 0xDBu) != 0xDBu) {
        finish_(false, "invalid UART encoding");
        return;
      }
      rx_byte_ |= static_cast<uint8_t>((((raw >> 2) & 1u) << rx_shift_) |
                                     (((raw >> 5) & 1u) << (rx_shift_ + 1)));
      rx_shift_ += 2;
      if (rx_shift_ == 8) {
        response_.push_back(static_cast<char>(rx_byte_));
        rx_shift_ = rx_byte_ = 0;
        if (response_.size() > 512) {
          finish_(false, "response too long");
          return;
        }
        const size_t n = response_.size();
        if (n >= 2 && response_[n - 2] == '\r' && response_[n - 1] == '\n') {
          response_.resize(n - 2);
          response_complete_ = true;
        }
      }
    }
    now = millis();
    if (state_ == State::IDLE) {
      if (available() || static_cast<uint32_t>(now - last_rx_ms_) < 100 ||
          static_cast<uint32_t>(now - finished_ms_) < recovery_ms_) return;
      if (!commands_.empty()) {
        std::string command = commands_.front();
        commands_.pop_front();
        start_(command, Kind::ACTION, now);
      } else if (poll_rt_pending_) {
        poll_rt_pending_ = false;
        start_("RT:0000", Kind::COUNTERS, now);
      } else if (poll_ic_pending_) {
        poll_ic_pending_ = false;
        start_("IC:", Kind::STATUS, now);
      }
      return;
    }
    if (state_ == State::SENDING) {
      if (static_cast<uint32_t>(now - started_ms_) >= 5000) {
        finish_(false, "transmit scheduling timeout");
        return;
      }
      if (tx_pos_ != 0 && static_cast<uint32_t>(now - last_tx_ms_) < 8) return;
      if (tx_pos_ < tx_.size()) {
        const uint8_t value = static_cast<uint8_t>(tx_[tx_pos_++]);
        uint8_t encoded[4];
        for (unsigned i = 0; i < 4; ++i)
          encoded[i] = static_cast<uint8_t>(0xDBu | (((value >> (2 * i)) & 1u) << 2) |
                                          (((value >> (2 * i + 1)) & 1u) << 5));
        // Only four bytes per call; never wait for the full frame to flush.
        write_array(encoded, sizeof(encoded));
        last_tx_ms_ = millis();
        return;
      }
      state_ = State::WAITING;
      response_started_ms_ = now;
    }
    if (response_complete_) {
      finish_(true, "");
    } else if (static_cast<uint32_t>(now - response_started_ms_) >= 5000) {
      finish_(false, "response timeout");
    }
  }

 protected:
  enum class State { IDLE, SENDING, WAITING };
  enum class Kind { ACTION, COUNTERS, STATUS };

  void start_(const std::string &command, Kind kind, uint32_t now) {
    active_command_ = command;
    active_kind_ = kind;
    tx_ = command + "\r\n";
    tx_pos_ = 0;
    response_.clear();
    response_complete_ = false;
    rx_shift_ = rx_byte_ = 0;
    started_ms_ = now;
    state_ = State::SENDING;
    ESP_LOGD("jura", "Sending %s", command.c_str());
  }

  static bool valid_hex_payload_(const std::string &value, const char *prefix, size_t min_length) {
    if (value.size() < min_length || value.compare(0, 3, prefix) != 0) return false;
    for (size_t i = 3; i < value.size(); ++i) {
      const char c = value[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
        return false;
    }
    return true;
  }

  void finish_(bool success, const char *reason) {
    if (success && active_kind_ == Kind::COUNTERS)
      success = valid_hex_payload_(response_, "rt:", 64) && ((response_.size() - 3) % 4 == 0);
    if (success && active_kind_ == Kind::STATUS)
      success = valid_hex_payload_(response_, "ic:", 7);
    if (!success) {
      ESP_LOGW("jura", "%s: %s (%u decoded bytes); not retried", active_command_.c_str(),
               reason[0] ? reason : "unexpected response", static_cast<unsigned>(response_.size()));
    } else if (active_kind_ == Kind::COUNTERS) {
      handle_counters_(response_);
    } else if (active_kind_ == Kind::STATUS) {
      handle_status_(response_);
    } else {
      ESP_LOGD("jura", "%s response: %s", active_command_.c_str(), response_.c_str());
    }
    state_ = State::IDLE;
    finished_ms_ = millis();
    // A quiet interval helps discard late replies after timeout; the protocol has no transaction IDs.
    recovery_ms_ = success ? 100 : 500;
  }

  void handle_counters_(const std::string &result) {
    std::vector<long> current = parse_all_counters_(result);
    // Publish the ones you already expose (examples below; keep as you had it)
    publish_number("counter_1", get_counter_n_(current, 1));
    publish_number("counter_2", get_counter_n_(current, 2));
    publish_number("counter_3", get_counter_n_(current, 3));
    publish_number("counter_4", get_counter_n_(current, 4));
    publish_number("counter_5", get_counter_n_(current, 5));
    publish_number("counter_6", get_counter_n_(current, 6));   
    publish_number("counter_7", get_counter_n_(current, 7));   
    publish_number("counter_8", get_counter_n_(current, 8));
    publish_number("counter_9", get_counter_n_(current, 9));
    publish_number("counter_10", get_counter_n_(current, 10));
    publish_number("counter_11", get_counter_n_(current, 11));
    publish_number("counter_12", get_counter_n_(current, 12));
    publish_number("counter_13", get_counter_n_(current, 13));
    publish_number("counter_14", get_counter_n_(current, 14));
    publish_number("counter_15", get_counter_n_(current, 15));
    publish_number("counter_16", get_counter_n_(current, 16));

    publish_counter_changes_(current);

  }

  void handle_status_(const std::string &ic) {
      uint8_t a = static_cast<uint8_t>(std::strtol(ic.substr(3,2).c_str(), nullptr, 16));
      uint8_t b = static_cast<uint8_t>(std::strtol(ic.substr(5,2).c_str(), nullptr, 16));

      publish_ic_bits_if_changed_(a, b);     

      uint8_t trayBit       = ((a >> 4) & 1u);
      uint8_t left_readyBit = ((a >> 2) & 1u);
      uint8_t tankBit       = ((b >> 5) & 1u);
      uint8_t right_busyBit = ((b >> 6) & 1u);

      std::string tray_status = (trayBit == 1) ? "Present" : "Missing";
      std::string tank_status = (tankBit == 1) ? "Fill Tank" : "OK";
      std::string machine_status = "Ready";
      if (trayBit == 0)       machine_status = "Tray Missing";
      if (tankBit == 1)       machine_status = "Fill Tank";
      if (right_busyBit == 1) machine_status = "Busy (Milk Drink)";
      if (left_readyBit == 0) machine_status = "Busy (Coffee Drink)";

      publish_text("tray_status",        tray_status);
      publish_text("water_tank_status",  tank_status);
      publish_text("machine_status",     machine_status);
  }

  // Return counter_n (1-based) from vector or -1 if missing
  long get_counter_n_(const std::vector<long> &v, int n) const {
    const size_t idx = (n >= 1) ? (size_t)(n - 1) : (size_t) -1;
    if (idx < v.size()) return v[idx];
    return -1;
  }

  // Parse all 4-hex counters from RT payload starting at pos=3
  std::vector<long> parse_all_counters_(const std::string &rt) const {
    std::vector<long> out;
    // field i starts at pos = 3 + 4*i (i = 0..)
    for (size_t pos = 3; pos + 4 <= rt.size(); pos += 4) {
      long val = std::strtol(rt.substr(pos, 4).c_str(), nullptr, 16);
      out.push_back(val);
    }
    return out;
  }

  void publish_counter_changes_(const std::vector<long> &current) {
    if (!last_counters_initialized_) {
      last_counters_ = current;
      last_counters_initialized_ = true;
      return;
    }
  
    std::string msg;
    bool any = false;
    const size_t max_n = std::max(last_counters_.size(), current.size());
    char buf[48];
  
    for (size_t i = 0; i < max_n; ++i) {
      long prev = (i < last_counters_.size()) ? last_counters_[i] : -1;
      long now  = (i < current.size())       ? current[i]          : -1;
      if (prev != now) {
        if (any) msg += ", ";
        // "counter_%zu %ld→%ld"
        std::snprintf(buf, sizeof(buf), "counter_%u %ld\u2192%ld", (unsigned)(i + 1), prev, now);
        msg += buf;
        any = true;
      }
    }
  
    if (any) {
      publish_text("counters_changed", msg);
      ESP_LOGD("jura", "Changed: %s", msg.c_str());
    }
  
    last_counters_ = current;
  }

  static inline void byte_to_bits(uint8_t v, char out[9]) {
    for (int i = 7; i >= 0; --i) out[7 - i] = (v & (1u << i)) ? '1' : '0';
    out[8] = '\0';
  }

  void publish_ic_bits_if_changed_(uint8_t a, uint8_t b) {
    if (!ic_bits_initialized_ || a != last_ic_a_ || b != last_ic_b_) {
      char abits[9], bbits[9], buf[32];
      byte_to_bits(a, abits);
      byte_to_bits(b, bbits);
      // "A=xxxxxxxx B=xxxxxxxx"
      std::snprintf(buf, sizeof(buf), "A=%s B=%s", abits, bbits);
      publish_text("ic_bits", std::string(buf));
  
      last_ic_a_ = a;
      last_ic_b_ = b;
      ic_bits_initialized_ = true;
  
      ESP_LOGD("jura", "IC bits changed: %s", buf);
    }
  }

  State state_{State::IDLE};
  Kind active_kind_{Kind::ACTION};
  std::deque<std::string> commands_;
  bool poll_rt_pending_{false}, poll_ic_pending_{false};
  std::string active_command_, tx_, response_;
  size_t tx_pos_{0};
  uint8_t rx_byte_{0}, rx_shift_{0};
  bool response_complete_{false};
  uint32_t last_rx_ms_{0}, finished_ms_{0}, recovery_ms_{100};
  uint32_t started_ms_{0}, last_tx_ms_{0}, response_started_ms_{0};

  std::string model_{"UNKNOWN"};

  std::map<std::string, sensor::Sensor*>            numeric_;
  std::map<std::string, text_sensor::TextSensor*>   text_;

  std::vector<long> last_counters_;
  bool last_counters_initialized_{false};

  uint8_t last_ic_a_{0};
  uint8_t last_ic_b_{0};
  bool ic_bits_initialized_{false};
};

}  // namespace jura
}  // namespace esphome
