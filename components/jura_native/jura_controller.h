#pragma once
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <set>

#include "jura_profiles.h"
#include "jura_progress.h"
#include "jura_transport.h"

namespace esphome {
namespace jura_native {
class JuraController : public TransportSink {
 public:
  JuraController() : transport_(*this) {
    selected_.fill(-1);
    counters_.fill(0xFFFF);
  }
  virtual ~JuraController() = default;
  void configure(bool brew, bool power, bool raw, uint32_t startup, const std::string &response,
                 const std::string &hint = "") {
    allow_brew_ = brew;
    allow_power_ = power;
    raw_extras_ = raw;
    startup_delay_ = startup;
    response_override_ = response;
    profile_hint_ = hint;
  }
  void begin(uint32_t now) {
    now_ = boot_ = now;
    identify_pending_ = true;
    text("status", "connecting");
    text("status_detail", "Identifying machine");
    text("session", "Starting");
    binary("connected", 0);
    text("command_result", "No command");
    text("progress_state", "Unknown");
    number("progress", NAN);
  }
  bool discovery_ready() const { return discovery_done_; }
  bool profile_ok() const { return !profiles_.ef.empty(); }
  const Profiles &profile() const { return profiles_; }
  const Product *selection() const {
    return selected_product_ < profiles_.products.size() ? &profiles_.products[selected_product_] : nullptr;
  }
  const std::array<int32_t, ATTRIBUTE_COUNT> &selected_values() const { return selected_; }
  void input(uint8_t raw, uint32_t now) {
    now_ = now;
    transport_.feed(raw, now);
  }
  void advance(uint32_t now) {
    now_ = now;
    transport_.tick(now);
    if (!discovery_done_ && uint32_t(now - boot_) >= 8000) {
      if (!profile_hint_.empty() && profiles_.select(profile_hint_))
        install_profile_();
      else {
        discovery_done_ = true;
        profile_changed(false);
        text("profile", "Unknown; restart after identification");
      }
    }
    if (session_ != Session::OFF && session_ != Session::READY && session_ != Session::BACKOFF &&
        uint32_t(now - session_started_) >= 30000)
      session_failed_("Handshake deadline exceeded");
    if (session_ == Session::READY && uint32_t(now - last_activity_) >= 60000)
      session_failed_("No valid AT traffic");
    const bool stale = !tf_valid_ || uint32_t(now - last_tf_) >= 60000;
    binary("alerts_stale", stale ? 1 : 0);
    if (stale && !alerts_invalidated_) {
      alerts_invalidated_ = true;
      for (const auto &a : profiles_.alerts) binary(key_("alert_", a.bit), -1);
      text("active_alerts", "Unknown (stale alerts)");
      text("tray_status", "Unknown");
      text("water_tank_status", "Unknown");
      update_status_();
    }
    if (operation_seen_ && uint32_t(now - last_tv_) >= 20000) {
      operation_seen_ = false;
      if (operation_finished_) {
        text("progress_state", "Idle");
        text("current_product", "None");
        binary("brew_active", 0);
      } else {
        text("progress_state", "Unknown (progress timeout)");
        binary("brew_active", -1);
        awaiting_tf_ = true;
        tf_requested_ = true;
      }
      number("progress", NAN);
      clear_stage_();
      update_status_();
    }
    if (brew_pending_ && brew_sent_ && !brew_started_ && uint32_t(now - brew_sent_at_) >= 20000) {
      brew_pending_ = false;
      brew_sent_ = false;
      text("command_result", "Uncertain: no start confirmation; not retried");
    }
    if (refresh_after_finish_ && uint32_t(now - finished_at_) >= 12000) {
      refresh_after_finish_ = false;
      refresh();
    }
    if (!transport_.can_start(now)) return;
    if (!handshake_command_.empty()) {
      auto command = handshake_command_;
      handshake_command_.clear();
      start_(command, handshake_kind_, true, handshake_kind_ != Kind::H3);
      return;
    }
    if (identify_pending_) {
      identify_pending_ = false;
      start_("TY:", Kind::TYPE);
      return;
    }
    if (!model_mismatch_ && discovery_done_ && uint32_t(now - boot_) >= startup_delay_ &&
        (session_ == Session::OFF ||
         (session_ == Session::BACKOFF && uint32_t(now - session_failed_at_) >= 60000))) {
      start_session_();
      return;
    }
    if (!control_.empty()) {
      const Request r = control_.front();
      control_.pop_front();
      if (r.kind == Kind::BREW && (!ready_for_brew_() || !brew_pending_)) {
        brew_pending_ = false;
        text("command_result", "Rejected: machine no longer ready");
        return;
      }
      start_(r.command, r.kind, r.encrypted);
      if (r.kind == Kind::BREW) {
        brew_sent_ = true;
        brew_sent_at_ = now;
        text("command_result", "Sent; waiting for machine");
      }
      return;
    }
    if (session_ != Session::READY) return;
    // TF is replaced by TV during a product. Don't query or restart a live TV session.
    if (!busy_() && (tf_requested_ || uint32_t(now - last_tf_query_) >= 30000)) {
      tf_requested_ = false;
      last_tf_query_ = now;
      start_("@TF:", Kind::TF, true);
      return;
    }
    if (busy_()) return;
    if (!polls_.empty()) {
      const auto r = polls_.front();
      polls_.pop_front();
      start_(r.command, r.kind, r.encrypted);
      return;
    }
  }
  void refresh() {
    if (!profile_ok() || !polls_.empty()) return;
    std::set<unsigned> pages{0};
    for (const auto &p : profiles_.products)
      if (p.flags & 1) pages.insert(p.code / 4);
    if (profiles_.ef == "EF545") pages.insert(0x12 / 4);
    for (unsigned page : pages) {
      char cmd[16];
      std::snprintf(cmd, sizeof(cmd), "@TR:32,%02X", page);
      polls_.push_back({cmd, Kind::PRODUCTS, true});
    }
    polls_.push_back({"@TG:43", Kind::MAINTENANCE, true});
    polls_.push_back({"@TG:C0", Kind::PERCENT, true});
    if (raw_extras_ && profiles_.ef == "EF533") polls_.push_back({"RT:0000", Kind::RAW, false});
  }
  bool choose(const std::string &name) {
    const auto *p = profiles_.find(name);
    if (!p) return reject_("Unknown or unsupported drink");
    selected_product_ = size_t(p - profiles_.products.data());
    reset_recipe();
    return true;
  }
  void reset_recipe() {
    selected_.fill(-1);
    if (const auto *p = selection())
      for (uint8_t i = 0; i < p->n_options; ++i) {
        const auto o = profiles_.option(*p, i);
        if (o.valid()) selected_[o.attr] = o.def;
      }
    recipe_changed();
  }
  bool set_parameter(uint8_t attr, int32_t value) {
    const auto *p = selection();
    if (!p || attr >= ATTRIBUTE_COUNT) return reject_("No selected product");
    const auto o = profiles_.option_for(*p, attr);
    if (!o.valid() || !o.accepts(value)) return reject_("Unsupported value for selected drink");
    selected_[attr] = value;
    recipe_changed();
    return true;
  }
  bool brew_selected() {
    const auto *p = selection();
    return p ? submit_(*p, selected_.data()) : reject_("No drink selected");
  }
  bool brew_named(const std::string &name) {
    std::array<int32_t, ATTRIBUTE_COUNT> values;
    values.fill(-1);
    return brew_recipe(name, values);
  }
  bool brew_recipe(const std::string &name, const std::array<int32_t, ATTRIBUTE_COUNT> &values) {
    const auto *p = profiles_.find(name);
    return p ? submit_(*p, values.data()) : reject_("Unknown drink");
  }
  bool brew_custom(const std::string &name, const std::string &strength, int water_ml, int milk_seconds,
                   int foam_seconds, const std::string &temperature, int bypass_ml, int pause_seconds,
                   const std::string &grinder_ratio) {
    const auto *p = profiles_.find(name);
    if (!p) return reject_("Unknown drink");
    std::array<int32_t, ATTRIBUTE_COUNT> values{
        {-1, -1, -1, water_ml, milk_seconds, foam_seconds, bypass_ml, pause_seconds}};
    const std::pair<uint8_t, std::string> choices[] = {
        {STRENGTH, strength}, {TEMPERATURE, temperature}, {GRINDER, grinder_ratio}};
    for (const auto &entry : choices) {
      if (entry.second.empty() || Profiles::lower(entry.second) == "default") continue;
      const auto option = profiles_.option_for(*p, entry.first);
      bool found = false;
      for (const auto &choice : option.choices)
        if (Profiles::lower(choice.name) == Profiles::lower(entry.second)) {
          values[entry.first] = choice.value;
          found = true;
          break;
        }
      if (!found) return reject_("Unknown strength, temperature or grinder choice for selected drink");
    }
    return brew_recipe(name, values);
  }
  bool power_off() {
    if (!allow_power_) return reject_("Power-off control disabled");
    if (brew_pending_ || busy_() || !control_.empty()) return reject_("Machine busy");
    control_.push_back({"AN:02", Kind::POWER, false});
    text("command_result", "Power off queued");
    return true;
  }
  // Compatibility with existing button lambdas; new cards should call named methods.
  bool queue_command(const std::string &command) {
    if (command == "AN:02") return power_off();
    if (command == "TY:") {
      identify_pending_ = true;
      return true;
    }
    if (profiles_.ef != "EF533") return reject_("Legacy button mapping not verified for this model");
    uint8_t code = command == "FA:04"   ? 2
                   : command == "FA:09" ? 3
                   : command == "FA:06" ? 13
                   : command == "FA:07" ? 4
                   : command == "FA:05" ? 1
                                        : 0;
    const auto *p = profiles_.find(code);
    return p ? brew_named(p->name) : reject_("Unsupported legacy command");
  }

