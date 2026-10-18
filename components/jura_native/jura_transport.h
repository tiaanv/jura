#pragma once

#include <cstdint>
#include <string>

#include "jura_cipher.h"

namespace esphome {
namespace jura_native {
// UART framing only. Session management and the user queue live in Jura.
class TransportSink {
 public:
  virtual ~TransportSink() = default;
  virtual uint32_t transmit(const uint8_t *data, size_t size) = 0;
  virtual void received(const std::string &frame, bool encrypted) = 0;
  virtual void completed(bool success) = 0;
  virtual void framing_error() = 0;
};

class JuraTransport {
 public:
  explicit JuraTransport(TransportSink &sink) : sink_(sink) {}
  bool idle() const { return state_ == State::IDLE; }
  bool waiting() const { return state_ == State::WAITING; }
  bool can_start(uint32_t now) const {
    return idle() && frame_.empty() && bits_ == 0 && !discard_ &&
           (!have_rx_ || uint32_t(now - last_rx_) >= 100) &&
           (!have_finished_ || uint32_t(now - finished_) >= cooldown_);
  }
  bool start(const std::string &command, bool encrypted, bool expect_reply, uint32_t now) {
    if (!can_start(now)) return false;
    tx_ = encrypted ? jura_protocol::atproto::build_frame(command + "\r\n", 0x2A) : command + "\r\n";
    pos_ = 0;
    expect_reply_ = expect_reply;
    state_ = State::SENDING;
    started_ = now;
    return true;
  }
  void complete(bool success, uint32_t now) {
    if (idle()) return;
    state_ = State::IDLE;
    finished_ = now;
    have_finished_ = true;
    cooldown_ = success ? 100 : 500;
    sink_.completed(success);
  }
  void feed(uint8_t raw, uint32_t now) {
    expire_(now);
    last_rx_ = now;
    have_rx_ = true;
    if (discard_) return;
    // Bit 7 is ignored by some Jura implementations.
    if ((raw & 0x5B) != 0x5B) {
      invalid_();
      return;
    }
    byte_ |= uint8_t(((((raw >> 2) & 1u) | (((raw >> 5) & 1u) << 1))) << bits_);
    bits_ += 2;
    if (bits_ != 8) return;
    const uint8_t value = byte_;
    bits_ = byte_ = 0;
    if (frame_.size() >= 1024) {
      invalid_();
      return;
    }
    frame_.push_back(static_cast<char>(value));
    if (frame_[0] != '&' && value != '\r' && value != '\n' && (value < 32 || value > 126)) {
      invalid_();
      return;
    }
    const size_t n = frame_.size();
    // Reserved bytes in encrypted frames (including the key) are escaped.
    if (n < 2 || frame_[n - 2] != '\r' || frame_[n - 1] != '\n') return;
    std::string frame;
    frame.swap(frame_);
    if (frame[0] == '&') {
      std::string plaintext;
      if (!jura_protocol::atproto::decode_frame(frame, plaintext)) {
        sink_.framing_error();
        return;
      }
      sink_.received(plaintext, true);
    } else {
      frame.resize(n - 2);
      sink_.received(frame, false);
    }
  }
  void tick(uint32_t now) {
    expire_(now);
    if (state_ == State::SENDING) {
      if (uint32_t(now - started_) >= 5000) {
        complete(false, now);
        return;
      }
      // 4 x 10 UART bits / 9600 = 4.167 ms, rounded up, then 9 ms idle.
      if (pos_ && uint32_t(now - last_tx_) < 14) return;
      const uint8_t c = static_cast<uint8_t>(tx_[pos_++]);
      uint8_t encoded[4];
      for (unsigned i = 0; i < 4; ++i)
        encoded[i] = uint8_t(0xDB | (((c >> (2 * i)) & 1u) << 2) | (((c >> (2 * i + 1)) & 1u) << 5));
      last_tx_ = sink_.transmit(encoded, 4);
      if (pos_ == tx_.size()) {
        state_ = expect_reply_ ? State::WAITING : State::DRAINING;
        wait_started_ = last_tx_;
      }
    } else if (state_ == State::DRAINING && uint32_t(now - last_tx_) >= 5) {
      complete(true, now);
    } else if (state_ == State::WAITING && uint32_t(now - wait_started_) >= 5000) {
      complete(false, now);
    }
  }

 private:
  enum class State { IDLE, SENDING, WAITING, DRAINING };
  void invalid_() {
    frame_.clear();
    bits_ = byte_ = 0;
    discard_ = true;
    sink_.framing_error();
  }
  void expire_(uint32_t now) {
    if (!have_rx_ || uint32_t(now - last_rx_) < 100) return;
    if (!frame_.empty() || bits_) sink_.framing_error();
    frame_.clear();
    bits_ = byte_ = 0;
    discard_ = false;
    have_rx_ = false;
  }
  TransportSink &sink_;
  State state_{State::IDLE};
  std::string tx_, frame_;
  size_t pos_{0};
  uint8_t byte_{0}, bits_{0};
  bool expect_reply_{true}, discard_{false}, have_rx_{false}, have_finished_{false};
  uint32_t last_rx_{0}, last_tx_{0}, started_{0}, wait_started_{0}, finished_{0}, cooldown_{100};
};
}  // namespace jura_native
}  // namespace esphome
