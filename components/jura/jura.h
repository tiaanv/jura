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
  void set_model(const std::string &m) { this->model_ = m; }
  void set_grounds_capacity(uint16_t v) { this->grounds_capacity_ = v; }
  void set_grounds_remaining_capacity_sensor(sensor::Sensor *s) { this->grounds_remaining_capacity_sensor_ = s; }

  void register_metric_sensor(const std::string &key, sensor::Sensor *s) {
    this->numeric_[key] = s;
    ESP_LOGD(TAG, "Registered metric sensor key='%s'", key.c_str());
  }
  void register_text_sensor(const std::string &key, text_sensor::TextSensor *t) {
    this->text_[key] = t;
    ESP_LOGD(TAG, "Registered text sensor key='%s'", key.c_str());
  }

  // ---------------------------
  // Helpers for parser/publish
  // ---------------------------
  void publish_number(const std::string &key, float value) {
    auto it = numeric_.find(key);
    if (it != numeric_.end() && it->second) {
      it->second->publish_state(value);
    } else {
      ESP_LOGV(TAG, "Numeric key '%s' not registered for model '%s'", key.c_str(), model_.c_str());
    }
  }
  void publish_text(const std::string &key, const std::string &value) {
    auto it = text_.find(key);
    if (it != text_.end() && it->second) {
      it->second->publish_state(value);
    } else {
      ESP_LOGV(TAG, "Text key '%s' not registered for model '%s'", key.c_str(), model_.c_str());
    }
  }

// Sends 'out' plus CRLF using Jura's bit placement, then reads until CRLF or timeout.
  std::string cmd2jura(std::string outbytes) {
    std::string inbytes;
    int w = 0;

    while (available()) {
      read();
    }

    outbytes += "\r\n";
    for (int i = 0; i < outbytes.size(); i++) {
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
    while (!(inbytes.size() >= 2 &&
           inbytes[inbytes.size() - 2] == '\r' &&
           inbytes[inbytes.size() - 1] == '\n')) {
      if (available()) {
        uint8_t rawbyte = static_cast<uint8_t>(read());
        bitWrite(inbyte, s + 0, bitRead(rawbyte, 2));
        bitWrite(inbyte, s + 1, bitRead(rawbyte, 5));
        if ((s += 2) >= 8) {
          s = 0;
          inbytes.push_back(static_cast<char>(inbyte));
          inbyte = 0;        }
      } else {
        delay(10);
      }
      if (w++ > 500) {
        return "";
      }
    }

    return inbytes.substr(0, inbytes.size() - 2);
  }

  //loop() and setup() are not used

  void update() override {
    // --- Read counters/status from RT:0000 ---
    std::string result = cmd2jura("RT:0000");
    if (result.size() < 64) {
      ESP_LOGW(TAG, "RT:0000 unexpected length (%u).", (unsigned)result.size());
      return;
    }

    auto hx = [&](size_t off, size_t len) -> uint32_t {
      if (off + len > result.size()) return 0;
      std::string sub = result.substr(off, len);
      return static_cast<uint32_t>(strtol(sub.c_str(), nullptr, 16));
    };

    // Note: these offsets are your current mapping; keep if they’re valid for the model
    long num_single_espresso = hx(3, 4);
    long num_double_espresso = hx(7, 4);
    long num_coffee         = hx(11, 4);
    long num_double_coffee  = hx(15, 4);
    long num_brews          = hx(31, 4);  // Brew unit movements (TODO confirm for each model)
    long num_clean          = hx(35, 4);

    uint16_t used = static_cast<uint16_t>(hx(59, 4));
    uint16_t cap  = this->grounds_capacity_;
    long num_grounds_remaining = (used >= cap) ? 0 : (cap - used);

    // Publish via stable keys; Python decides which keys exist for the selected model
    publish_number("counter_1", num_single_espresso);
    publish_number("counter_2", num_double_espresso);
    publish_number("counter_3", num_coffee);
    publish_number("counter_4", num_double_coffee);
    publish_number("brews",     num_brews);
    publish_number("cleanings", num_clean);
    publish_number("grounds_remaining", num_grounds_remaining);

    // --- Read flags/status from IC: ---
    std::string ic = cmd2jura("IC:");
    if (ic.size() < 7) {
      ESP_LOGW(TAG, "IC: unexpected length (%u).", (unsigned)ic.size());
      return;
    }

    auto hx_byte = [&](size_t off) -> uint8_t {
      std::string b = ic.substr(off, 2);
      return static_cast<uint8_t>(strtol(b.c_str(), nullptr, 16));
    };

    uint8_t A = hx_byte(3);
    uint8_t B = hx_byte(5);

    uint8_t trayBit       = bitRead(A, 4);
    uint8_t left_readyBit = bitRead(A, 2);
    uint8_t tankBit       = bitRead(B, 5);
    uint8_t right_busyBit = bitRead(B, 6);

    std::string tray_status  = (trayBit == 1) ? "Present" : "Missing";
    std::string tank_status  = (tankBit == 1) ? "Fill Tank" : "OK";

    // Machine status synthesis
    std::string machine_status = "Ready";
    if (trayBit == 0)       machine_status = "Tray Missing";
    if (tankBit == 1)       machine_status = "Fill Tank";
    if (right_busyBit == 1) machine_status = "Busy (Milk Drink)";
    if (left_readyBit == 0) machine_status = "Busy (Coffee Drink)";

    // Publish text keys (only appear if model spec registered them)
    publish_text("tray_status", tray_status);
    publish_text("water_tank_status", tank_status);
    publish_text("machine_status", machine_status);

    // Debug traces (keep, super helpful while reverse-engineering)
    ESP_LOGD(TAG, "Raw IC: %s", ic.c_str());
    ESP_LOGD(TAG, "A=0x%02X B=0x%02X  (tray=%u, tank=%u, right_busy=%u, left_ready=%u)",
             A, B, trayBit, tankBit, right_busyBit, left_readyBit);
  }
 protected:
  std::string model_{"UNKNOWN"};
  uint16_t grounds_capacity_{12};

  std::map<std::string, sensor::Sensor*> numeric_;
  std::map<std::string, text_sensor::TextSensor*> text_;
};

}  // namespace jura
}  // namespace esphome
