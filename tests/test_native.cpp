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
static void inject_at(JuraNative &device, const std::string &frame) {
  for (auto raw : encode(jura_protocol::atproto::build_frame(frame + "\r\n", 0xA0)))
    device.incoming.push_back(raw);
  ticks(device, 10);
}
struct CoolerLogProbe : public JuraNative {
  void log_inputs(uint32_t now) { log_coolcontrol_inputs_(now); }
  float logged_counter() const { return cooler_logged_counter_; }
  uint8_t availability() const { return cooler_logged_availability_; }
  bool logged_online() const { return cooler_logged_online_; }
  uint32_t logged_at() const { return cooler_inputs_logged_at_; }
};
int main() {
  CoolerLogProbe logging;
  esphome::text_sensor::TextSensor imported_raw;
  esphome::sensor::Sensor imported_counter;
  esphome::binary_sensor::BinarySensor imported_online;
  logging.set_coolcontrol(&imported_raw, &imported_counter, &imported_online, 15000, 250);
  CoolcontrolOverrideSwitch override;
  logging.set_coolcontrol_override_switch(&override);
  logging.setup();
  assert(!override.state && !logging.coolcontrol_override());
  override.turn_on(); assert(override.state && logging.coolcontrol_override());
  override.turn_off(); assert(!override.state && !logging.coolcontrol_override());
  override.turn_on(); logging.setup();
  assert(!override.state && !logging.coolcontrol_override());
  logging.log_inputs(0); assert(logging.availability() == 0);
  imported_raw.publish_state("@mo:321E"); imported_counter.publish_state(1); imported_online.publish_state(true);
  logging.log_inputs(1); assert(logging.availability() == 7 && logging.logged_counter() == 1);
  imported_counter.publish_state(2); logging.log_inputs(1000); assert(logging.logged_counter() == 1);
  logging.log_inputs(5001); assert(logging.logged_counter() == 2);
  imported_online.publish_state(false); logging.log_inputs(5002); assert(!logging.logged_online());
  imported_online.invalidate_state(); logging.log_inputs(5003); assert(logging.availability() == 3);
  logging.log_inputs(20002); assert(logging.logged_at() == 5003);
  logging.log_inputs(20003); assert(logging.logged_at() == 20003 && logging.availability() == 3);

  test_now = 0;
  JuraNative device;
  RecipeSelect drink, strength, temp, grinder;
  RecipeNumber water, milk;
  ProfileSensor espresso, unrelated, grounds;
  ProfileBinarySensor tray, unrelated_alert;
  CommandButton brew, off, probe;
  esphome::text_sensor::TextSensor probe_result;
  esphome::text_sensor::TextSensor recipe_source;
  esphome::binary_sensor::BinarySensor recipe_loading, recipe_modified;
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
  device.register_button(4, &probe);
  device.register_text("espresso_probe_result", &probe_result);
  device.register_text("recipe_source", &recipe_source);
  device.register_binary("recipe_loading", &recipe_loading);
  device.register_binary("recipe_modified", &recipe_modified);
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
  assert(!probe.is_internal());
  device.press(4);  // Identification alone is not an AT connection.
  assert(probe_result.state.find("Rejected") == 0);
  assert(recipe_loading.state && recipe_modified.state);
  // Complete the real adapter's handshake and load the selected Cappuccino.
  ticks(device, 8300 - test_now);
  inject_at(device, "@t0");
  ticks(device, 400);
  inject_at(device, "@t1");
  inject_at(device, "@T2:010001B228");
  ticks(device, 1000);
  inject_at(device, "@T3:3BA4EF533M V02.09");
  ticks(device, 500);
  inject_at(device, "@TF:000400000000");
  ticks(device, 800);
  inject_at(device, "@tm:41,0400060C0E00010000000000BB");
  assert(recipe_source.state == "Machine" && !recipe_loading.state);
  assert(water.state == 80 && recipe_modified.state);  // Preserve the earlier HA override.
  device.press(0);
  assert(water.state == 60 && !recipe_modified.state);  // Consume it for exactly one request.

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
  const std::string frame = "@mo:321E\r\n";
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