 protected:
  enum class Session { OFF, ACK0, ACK1, CHALLENGE, MODEL, ACK_MODEL, READY, BACKOFF };
  enum class Kind { NONE, TYPE, H0, H1, H2, H3, TF, PRODUCTS, MAINTENANCE, PERCENT, RAW, BREW, POWER };
  struct Request {
    std::string command;
    Kind kind;
    bool encrypted;
  };
  virtual void text(const std::string &key, const std::string &value) = 0;
  virtual void number(const std::string &key, float value) = 0;
  virtual void binary(const std::string &key, int value) = 0;  // -1 = unavailable
  virtual void profile_changed(bool supported) = 0;
  virtual void recipe_changed() = 0;
  virtual uint32_t write_transport(const uint8_t *data, size_t count) = 0;
  virtual void trace(const std::string &) {}
  static std::string key_(const char *prefix, unsigned value) {
    return std::string(prefix) + std::to_string(value);
  }
  static bool starts_(const std::string &s, const std::string &p) {
    return Profiles::lower(s).compare(0, p.size(), Profiles::lower(p)) == 0;
  }
  static bool hex_(const std::string &s) {
    if (s.empty()) return false;
    for (char c : s)
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    return true;
  }
  static unsigned hexval_(const std::string &s) {
    return static_cast<unsigned>(std::strtoul(s.c_str(), nullptr, 16));
  }
  static std::string hexbyte_(uint8_t value) {
    const char d[] = "0123456789ABCDEF";
    return std::string() + d[value >> 4] + d[value & 15];
  }
  bool reject_(const std::string &why) {
    text("command_result", "Rejected: " + why);
    return false;
  }
  bool busy_() const { return operation_seen_ && !operation_finished_; }
  bool flag_(unsigned bit) const { return bit / 8 < tf_.size() && (tf_[bit / 8] & (0x80u >> (bit % 8))); }
  bool blocked_() const {
    for (const auto &a : profiles_.alerts)
      if (a.blocking && flag_(a.bit)) return true;
    return false;
  }
  bool ready_for_brew_() const {
    if (model_mismatch_ || !profile_ok() || session_ != Session::READY || !tf_valid_ || busy_() ||
        awaiting_tf_ || uint32_t(now_ - last_tf_) >= 60000 || blocked_())
      return false;
    for (const auto &a : profiles_.alerts)
      if (a.blocking && a.bit / 8 >= tf_.size()) return false;
    for (const auto &a : profiles_.alerts)
      if (Profiles::lower(a.name) == "coffee ready") return flag_(a.bit);
    return false;  // No positive readiness signal: don't guess.
  }
  bool submit_(const Product &product, const int32_t *values) {
    if (!allow_brew_) return reject_("Brewing control disabled");
    if (brew_pending_ || !control_.empty()) return reject_("Another command is pending");
    if (!ready_for_brew_()) return reject_("Machine not ready or status unavailable");
    std::string payload, error;
    if (!profiles_.recipe(product, values, payload, error)) return reject_(error);
    brew_pending_ = true;
    brew_sent_ = false;
    brew_started_ = false;
    requested_code_ = product.code;
    control_.push_back({"@TP:" + payload, Kind::BREW, true});
    text("requested_product", product.name);
    text("command_result", "Queued");
    return true;
  }
  void start_(const std::string &command, Kind kind, bool encrypted = false, bool reply = true) {
    active_ = kind;
    active_command_ = command;
    transport_.start(command, encrypted, reply, now_);
    trace("Sending " + command);
  }
  void identify_(const std::string &model) {
    text("detected_model", model);
    if (!detected_.empty() && model == detected_) return;
    detected_ = model;
    if (discovery_done_) {
      Profiles candidate;
      if (profile_ok() && candidate.select(model) && candidate.ef == profiles_.ef) return;
      model_mismatch_ = true;
      session_failed_("Machine/profile changed: restart to update native entities");
      return;
    }
    if (profiles_.select(model))
      install_profile_();
    else {
      discovery_done_ = true;
      profile_changed(false);
      text("profile", "Unsupported machine");
    }
  }
  void install_profile_() {
    discovery_done_ = true;
    profile_changed(true);
    text("profile", profiles_.ef);
    text("model_family", profiles_.name);
    selected_product_ = 0;
    for (size_t i = 0; i < profiles_.products.size(); ++i)
      if (profiles_.products[i].code == 2 && (profiles_.products[i].flags & 1)) {
        selected_product_ = i;
        break;
      }
    reset_recipe();
    refresh();
  }
  void start_session_() {
    session_ = Session::ACK0;
    session_started_ = now_;
    handshake_command_ = "@T0";
    handshake_kind_ = Kind::H0;
    tf_valid_ = false;
    tf_.clear();
    binary("connected", 0);
    text("session", "Handshake: T0");
    update_status_();
  }
  void session_failed_(const std::string &why) {
    session_ = Session::BACKOFF;
    session_failed_at_ = now_;
    handshake_command_.clear();
    polls_.clear();
    control_.clear();
    tf_valid_ = false;
    brew_pending_ = false;
    brew_sent_ = false;
    operation_seen_ = false;
    binary("connected", 0);
    binary("brew_active", -1);
    number("progress", NAN);
    clear_stage_();
    for (const auto &a : profiles_.alerts) binary(key_("alert_", a.bit), -1);
    text("session", why + (model_mismatch_ ? "; restart required" : "; retry in 60 s"));
    text("status", "offline");
    text("status_detail", why);
    text("tray_status", "Unknown");
    text("water_tank_status", "Unknown");
    text("active_alerts", "Unknown");
    text("command_result", "Connection lost; no control command retried");
  }
  void update_status_() {
    if (session_ != Session::READY) {
      text("status", "connecting");
      return;
    }
    if (busy_()) {
      text("status", "busy");
      text("status_detail", phase_);
      return;
    }
    if (awaiting_tf_) {
      text("status", operation_finished_ ? "finishing" : "unknown");
      text("status_detail", "Waiting for fresh machine status");
      return;
    }
    if (!tf_valid_ || uint32_t(now_ - last_tf_) >= 60000) {
      text("status", "unknown");
      text("status_detail", "Alerts unavailable or stale");
      return;
    }
    for (const auto &a : profiles_.alerts) {
      if (a.blocking && a.bit / 8 >= tf_.size()) {
        text("status", "unknown");
        text("status_detail", "Incomplete machine status");
        return;
      }
    }
    std::string reasons;
    for (const auto &a : profiles_.alerts)
      if (a.blocking && flag_(a.bit)) {
        if (!reasons.empty()) reasons += ", ";
        reasons += a.name;
      }
    if (!reasons.empty()) {
      text("status", "blocked");
      text("status_detail", reasons.substr(0, 250));
    } else if (flag_(12)) {
      text("status", "heating");
      text("status_detail", "Heating up");
    } else if (flag_(13)) {
      text("status", "ready");
      text("status_detail", "Ready");
    } else {
      text("status", "not_ready");
      text("status_detail", "Machine not reporting ready");
    }
  }
  uint32_t transmit(const uint8_t *data, size_t size) override { return write_transport(data, size); }
  void framing_error() override { number("framing_errors", float(++framing_errors_)); }
  void completed(bool success) override {
    const Kind kind = active_;
    active_ = Kind::NONE;
    if (!success) {
      trace("Timeout: " + active_command_);
      number("timeouts", float(++timeouts_));
      if (kind == Kind::H0 || kind == Kind::H1 || kind == Kind::H2 || kind == Kind::H3)
        session_failed_("Handshake command timeout");
      else if (kind == Kind::BREW) {
        text("command_result", "Awaiting start; no immediate reply (not retried)");
      } else if (kind == Kind::POWER)
        text("command_result", "Power-off reply timeout; not retried");
    } else if (kind == Kind::H3 && session_ == Session::ACK_MODEL) {
      session_ = Session::READY;
      last_activity_ = now_;
      tf_requested_ = true;
      text("session", "Ready; waiting for TF");
      refresh();
    }
  }
  void received(const std::string &frame, bool encrypted) override {
    if (encrypted) {
      receive_at_(frame);
      return;
    }
    if (starts_(frame, "ty:")) identify_(frame.substr(3));
    if (!transport_.waiting()) return;
    if (active_ == Kind::TYPE && starts_(frame, "ty:") && frame.size() > 3)
      transport_.complete(true, now_);
    else if (active_ == Kind::POWER && starts_(frame, "an:")) {
      text("command_result", "Power-off response: " + frame);
      transport_.complete(true, now_);
    } else if (active_ == Kind::RAW && starts_(frame, "rt:") && frame.size() >= 67 &&
               (frame.size() - 3) % 4 == 0 && hex_(frame.substr(3))) {
      publish_raw_(frame.substr(3));
      transport_.complete(true, now_);
    }
  }
  void receive_at_(const std::string &frame) {
    trace("AT received: " + frame);
    if (session_ == Session::READY) {
      last_activity_ = now_;
      binary("connected", 1);
    }
    if (frame == "@t0" && session_ == Session::ACK0 && active_ == Kind::H0 && transport_.waiting()) {
      transport_.complete(true, now_);
      session_ = Session::ACK1;
      handshake_command_ = "@T1";
      handshake_kind_ = Kind::H1;
    } else if (frame == "@t1" && session_ == Session::ACK1 && active_ == Kind::H1 && transport_.waiting()) {
      transport_.complete(true, now_);
      session_ = Session::CHALLENGE;
    } else if (frame.compare(0, 4, "@T2:") == 0 &&
               (session_ == Session::CHALLENGE || session_ == Session::ACK1)) {
      const auto challenge = frame.substr(4);
      text("challenge", challenge);
      if (active_ == Kind::H1 && transport_.waiting()) transport_.complete(true, now_);
      std::string response = response_override_;
      // Both responses are captured dongle exchanges. Unknown challenges require an override.
      if (response.empty()) {
        if (Profiles::lower(challenge) == "010001b228")
          response = "8120000000";
        else if (Profiles::lower(challenge) == "01c000")
          response = "818000";
      }
      if (!hex_(challenge) || challenge.size() % 2 || !hex_(response) ||
          response.size() != challenge.size()) {
        session_failed_("Unrecognised challenge; configure handshake_response");
        return;
      }
      session_ = Session::MODEL;
      handshake_command_ = "@t2:" + response;
      handshake_kind_ = Kind::H2;
    } else if (frame.compare(0, 4, "@T3:") == 0 && session_ == Session::MODEL && active_ == Kind::H2 &&
               transport_.waiting()) {
      const auto ef = frame.find("EF", 4);
      if (ef == std::string::npos) return;
      identify_(frame.substr(ef));
      if (session_ == Session::BACKOFF) return;
      transport_.complete(true, now_);
      session_ = Session::ACK_MODEL;
      handshake_command_ = "@t3";
      handshake_kind_ = Kind::H3;
    } else if (session_ == Session::READY) {
      if (starts_(frame, "@TF:")) {
        if (parse_tf_(frame) && active_ == Kind::TF && transport_.waiting()) transport_.complete(true, now_);
      } else if (starts_(frame, "@TV:")) {
        parse_tv_(frame);
      } else if (frame == "@TB") {
        operation_seen_ = true;
        operation_finished_ = false;
        last_tv_ = now_;
        phase_ = "Started";
        binary("brew_active", 1);
        text("progress_state", phase_);
        number("progress", NAN);
        if (brew_pending_) {
          text("command_result", "Machine active; waiting for product confirmation");
          if (active_ == Kind::BREW && transport_.waiting()) transport_.complete(true, now_);
        }
        update_status_();
      } else if (frame == "@tp" && brew_pending_) {
        text("command_result", "Accepted; waiting for start");
        if (active_ == Kind::BREW && transport_.waiting()) transport_.complete(true, now_);
      } else if (starts_(frame, "@tp:") && brew_pending_) {
        brew_pending_ = false;
        brew_sent_ = false;
        text("command_result", "Rejected by machine: " + frame);
        if (active_ == Kind::BREW && transport_.waiting()) transport_.complete(true, now_);
      } else
        parse_statistics_(frame);
    }
  }
  bool parse_tf_(const std::string &frame) {
    const auto payload = frame.substr(4);
    if (!hex_(payload) || payload.size() % 2 || payload.size() > 64) return false;
    tf_.clear();
    for (size_t i = 0; i < payload.size(); i += 2) tf_.push_back(uint8_t(hexval_(payload.substr(i, 2))));
    tf_valid_ = true;
    alerts_invalidated_ = false;
    last_tf_ = now_;
    awaiting_tf_ = false;
    binary("connected", 1);
    text("session", "Connected");
    text("tf_raw", payload);
    if (!operation_seen_ && brew_pending_ && brew_started_ && flag_(13)) {
      brew_pending_ = false;
      brew_sent_ = false;
      text("command_result", "Machine ready again; completion unconfirmed");
    }
    std::string labels;
    for (const auto &a : profiles_.alerts) {
      binary(key_("alert_", a.bit), a.bit / 8 < tf_.size() ? (flag_(a.bit) ? 1 : 0) : -1);
      if (flag_(a.bit) && a.bit != 12 && a.bit != 13 && a.bit != 36 && a.bit != 37) {
        if (!labels.empty()) labels += ", ";
        labels += a.name;
      }
    }
    text("active_alerts", labels.empty() ? "None" : labels.substr(0, 250));
    text("tray_status", flag_(0) ? "Missing" : "Present");
    text("water_tank_status", flag_(1) ? "Fill Tank" : "OK");
    update_status_();
    return true;
  }
  static const char *process_(uint8_t code) {
    switch (code) {
      case 0x21:
        return "Milk system cleaning";
      case 0x22:
        return "Coffee rinsing";
      case 0x23:
        return "Milk system rinsing";
      case 0x24:
        return "Cleaning";
      case 0x25:
        return "Descaling";
      case 0x26:
        return "Filter change";
      default:
        return nullptr;
    }
  }
  static std::string phase_name_(uint8_t state) {
    switch (state) {
      case 0x39:
        return "Grinding";
      case 0x3C:
        return "Dispensing coffee";
      case 0x3E:
        return "Finished";
      case 0x31:
        return "Grinding (milk drink)";
      case 0x32:
        return "Dispensing milk";
      case 0x33:
        return "Milk pause";
      case 0x34:
        return "Dispensing milk foam";
      case 0x37:
        return "Dispensing water (milk drink)";
      case 0x41:
        return "Dispensing water";
      case 0x21:
        return "Heating";
      case 0x23:
        return "Rinsing";
      case 0x54:
        return "Descaling";
      case 0x74:
        return "Cleaning";
      default:
        for (const auto &entry : PROGRESS_DEFINITIONS)
          if (entry.code == state) return entry.name;
        return "State 0x" + hexbyte_(state);
    }
  }
  void clear_stage_() {
    number("stage_current", NAN);
    number("stage_target", NAN);
    text("stage_unit", "");
  }
  void parse_tv_(const std::string &frame) {
    const auto payload = frame.substr(4);
    if (!hex_(payload) || payload.size() < 4 || payload.size() % 2 || payload.size() > 80) return;
    text("tv_raw", payload);
    std::vector<uint8_t> v;
    for (size_t i = 0; i < payload.size(); i += 2) v.push_back(uint8_t(hexval_(payload.substr(i, 2))));
    const uint8_t state = v[0], code = v[1];
    if (state == 0xFE || state == 0xFF) return;
    const auto *p = profiles_.find(code);
    const char *process = process_(code);
    if (!p && !process) return;
    operation_seen_ = true;
    operation_finished_ = state == 0x3E;
    last_tv_ = now_;
    phase_ = phase_name_(state);
    text("current_product", p ? p->name : process);
    text("progress_state", phase_);
    number("product_code", code);
    number("stage_code", state);
    binary("brew_active", operation_finished_ ? 0 : 1);
    const size_t window = v.size() > 2 && v[2] == 0x8F ? 3 : 2;
    number("progress", operation_finished_
                           ? 100
                           : (v.size() > window + 12 && v[window + 12] <= 100 ? float(v[window + 12]) : NAN));
    clear_stage_();
    size_t pair = 99;
    float factor = 1;
    std::string unit;
    if (state == 0x39 || state == 0x31) {
      pair = 0;
      unit = "level";
    } else if (state == 0x3C || state == 0x37) {
      pair = 2;
      factor = 5;
      unit = "ml";
    } else if (state == 0x32) {
      pair = 4;
      unit = "s";
    } else if (state == 0x34) {
      pair = 6;
      unit = "s";
    } else if (state == 0x33) {
      pair = 9;
      unit = "s";
    } else if (state == 0x41) {
      pair = p && profiles_.option_for(*p, BYPASS).valid() ? 6 : 2;
      factor = 5;
      unit = "ml";
    }
    if (pair != 99 && v.size() > window + pair + 1) {
      number("stage_current", v[window + pair] == 0xFF ? NAN : v[window + pair] * factor);
      number("stage_target", v[window + pair + 1] == 0xFF ? NAN : v[window + pair + 1] * factor);
      text("stage_unit", unit);
    }
    if (brew_pending_ && !brew_started_) {
      if (code == requested_code_) {
        brew_started_ = true;
        text("command_result", "Started");
      } else {
        brew_pending_ = false;
        brew_sent_ = false;
        text("command_result", "Different operation observed; request not confirmed");
      }
      if (active_ == Kind::BREW && transport_.waiting()) transport_.complete(true, now_);
    }
    if (operation_finished_) {
      if (brew_pending_ && code == requested_code_) text("command_result", "Completed");
      brew_pending_ = false;
      brew_sent_ = false;
      awaiting_tf_ = true;
      tf_requested_ = true;
      finished_at_ = now_;
      refresh_after_finish_ = true;
    }
    update_status_();
  }
  void parse_statistics_(const std::string &frame) {
    if (!transport_.waiting()) return;
    if (active_ == Kind::PRODUCTS) {
      const std::string prefix = "@tr:32," + active_command_.substr(7, 2) + ",";
      if (!starts_(frame, prefix)) return;
      const auto data = frame.substr(prefix.size());
      if (data.size() != 16 || !hex_(data)) return;
      unsigned page = hexval_(active_command_.substr(7, 2));
      for (unsigned i = 0; i < 4 && page * 4 + i < 256; ++i)
        counters_[page * 4 + i] = uint16_t(hexval_(data.substr(i * 4, 4)));
      number("total_products", counters_[0] == 0xFFFF ? NAN : float(counters_[0]));
      for (const auto &p : profiles_.products) {
        unsigned slot = p.code;
        // The J.O.E. app has this EF545-only remap; preserve own-code fallback.
        if (profiles_.ef == "EF545" && (p.code == 0x31 || p.code == 0x36)) {
          const unsigned alt = p.code == 0x31 ? 0x12 : 0x13;
          if (counters_[alt] != 0xFFFF) slot = alt;
        }
        number(key_("product_", p.code), counters_[slot] == 0xFFFF ? NAN : float(counters_[slot]));
      }
      transport_.complete(true, now_);
    } else if (active_ == Kind::MAINTENANCE && starts_(frame, "@tg:43")) {
      publish_maintenance_(frame.substr(6), false);
    } else if (active_ == Kind::PERCENT && starts_(frame, "@tg:C0"))
      publish_maintenance_(frame.substr(6), true);
  }
  void publish_maintenance_(const std::string &data, bool percent) {
    const unsigned width = percent ? 2 : 4;
    const size_t count =
        percent ? profiles_.maintenance_percentages.size() : profiles_.maintenance_counts.size();
    if (!hex_(data) || data.size() % width || data.size() < count * width) return;
    for (size_t i = 0; i < count; ++i) {
      const unsigned value = hexval_(data.substr(i * width, width));
      number(key_(percent ? "maintenance_percent_" : "maintenance_count_", unsigned(i)),
             value == (percent ? 0xFFu : 0xFFFFu) ? NAN : float(value));
    }
    transport_.complete(true, now_);
  }
  void publish_raw_(const std::string &data) {
    if (profiles_.ef != "EF533") return;
    std::vector<long> values;
    for (size_t i = 0; i + 4 <= data.size(); i += 4) values.push_back(long(hexval_(data.substr(i, 4))));
    std::string changes;
    for (size_t i = 0; i < values.size(); ++i) {
      number(key_("raw_", unsigned(i + 1)), float(values[i]));
      if (i < last_raw_.size() && values[i] != last_raw_[i]) {
        if (!changes.empty()) changes += ", ";
        changes += "counter_" + std::to_string(i + 1) + " " + std::to_string(last_raw_[i]) + " -> " +
                   std::to_string(values[i]);
      }
    }
    if (!changes.empty()) text("counters_changed", changes.substr(0, 250));
    last_raw_ = values;
    const std::pair<unsigned, const char *> extras[] = {
        {1, "single_espresso_made"},   {2, "double_espresso_made"}, {11, "brew_movements"},
        {14, "brews_since_descaling"}, {15, "grounds_level"},       {16, "brews_since_cleaning"}};
    for (const auto &x : extras)
      if (values.size() >= x.first) number(x.second, float(values[x.first - 1]));
  }

