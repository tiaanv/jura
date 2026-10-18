#pragma once
#include <cstdint>
inline uint32_t test_now = 0;
namespace esphome {
inline uint32_t millis() { return test_now; }
}  // namespace esphome
