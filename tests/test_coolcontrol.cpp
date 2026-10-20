#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include "../components/jura_coolcontrol/jura_coolcontrol.h"
using esphome::jura_coolcontrol::JuraCoolcontrol;
struct Fixture {
  JuraCoolcontrol cooler;
  esphome::sensor::Sensor level, temperature;
  esphome::text_sensor::TextSensor status;
  esphome::binary_sensor::BinarySensor online;
  Fixture() {
    test_now = 0;
    cooler.set_level_sensor(&level);
    cooler.set_temperature_sensor(&temperature);
    cooler.set_status_sensor(&status);
    cooler.set_online_sensor(&online);
    cooler.setup();
  }
  void feed(const std::string &s) {
    for (unsigned char c : s) cooler.incoming.push_back(c);
    do { cooler.loop(); } while (cooler.available());
  }
};
int main() {
  Fixture f;
  assert(!f.online.state && f.status.state == "unknown" && std::isnan(f.level.state));
  f.feed("@mo:321E\r\n");
  assert(f.online.state && f.status.state == "mo" && f.level.state == 50 && f.temperature.state == 3);
  f.feed("@mn:0028\r\n");
  assert(f.status.state == "mn" && f.level.state == 0 && f.temperature.state == 4);
  f.feed("@mo:64FF\r\n");
  assert(f.level.state == 100 && std::isnan(f.temperature.state));
  f.feed("@mn:FF28\r\n");
  assert(std::isnan(f.level.state) && f.temperature.state == 4);
  f.feed("@mo:321E\r\n");
  for (auto s : {"abcd0028\r\n", "@mx:0028\r\n", "@moX0028\r\n", "@mo:001G\r\n",
                 "@mo:651E\r\n", "@mo:001E00\r\n", "@me:001E\r\n", "@me2\r\n"}) f.feed(s);
  assert(f.status.state == "mo" && f.level.state == 50);
  test_now = 9000;
  f.feed("@mn\r\n");
  assert(f.status.state == "mn" && f.level.state == 50);
  test_now = 10000;
  f.cooler.update();
  assert(f.online.state && std::isnan(f.level.state) && std::isnan(f.temperature.state));
  test_now = 19000;
  f.cooler.update();
  assert(!f.online.state && f.status.state == "unknown");
  f.feed("@mo\r\n");
  assert(f.online.state && std::isnan(f.level.state));
  for (auto s : {"@me\r\n", "@me1\r\n"}) {
    f.feed("@mo:321E\r\n");
    f.feed(s);
    assert(f.online.state && std::isnan(f.level.state) && std::isnan(f.temperature.state));
  }
  assert(f.status.state == "me1");
  f.feed("@mo:32");
  test_now += 501;
  f.feed("1E\r\n");
  assert(f.status.state == "me1");
  f.feed("@mo:32"); f.feed("1E\r\n");
  assert(f.level.state == 50);
  f.feed(std::string(128, 'x') + "@mn:001E\r\n");
  assert(f.status.state == "mo");
  f.feed("@mn\r\n");
  assert(f.status.state == "mn");
  assert(f.cooler.outgoing.empty()); // Default remains passive.
  Fixture p;
  p.cooler.set_startup_probe("hello\r\n", 1000, 2000, 3);
  test_now = 999; p.cooler.loop(); assert(p.cooler.outgoing.empty());
  test_now = 1000; p.cooler.loop(); assert(p.cooler.outgoing.size() == 7);
  test_now = 3000; p.cooler.loop(); assert(p.cooler.outgoing.size() == 14);
  p.feed("@mo\r\n");
  test_now = 5000; p.cooler.loop(); assert(p.cooler.outgoing.size() == 14);
  test_now = 20000; p.cooler.loop(); assert(p.cooler.outgoing.size() == 14);
  Fixture bounded;
  bounded.cooler.set_startup_probe("\r\n", 1, 1, 2);
  for (test_now = 1; test_now < 10; ++test_now) bounded.cooler.loop();
  assert(bounded.cooler.outgoing.size() == 4);
  Fixture before;
  before.cooler.set_startup_probe("\r\n", 1, 1, 2);
  before.feed("@me\r\n");
  test_now = 100; before.cooler.loop(); assert(before.cooler.outgoing.empty());
  Fixture reconnect;
  reconnect.cooler.set_communication_timeout(100);
  reconnect.cooler.set_measurement_timeout(1000);
  reconnect.feed("@mo:321E\r\n");
  test_now = 101; reconnect.feed("@mn\r\n");
  assert(reconnect.online.state && std::isnan(reconnect.level.state));
  Fixture wrap;
  wrap.cooler.set_communication_timeout(100);
  wrap.cooler.set_measurement_timeout(50);
  test_now = std::numeric_limits<uint32_t>::max() - 25;
  wrap.feed("@mo:321E\r\n");
  test_now = 30; wrap.cooler.update();
  assert(wrap.online.state && std::isnan(wrap.level.state));
  test_now = 80; wrap.cooler.update(); assert(!wrap.online.state);
  std::cout << "PASS: Cool Control status, telemetry, framing, expiry and bounded startup probe\n";
}
