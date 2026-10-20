#pragma once
#include <cmath>
#include <cstdint>
#include <string>

namespace esphome {
namespace jura_native {
// Latest-state bridge, not a FIFO: imported Home Assistant entities may arrive separately.
class CoolcontrolBridge {
 public:
  void configure(uint32_t timeout, uint32_t coalesce) {
    enabled_ = true;
    timeout_ = timeout;
    coalesce_ = coalesce;
  }
  bool enabled() const { return enabled_; }
  static bool valid(const std::string &s) {
    if (s == "@mo" || s == "@mn" || s == "@me" || s == "@me1") return true;
    if (s.size() != 8 || (s.compare(0, 4, "@mo:") && s.compare(0, 4, "@mn:"))) return false;
    for (size_t i = 4; i < 8; ++i) if (nibble_(s[i]) < 0) return false;
    const int level = nibble_(s[4]) * 16 + nibble_(s[5]);
    return level <= 100 || level == 255;
  }
  void snapshot(const std::string &raw, float counter, bool online, uint32_t now) {
    if (!enabled_) return;
    if (!online || !valid(raw) || !std::isfinite(counter) || counter < 0 || counter > 16777215 ||
        std::floor(counter) != counter) {
      if (source_online_ || fresh_) pending_ = true;
      source_online_ = fresh_ = coalescing_ = have_counter_ = false;
      return;
    }
    const bool heartbeat = !have_counter_ || counter != counter_;
    const bool changed = !source_online_ || heartbeat || raw != candidate_;
    source_online_ = have_counter_ = true;
    counter_ = counter;
    if (heartbeat) last_heartbeat_ = now;
    if (changed) {
      candidate_ = raw;
      changed_at_ = now;
      if (!coalescing_) window_started_ = now;
      coalescing_ = true;
    }
  }
  void advance(uint32_t now) {
    if (!enabled_) return;
    if (source_online_ && uint32_t(now - last_heartbeat_) >= timeout_) {
      if (fresh_ || coalescing_) pending_ = true;
      fresh_ = coalescing_ = false;
    } else if (coalescing_ && (uint32_t(now - changed_at_) >= coalesce_ ||
                                uint64_t(uint32_t(now - window_started_)) >= uint64_t(coalesce_) * 2)) {
      raw_ = candidate_;
      fresh_ = true;
      coalescing_ = false;
      pending_ = true;
    }
  }
  void reset_session() {
    reporting_ = extended_ = false;
    sent_ = false;
    pending_ = true;
    last_sent_.clear();
  }
  void negotiate(uint8_t flags) {
    reporting_ = (flags & 0x80) != 0;
    extended_ = (flags & 0x40) != 0;
    pending_ = true;
  }
  bool next(uint32_t now, std::string &command) {
    if (!enabled_ || !reporting_ || !pending_ || coalescing_ ||
        (sent_ && uint32_t(now - last_send_) < 1000)) return false;
    command = fresh_ ? raw_ : "@me1";
    if (command.size() == 8 && (!extended_ || (nibble_(command[4]) == 15 && nibble_(command[5]) == 15))) command.resize(3);
    pending_ = false;
    // Send an expiry indication once, rather than inventing repeated cooler traffic.
    if (!fresh_ && last_sent_ == command) return false;
    last_sent_ = command;
    sent_ = true;
    last_send_ = now;
    return true;
  }
 private:
  static int nibble_(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }
  bool enabled_{false}, reporting_{false}, extended_{false}, source_online_{false};
  bool fresh_{false}, coalescing_{false}, have_counter_{false}, pending_{false}, sent_{false};
  uint32_t timeout_{15000}, coalesce_{250}, last_heartbeat_{0}, changed_at_{0}, window_started_{0}, last_send_{0};
  float counter_{0};
  std::string candidate_, raw_, last_sent_;
};
}  // namespace jura_native
}  // namespace esphome
