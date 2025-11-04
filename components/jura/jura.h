#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/log.h"
#include <map>

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

  void update() override {
    // ---- counters ----
    std::string result = cmd2jura("RT:0000");
    if (result.empty() || result.size() < 64) {
      ESP_LOGW("jura", "Unexpected RT:0000 response len=%d", (int)result.size());
      return;
    }

   // Parse all counters available (4 hex chars per field starting at pos=3)
    std::vector<long> current = parse_all_counters_(result);

    // Publish the ones you already expose (examples below; keep as you had it)
    publish_number("counter_1", get_counter_n_(current, 1));
    publish_number("counter_2", get_counter_n_(current, 2));
    publish_number("counter_3", get_counter_n_(current, 3));
    publish_number("counter_4", get_counter_n_(current, 4));
    publish_number("counter_8", get_counter_n_(current, 8));
    publish_number("counter_9", get_counter_n_(current, 9));
    publish_number("counter_15", get_counter_n_(current, 15));

    publish_counter_changes_(current);

    // ---- flags ----
    std::string ic = cmd2jura("IC:");
    if (ic.size() >= 7) {
      byte a = static_cast<byte>(strtol(ic.substr(3,2).c_str(), NULL, 16));
      byte b = static_cast<byte>(strtol(ic.substr(5,2).c_str(), NULL, 16));

      byte trayBit       = bitRead(a, 4);
      byte left_readyBit = bitRead(a, 2);
      byte tankBit       = bitRead(b, 5);
      byte right_busyBit = bitRead(b, 6);

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
    } else {
      ESP_LOGW("jura", "Unexpected IC response len=%d", (int) ic.size());
    }
  }

 protected:
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
      long val = strtol(rt.substr(pos, 4).c_str(), nullptr, 16);
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
        snprintf(buf, sizeof(buf), "counter_%u %ld\u2192%ld", (unsigned)(i + 1), prev, now);
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

  std::string cmd2jura(std::string outbytes) {
    std::string inbytes;
    int w = 0;

    while (available()) { read(); }
    outbytes += "\r\n";
    for (int i = 0; i < (int) outbytes.size(); i++) {
      uint8_t src = static_cast<uint8_t>(outbytes[i]);
      for (int s = 0; s < 8; s += 2) {
        uint8_t rawbyte = 0xFF;
        bitWrite(rawbyte, 2, bitRead(src, s + 0));
        bitWrite(rawbyte, 5, bitRead(src, s + 1));
        write(rawbyte);
      }
      delay(8);
    }

    int s = 0;
    uint8_t inbyte = 0;
    while (!(inbytes.size() >= 2 && inbytes[inbytes.size()-2] == '\r' && inbytes[inbytes.size()-1] == '\n')) {
      if (available()) {
        uint8_t rawbyte = static_cast<uint8_t>(read());
        bitWrite(inbyte, s + 0, bitRead(rawbyte, 2));
        bitWrite(inbyte, s + 1, bitRead(rawbyte, 5));
        if ((s += 2) >= 8) {
          s = 0;
          inbytes.push_back(static_cast<char>(inbyte));
          inbyte = 0;
        }
      } else {
        delay(10);
      }
      if (w++ > 500) return "";
    }
    return inbytes.substr(0, inbytes.size() - 2);
  }

  std::string model_{"UNKNOWN"};

  std::map<std::string, sensor::Sensor*>            numeric_;
  std::map<std::string, text_sensor::TextSensor*>   text_;

  std::vector<long> last_counters_;
  bool last_counters_initialized_{false};
};

}  // namespace jura
}  // namespace esphome
