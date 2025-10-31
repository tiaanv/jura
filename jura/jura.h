#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace jura {

class Jura : public PollingComponent, public uart::UARTDevice {

 public:
  void set_single_espresso_made_sensor(sensor::Sensor *s) { this->single_espresso_made_sensor_ = s; }
  void set_double_espresso_made_sensor(sensor::Sensor *s) { this->double_espresso_made_sensor_ = s; }
  void set_coffee_made_sensor(sensor::Sensor *s) { this->coffee_made_sensor_ = s; }
  void set_double_coffee_made_sensor(sensor::Sensor *s) { this->double_coffee_made_sensor_ = s; }
  void set_cleanings_performed_sensor(sensor::Sensor *s) { this->cleanings_performed_sensor_ = s; }

  void set_tray_status_sensor(text_sensor::TextSensor *s) { this->tray_status_sensor_ = s; }
  void set_water_tank_status_sensor(text_sensor::TextSensor *s) { this->water_tank_status_sensor_ = s; }
  void set_machine_status_sensor(text_sensor::TextSensor *s) { this->machine_status_sensor_ = s; }
  long num_single_espresso, num_double_espresso, num_coffee, num_double_coffee, num_clean;
  std::string tray_status, tank_status, machine_status;  

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
    std::string result, hexString_a, hexString_b, substring;
    byte hex_to_byte_a, hex_to_byte_b;
    byte trayBit, tankBit, right_busyBit, left_readyBit;
    // For Testing
    byte read_bit0,read_bit1,read_bit2,read_bit3,read_bit4,read_bit5,read_bit6,read_bit7;

    // Fetch data
    result = cmd2jura("RT:0000");

    // Get Single Espressos made
    substring = result.substr(3,4);
    num_single_espresso = strtol(substring.c_str(),NULL,16);

    // Double Espressos made
    substring = result.substr(7,4);
    num_double_espresso = strtol(substring.c_str(),NULL,16);

    // Coffees made
    substring = result.substr(11,4);
    num_coffee = strtol(substring.c_str(),NULL,16);

    // Double Coffees made
    substring = result.substr(15,4);
    num_double_coffee = strtol(substring.c_str(),NULL,16);

    // Cleanings done
    substring = result.substr(35,4);
    num_clean = strtol(substring.c_str(),NULL,16);

    // Tray & water tank status
    result = cmd2jura("IC:");

    

    //----------- DECODE FLAGS -------------
    hexString_a = result.substr(3,2);
    hexString_b = result.substr(5,2);
    hex_to_byte_a = static_cast<byte>(strtol(hexString_a.c_str(), NULL, 16));
    hex_to_byte_b = static_cast<byte>(strtol(hexString_b.c_str(), NULL, 16));
    trayBit = bitRead(hex_to_byte_a, 4);
    tankBit = bitRead(hex_to_byte_b, 5);
    right_busyBit = bitRead(hex_to_byte_b, 6);
    left_readyBit = bitRead(hex_to_byte_a, 2);
    if (trayBit == 1) { tray_status = "Present"; } else { tray_status = "Missing"; }
    if (tankBit == 1) { tank_status = "Fill Tank"; } else { tank_status = "OK"; }

    //----------- TRACE/DEBUG BIT FLAGS -------------
    // For Testing/ decoding your own machine's bit flags... Takes hours ;)
    ESP_LOGD("main", "Raw IC result: %s", result.c_str());
    ESP_LOGD("main", "A: %s", hexString_a.c_str());
    read_bit0 = bitRead(hex_to_byte_a, 0);
    read_bit1 = bitRead(hex_to_byte_a, 1);
    read_bit2 = bitRead(hex_to_byte_a, 2);
    read_bit3 = bitRead(hex_to_byte_a, 3);
    read_bit4 = bitRead(hex_to_byte_a, 4);
    read_bit5 = bitRead(hex_to_byte_a, 5);
    read_bit6 = bitRead(hex_to_byte_a, 6);
    read_bit7 = bitRead(hex_to_byte_a, 7);
    ESP_LOGD("main", "A Bits: %d%d%d%d%d%d%d%d", read_bit7,read_bit6,read_bit5,read_bit4,read_bit3,read_bit2,read_bit1,read_bit0);
    ESP_LOGD("main", "B: %s", hexString_b.c_str());
    read_bit0 = bitRead(hex_to_byte_b, 0);
    read_bit1 = bitRead(hex_to_byte_b, 1);
    read_bit2 = bitRead(hex_to_byte_b, 2);
    read_bit3 = bitRead(hex_to_byte_b, 3);
    read_bit4 = bitRead(hex_to_byte_b, 4);
    read_bit5 = bitRead(hex_to_byte_b, 5);
    read_bit6 = bitRead(hex_to_byte_b, 6);
    read_bit7 = bitRead(hex_to_byte_b, 7);
    ESP_LOGD("main", "B Bits: %d%d%d%d%d%d%d%d", read_bit7,read_bit6,read_bit5,read_bit4,read_bit3,read_bit2,read_bit1,read_bit0);


    //----------- UPDATE SENSORS -------------
    if (single_espresso_made_sensor_ != nullptr)   single_espresso_made_sensor_->publish_state(num_single_espresso);
    if (double_espresso_made_sensor_ != nullptr)   double_espresso_made_sensor_->publish_state(num_double_espresso);
    if (coffee_made_sensor_ != nullptr)   coffee_made_sensor_->publish_state(num_coffee);
    if (double_coffee_made_sensor_ != nullptr)   double_coffee_made_sensor_->publish_state(num_double_coffee);
    if (cleanings_performed_sensor_ != nullptr)   cleanings_performed_sensor_->publish_state(num_clean);

    if (tray_status_sensor_ != nullptr)   tray_status_sensor_->publish_state(tray_status);
    if (water_tank_status_sensor_ != nullptr)   water_tank_status_sensor_->publish_state(tank_status);

    machine_status = "Ready";
    if (trayBit == 0)       machine_status = "Tray Missing";
    if (tankBit == 1)       machine_status = "Fill Tank";
    if (right_busyBit == 1) machine_status = "Busy (Milk Drink)";
    if (left_readyBit == 0) machine_status = "Busy (Coffee Drink)";

    if (machine_status_sensor_ != nullptr)   machine_status_sensor_->publish_state(machine_status);
  }
  protected:    
   sensor::Sensor *single_espresso_made_sensor_{nullptr};
   sensor::Sensor *double_espresso_made_sensor_{nullptr};
   sensor::Sensor *coffee_made_sensor_{nullptr};
   sensor::Sensor *double_coffee_made_sensor_{nullptr};
   sensor::Sensor *cleanings_performed_sensor_{nullptr};

   text_sensor::TextSensor *tray_status_sensor_{nullptr};
   text_sensor::TextSensor *water_tank_status_sensor_{nullptr};
   text_sensor::TextSensor *machine_status_sensor_{nullptr};
};

}  // namespace jura
}  // namespace esphome
