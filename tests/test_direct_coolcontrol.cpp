#include <cassert>
#include <iostream>
#include <limits>
#include "../components/jura_coolcontrol/jura_coolcontrol.h"
#include "../components/jura_native/jura_native.h"
using namespace esphome;
struct Receiver : jura_native::JuraNative {
  bool reply(std::string &out) {
    cooler_.advance(test_now);
    cooler_.negotiate(0xC0);
    return cooler_.next(test_now, out);
  }
};
int main() {
  test_now = 0;
  jura_coolcontrol::JuraCoolcontrol source;
  sensor::Sensor wire;
  Receiver receiver;
  receiver.set_coolcontrol_packet(&wire, 15000);
  receiver.setup();
  source.set_bridge_packet_sensor(&wire);
  source.setup();
  std::string out;
  assert(receiver.reply(out) && out == "@me1"); // No serial data yet.
  auto feed = [&](const std::string &frame) {
    for (char c : frame + "\r\n") source.incoming.push_back(uint8_t(c));
    source.loop();
  };
  for (const auto &frame : {"@mo", "@mn", "@me", "@me1", "@mo:0028", "@mn:641E", "@mo:32FF"}) {
    ++test_now;
    feed(frame);
    assert(receiver.reply(out) && out == frame);
  }
  feed("@mo:FF28");
  assert(receiver.reply(out) && out == "@mo"); // Existing unknown-level wire fallback.
  const auto good = wire.state;
  feed("@mo:651E"); feed("noise");
  assert(wire.state == good); // Invalid UART cannot overwrite accepted data.
  // Periodic delivery of an identical numeric value, without a changing counter.
  test_now = 14000;
  wire.publish_state(good);
  assert(receiver.reply(out) && out == "@mo");
  test_now = 28000;
  wire.publish_state(good);
  assert(receiver.reply(out) && out == "@mo");
  test_now = 43000;
  assert(receiver.reply(out) && out == "@me1"); // Retained sensor state does not refresh liveness.
  wire.publish_state(good);
  assert(receiver.reply(out) && out == "@mo");
  // A running sender with a silent UART must withdraw rather than repeat healthy status.
  source.loop();
  assert(wire.state == float(0xA00000) && receiver.reply(out) && out == "@me1");
  for (float value : {NAN, INFINITY, -1.0f, 0.5f, float(0xFFFFFF), float(0xA70000),
                      float(0xA10001), float(0xA56500), float(0xB10000)}) {
    wire.publish_state(value);
    assert(receiver.reply(out) && out == "@me1");
  }
  test_now = 44000;
  feed("@mn:0028");
  assert(receiver.reply(out) && out == "@mn:0028");
  receiver.set_coolcontrol_override(true);
  wire.publish_state(float(0xA00000));
  assert(receiver.reply(out) && out == "@mo");
  receiver.set_coolcontrol_override(false);
  assert(receiver.reply(out) && out == "@me1");
  // Clock rollover still expires a missed heartbeat correctly.
  test_now = UINT32_MAX - 1000;
  wire.publish_state(float(0xA10000));
  test_now = 14000;
  assert(receiver.reply(out) && out == "@me1");
  std::cout << "PASS: direct cooler packet round trips, repeated-state heartbeat, UART/network expiry, malformed input and override\n";
}
