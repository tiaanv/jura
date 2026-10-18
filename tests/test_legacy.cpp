
#include "../components/jura/jura.h"
#include <cassert>
#include <iostream>
using esphome::jura::Jura;
struct Test : Jura {
  bool idle() { return state_ == State::IDLE; }
  bool waiting() { return state_ == State::WAITING; }
  std::string active() { return active_command_; }
  size_t queued() { return commands_.size(); }
  bool polls() { return poll_rt_pending_ || poll_ic_pending_; }
  bool counters_valid(const std::string &s) {
    return valid_hex_payload_(s, "rt:", 64);
  }
};
void tick(Test &j, unsigned n = 1) {
  for (unsigned i = 0; i < n; ++i) {
    ++test_now;
    j.loop();
  }
}
void wait_reply(Test &j) {
  for (int n = 0; n < 500 && !j.waiting(); ++n)
    tick(j);
  assert(j.waiting());
}
void feed(Test &j, const std::string &s) {
  for (unsigned char c : s)
    for (unsigned b = 0; b < 8; b += 2)
      j.incoming.push_back(0xDB | (((c >> b) & 1) << 2) |
                           (((c >> (b + 1)) & 1) << 5));
}
std::string decode(const std::vector<uint8_t> &v) {
  std::string s;
  for (size_t i = 0; i + 3 < v.size(); i += 4) {
    uint8_t c = 0;
    for (int n = 0; n < 4; ++n)
      c |= (((v[i + n] >> 2) & 1) << (2 * n)) |
           (((v[i + n] >> 5) & 1) << (2 * n + 1));
    s += c;
  }
  return s;
}
int main() {
  test_now = 0;
  Test j;
  assert(!j.queue_command(""));
  assert(!j.queue_command("FA:09\r\n"));
  j.update();
  j.update();
  assert(j.queue_command("FA:09"));
  assert(j.queue_command("FA:06"));
  tick(j, 100);
  assert(j.active() == "FA:09");
  wait_reply(j);
  assert(decode(j.outgoing) == "FA:09\r\n");
  feed(j, "fa:OK\r\n");
  tick(j);
  assert(j.idle());
  tick(j, 100);
  assert(j.active() == "FA:06");
  wait_reply(j);
  tick(j, 4999);
  assert(j.waiting());
  tick(j);
  assert(j.idle());
  tick(j, 499);
  assert(j.idle());
  tick(j);
  assert(j.active() == "RT:0000");
  wait_reply(j);
  esphome::sensor::Sensor counter;
  j.register_metric_sensor("counter_1", &counter);
  std::string rt =
      "rt:00F40004014C2D3E003A0014003534F400490024BCB300320000011600000066\r\n";
  feed(j, rt);
  tick(j);
  assert(!counter.has_state());
  tick(j, 2);
  assert(j.idle());
  assert(counter.state == 244);
  tick(j, 100);
  assert(j.active() == "IC:");
  wait_reply(j);
  esphome::text_sensor::TextSensor bits;
  j.register_text_sensor("ic_bits", &bits);
  feed(j, "ic:0129\r\n");
  tick(j);
  assert(j.idle());
  assert(bits.state == "A=00000001 B=00101001");
  Test q;
  for (int i = 0; i < 8; ++i)
    assert(q.queue_command("FA:09"));
  assert(!q.queue_command("FA:09"));
  for (int i = 0; i < 100; ++i)
    q.update();
  assert(q.queued() == 8 && q.polls());
  Test bad;
  assert(bad.queue_command("FA:09"));
  tick(bad, 100);
  wait_reply(bad);
  bad.incoming.push_back(0);
  tick(bad);
  assert(bad.idle());
  Test wrap;
  test_now = 0xFFFFFF00u;
  assert(wrap.queue_command("FA:09"));
  tick(wrap, 101);
  wait_reply(wrap);
  tick(wrap, 5000);
  assert(wrap.idle());
  Test partial;
  test_now = 100;
  partial.update();
  tick(partial, 101);
  wait_reply(partial);
  feed(partial, "rt:00GG\r\n");
  tick(partial);
  assert(partial.idle());
  assert(!partial.counters_valid("rt:0001"));
  std::cout
      << "PASS: FIFO, queue limits, poll coalescing, wire encoding, split RX, "
         "publishing, timeout, recovery, invalid input and millis rollover\n";
}