  Profiles profiles_;
  JuraTransport transport_;
  Session session_{Session::OFF};
  Kind active_{Kind::NONE}, handshake_kind_{Kind::H0};
  std::string active_command_, handshake_command_, detected_, response_override_, profile_hint_, phase_;
  std::deque<Request> polls_, control_;
  std::array<int32_t, ATTRIBUTE_COUNT> selected_;
  std::array<uint16_t, 256> counters_;
  size_t selected_product_{0};
  bool allow_brew_{false}, allow_power_{false}, raw_extras_{true}, discovery_done_{false},
      identify_pending_{false};
  bool tf_valid_{false}, tf_requested_{false}, awaiting_tf_{false}, operation_seen_{false},
      operation_finished_{false};
  bool alerts_invalidated_{false}, model_mismatch_{false};
  bool brew_pending_{false}, brew_sent_{false}, brew_started_{false}, refresh_after_finish_{false};
  uint32_t now_{0}, boot_{0}, startup_delay_{8000}, session_started_{0}, session_failed_at_{0};
  uint32_t last_activity_{0}, last_tf_{0}, last_tf_query_{0}, last_tv_{0}, brew_sent_at_{0}, finished_at_{0};
  uint32_t timeouts_{0}, framing_errors_{0};
  std::vector<uint8_t> tf_;
  std::vector<long> last_raw_;
  uint8_t requested_code_{0};
};
}  // namespace jura_native
}  // namespace esphome
