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

  void update() override {
    // ---- counters ----
    std::string result = cmd2jura("RT:0000");
    if (result.empty() || result.size() < 64) {
      ESP_LOGW("jura", "Unexpected RT:0000 response len=%d", (int)result.size());
      return;
    }

    long num_single_espresso = strtol(result.substr( 3,4).c_str(), NULL, 16);
    long num_double_espresso = strtol(result.substr( 7,4).c_str(), NULL, 16);
    long num_coffee          = strtol(result.substr(11,4).c_str(), NULL, 16);
    long num_double_coffee   = strtol(result.substr(15,4).c_str(), NULL, 16);
    long num_brews           = strtol(result.substr(31,4).c_str(), NULL, 16);
    long num_clean           = strtol(result.substr(35,4).c_str(), NULL, 16);

    uint16_t used = strtol(result.substr(59,4).c_str(), NULL, 16);
    uint16_t cap  = grounds_capacity_;
    long num_grounds_remaining = (used >= cap) ? 0 : (cap - used);

    publish_number("counter_1",        num_single_espresso);
    publish_number("counter_2",        num_double_espresso);
    publish_number("counter_3",        num_coffee);
    publish_number("counter_4",        num_double_coffee);
    publish_number("brews",            num_brews);
    publish_number("cleanings",        num_clean);
    publish_number("grounds_remaining",num_grounds_remaining);

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
