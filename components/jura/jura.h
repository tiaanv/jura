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
  void set_grounds_capacity(uint16_t v) { grounds_capacity_ = v; }

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

  // helper: get counter_n from RT string
  static inline long get_counter_n(const std::string &rt, int n) {
    // position = 3 + 4*(n-1)
    const size_t pos = 3u + 4u * (size_t)(n - 1);
    if (rt.size() < pos + 4u) return 0;
    return strtol(rt.substr(pos, 4).c_str(), nullptr, 16);
  }

  void update() override {
    // ---- counters ----
    std::string result = cmd2jura("RT:0000");
    if (result.empty() || result.size() < 64) {
      ESP_LOGW("jura", "Unexpected RT:0000 response len=%d", (int)result.size());
      return;
    }

    // Parse by index
    const long counter_1  = get_counter_n(rt, 1);
    const long counter_2  = get_counter_n(rt, 2);
    const long counter_3  = get_counter_n(rt, 3);
    const long counter_4  = get_counter_n(rt, 4);
    const long counter_8  = get_counter_n(rt, 8);  // was 'brews'
    const long counter_9  = get_counter_n(rt, 9);  // was 'cleanings'
    const long used_raw   = get_counter_n(rt, 15); // raw "used" count for grounds
  
    // Derive grounds_remaining from counter_15 and capacity
    const uint16_t cap = grounds_capacity_;
    const uint16_t used = (used_raw < 0) ? 0 : (uint16_t) used_raw;
    const long grounds_remaining = (used >= cap) ? 0 : (cap - used);
  
    // Publish by generic keys only
    publish_number("counter_1",         counter_1);
    publish_number("counter_2",         counter_2);
    publish_number("counter_3",         counter_3);
    publish_number("counter_4",         counter_4);
    publish_number("counter_8",         counter_8);
    publish_number("counter_9",         counter_9);
    publish_number("grounds_remaining", grounds_remaining);

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
  uint16_t grounds_capacity_{12};

  std::map<std::string, sensor::Sensor*>            numeric_;
  std::map<std::string, text_sensor::TextSensor*>   text_;
};

}  // namespace jura
}  // namespace esphome
