#include <cassert>
#include <iostream>

#include "../components/jura_coolcontrol/jura_coolcontrol.h"
#include "../components/jura_native/jura_native.h"
using namespace esphome::jura_native;
static std::vector<uint8_t> encode(const std::string &s) {
  std::vector<uint8_t> out;
  for (unsigned char c : s) {
    for (unsigned n = 0; n < 8; n += 2)
      out.push_back(uint8_t(0xDB | (((c >> n) & 1) << 2) | (((c >> (n + 1)) & 1) << 5)));
  }
  return out;
}
static void ticks(JuraNative &device, unsigned n) {
  for (unsigned i = 0; i < n; ++i) {
    ++test_now;
    device.loop();
  }
}
int main() {
  test_now = 0;
  JuraNative device;
  RecipeSelect drink, strength, temp, grinder;
  RecipeNumber water, milk;
  ProfileSensor espresso, unrelated, grounds;
  ProfileBinarySensor tray, unrelated_alert;
  CommandButton brew, off;
  esphome::sensor::Sensor water_min;
  device.register_select(-1, &drink);
  device.register_select(0, &strength);
  device.register_select(1, &grinder);
  device.register_select(2, &temp);
  device.register_number(3, &water);
  device.register_number(4, &milk);
  device.register_profile_sensor("product_2", &espresso);
  device.register_profile_sensor("product_55", &unrelated);
  device.register_profile_sensor("grounds_level", &grounds);
  device.register_sensor("water_min", &water_min);
  device.register_alert(0, &tray);
  device.register_alert(55, &unrelated_alert);
  device.register_button(0, &brew);
  device.register_button(3, &off);
  device.set_allow_brew(true);
  device.set_allow_power_off(true);
  device.setup();
  assert(!device.can_proceed());
  ticks(device, 100);
  for (auto raw : encode("ty:EF533M V02.09\r\n")) device.incoming.push_back(raw);
  ticks(device, 2);
  assert(device.can_proceed());
  assert(espresso.name() == "Espresso Count" && !espresso.is_internal());
  assert(unrelated.is_internal() && unrelated_alert.is_internal());
  assert(tray.name() == "insert tray" && !tray.is_internal());
  assert(grounds.name() == "Grounds Level" && !grounds.is_internal());
  assert(drink.state == "Espresso" && strength.state == "6" && temp.state == "High");
  assert(grinder.is_internal());
  assert(water.state == 45 && water_min.state == 15 && water.traits.step == 5);
  device.select_value(-1, "Cappuccino");
  assert(drink.state == "Cappuccino" && milk.state == 14);
  device.select_value(0, "8");
  assert(strength.state == "8");
  device.number_value(3, 52);
  assert(water.state == 60);  // invalid step doesn't alter the recipe
  device.number_value(3, 80);
  assert(water.state == 80);
  assert(!brew.is_internal() && !off.is_internal());

  JuraNative unavailable;
  RecipeSelect missing;
  RecipeNumber hidden;
  unavailable.register_select(-1, &missing);
  unavailable.register_number(3, &hidden);
  unavailable.setup();
  ticks(unavailable, 8001);
  assert(!unavailable.can_proceed());
  ticks(unavailable, 30000);
  assert(unavailable.can_proceed() && missing.is_internal() && hidden.is_internal());

  esphome::jura_coolcontrol::JuraCoolcontrol cooler;
  esphome::sensor::Sensor level, temperature;
  cooler.set_level_sensor(&level);
  cooler.set_temperature_sensor(&temperature);
  const std::string frame = "abcd321E\r\n";
  for (unsigned char c : frame) cooler.incoming.push_back(c);
  cooler.loop();
  cooler.update();
  assert(level.state == 50 && temperature.state == 3);
  test_now += 10001;
  cooler.update();
  assert(std::isnan(level.state) && std::isnan(temperature.state));
  std::cout << "PASS: boot discovery, native names/visibility/choices, per-drink limits, validated controls "
               "and nonblocking Cool Control\n";
}
