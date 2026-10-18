#pragma once
#include <cstdint>
#include <deque>
#include <vector>
namespace esphome::uart {
struct UARTDevice {
  std::deque<uint8_t> incoming;
  std::vector<uint8_t> outgoing;
  size_t available() { return incoming.size(); }
  bool read_byte(uint8_t *b) {
    if (incoming.empty()) return false;
    *b = incoming.front();
    incoming.pop_front();
    return true;
  }
  void write_array(const uint8_t *p, size_t n) { outgoing.insert(outgoing.end(), p, p + n); }
};
}  // namespace esphome::uart
