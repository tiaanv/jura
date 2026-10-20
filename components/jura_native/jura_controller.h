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
#include "jura_coolcontrol_bridge.h"

namespace esphome {
namespace jura_native {
class JuraController : public TransportSink {
 public:
  JuraController() : transport_(*this) {
    selected_.fill(-1);
    baseline_.fill(-1);
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
  void configure_coolcontrol(uint32_t timeout, uint32_t coalesce, const std::string &reporting = "auto") {
    cooler_.configure(timeout, coalesce, reporting == "legacy" ? CoolcontrolBridge::ReportingMode::LEGACY
                                      : reporting == "negotiated" ? CoolcontrolBridge::ReportingMode::NEGOTIATED
                                                                  : CoolcontrolBridge::ReportingMode::AUTO);
  }
  void coolcontrol_snapshot(const std::string &raw, float counter, bool online, uint32_t now) {
    cooler_.snapshot(raw, counter, online, now);
  }
  void set_accept_short_status(bool v) { accept_short_status_ = v; }
  void begin(uint32_t now) {
    now_ = boot_ = now;
    identify_pending_ = true;
    text("status", "connecting");
    text("status_detail", "Identifying machine");
    text("session", "Starting");
    binary("connected", 0);
    text("command_result", "No command");
    text("command_recipe_source", "Not requested");
    text("command_recipe_status", "No API request");
    text("command_recipe_raw", "");
    text("espresso_probe_result", "Not run");
    text("espresso_probe_raw", "");
    text("espresso_probe_settings", "");
    clear_operation_("Unknown", "Unknown", -1);
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
    cooler_.advance(now);
    if (cooler_.enabled()) {
      const auto diagnostic = cooler_.reporting_description() + "; " + cooler_.input_description();
      if (diagnostic != cooler_diagnostic_) {
        cooler_diagnostic_ = diagnostic;
        trace("Cool Control bridge: " + diagnostic);
      }
    }
    const uint32_t boot_elapsed = uint32_t(now - boot_);
    // TY is optional: some machines reveal their model only in the AT T3 reply.
    // Keep API discovery gated while that first handshake can identify the profile.
    if (!discovery_done_) {
      if (boot_elapsed >= 8000 && !profile_hint_.empty() && profiles_.select(profile_hint_)) {
        install_profile_();
      } else if (boot_elapsed >= startup_delay_ && boot_elapsed - startup_delay_ >= 30000) {
        discovery_done_ = true;
        profile_changed(false);
        text("profile", "Identification timed out; configure profile_hint and restart");
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
      if (operation_finished_) {
        clear_operation_("Idle", "None", 0);
      } else {
        clear_operation_("Unknown (progress timeout)", "Unknown", -1);
        awaiting_tf_ = true;
        tf_requested_ = true;
      }
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
    if (!model_mismatch_ && uint32_t(now - boot_) >= startup_delay_ &&
        (session_ == Session::OFF ||
         (session_ == Session::BACKOFF && uint32_t(now - session_failed_at_) >= 60000))) {
      start_session_();
      return;
    }
    if (!control_.empty()) {
      const Request r = control_.front();
      control_.pop_front();
      if (r.kind == Kind::TEST_AN01 &&
          (session_ != Session::READY || model_mismatch_ || busy_() || brew_pending_ ||
           api_recipe_pending_ || probe_pending_)) {
        reject_("AN:01 test cancelled: connection or operation changed");
        return;
      }
      if (r.kind == Kind::API_RECIPE && (!api_recipe_pending_ || !ready_for_brew_(api_recipe_code_))) {
        cancel_api_recipe_("Machine no longer ready before settings read");
        return;
      }
      if (r.kind == Kind::RECIPE_PROBE && !can_probe_()) {
        probe_pending_ = false;
        const auto why = "Cancelled: " + probe_block_reason_();
        text("espresso_probe_result", why);
        trace("Espresso probe: " + why);
        return;
      }
      if (r.kind == Kind::BREW && (!ready_for_brew_(requested_code_) || !brew_pending_)) {
        brew_pending_ = false;
        text("command_result", "Rejected: machine no longer ready");
        return;
      }
      start_(r.command, r.kind, r.encrypted);
      if (r.kind == Kind::TEST_AN01)
        text("command_result", "AN:01 test sent; waiting for reply; effect unverified");
      if (r.kind == Kind::API_RECIPE) {
        text("command_result", "Reading saved settings before brew");
        text("command_recipe_status", "Reading machine settings");
      }
      if (r.kind == Kind::RECIPE_PROBE) text("espresso_probe_result", "Sent @TM:41,02; waiting for response");
      if (r.kind == Kind::BREW) {
        brew_sent_ = true;
        brew_sent_at_ = now;
        text("command_result", "Sent; waiting for machine");
      }
      return;
    }
    if (session_ != Session::READY) return;
    if (cooler_.take_announcement()) {
      trace("Cool Control announcement: @H1 (plaintext; fresh cooler available in this machine session)");
      start_("@H1", Kind::COOLER, false, false);
      return;
    }
    std::string cooler_command;
    bool cooler_requested = false;
    if (cooler_.next(now, cooler_command, &cooler_requested)) {
      if (cooler_requested) trace("Cool Control request response: " + cooler_command + " (plaintext; " + cooler_.input_description() + ")");
      // Milk status uses plaintext framing without the AT cipher envelope.
      start_(cooler_command, Kind::COOLER, false, false);
      return;
    }
    // TF is replaced by TV during a product. Don't query or restart a live TV session.
    if (!busy_() && (tf_requested_ || uint32_t(now - last_tf_query_) >= 30000)) {
      tf_requested_ = false;
      last_tf_query_ = now;
      start_("@TF:", Kind::TF, true);
      return;
    }
    if (busy_()) return;
    if (recipe_read_requested_ && !brew_pending_ && !api_recipe_pending_ && !probe_pending_ &&
        can_read_recipe_()) {
      const auto *p = selection();
      if (p) {
        recipe_read_requested_ = false;
        active_recipe_epoch_ = recipe_epoch_;
        active_recipe_code_ = p->code;
        recipe_read_status_ = "Reading saved recipe for " + p->name;
        publish_recipe_state_();
        start_("@TM:41," + hexbyte_(p->code), Kind::SAVED_RECIPE, true);
        return;
      }
    }
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
    ++recipe_epoch_;
    baseline_.fill(-1);
    if (const auto *p = selection())
      for (uint8_t i = 0; i < p->n_options; ++i) {
        const auto o = profiles_.option(*p, i);
        if (o.valid()) baseline_[o.attr] = o.def;
      }
    selected_ = baseline_;
    edited_mask_ = 0;
    recipe_source_ = "Profile defaults";
    recipe_read_status_ = "Waiting to read saved recipe";
    recipe_read_requested_ = recipe_loading_ = selection() != nullptr;
    text("saved_recipe_raw", "");
    publish_recipe_state_();
    recipe_changed();
  }
  bool set_parameter(uint8_t attr, int32_t value) {
    const auto *p = selection();
    if (!p || attr >= ATTRIBUTE_COUNT) return reject_("No selected product");
    const auto o = profiles_.option_for(*p, attr);
    if (!o.valid() || !o.accepts(value)) return reject_("Unsupported value for selected drink");
    selected_[attr] = value;
    edited_mask_ |= uint8_t(1u << attr);
    publish_recipe_state_();
    recipe_changed();
    return true;
  }
  bool brew_selected() {
    const auto *p = selection();
    if (!p) return reject_("No drink selected");
    if (recipe_loading_) return reject_("Saved recipe is still loading; wait for Recipe Loading to turn off");
    if (!submit_(*p, selected_.data())) return false;
    // submit_ freezes the one-shot wire recipe before clearing UI overrides.
    selected_ = baseline_;
    edited_mask_ = 0;
    publish_recipe_state_();
    recipe_changed();
    return true;
  }
  bool brew_named(const std::string &name) {
    std::array<int32_t, ATTRIBUTE_COUNT> values;
    values.fill(-1);
    return brew_recipe(name, values);
  }
  bool brew_recipe(const std::string &name, const std::array<int32_t, ATTRIBUTE_COUNT> &values) {
    const auto *p = profiles_.find(name);
    if (!p) return reject_("Unknown drink");
    if (!allow_brew_) return reject_("Brewing control disabled");
    if (brew_pending_ || api_recipe_pending_ || probe_pending_ || active_ == Kind::API_RECIPE ||
        !control_.empty())
      return reject_("Another command is pending");
    if (!ready_for_brew_()) return reject_("Machine not ready or status unavailable");
    const auto milk_reason = milk_block_reason_(p->code);
    if (!milk_reason.empty()) return reject_(milk_reason);
    // Validate explicit overrides now; omitted values are filled after the read.
    for (uint8_t attr = 0; attr < ATTRIBUTE_COUNT; ++attr) {
      if (values[attr] == -1) continue;
      const auto opt = profiles_.option_for(*p, attr);
      if (values[attr] < -1 || !opt.valid() || !opt.accepts(values[attr]))
        return reject_("Unsupported override for requested drink");
    }
    api_recipe_code_ = p->code;
    api_overrides_ = values;
    api_recipe_response_.clear();
    api_recipe_pending_ = brew_pending_ = true;
    brew_sent_ = brew_started_ = false;
    text("requested_product", p->name);
    text("command_recipe_source", "Pending");
    text("command_recipe_status", "Queued saved-settings read for " + p->name);
    text("command_recipe_raw", "");
    text("command_result", "Queued settings read before brew");
    control_.push_back({"@TM:41," + hexbyte_(p->code), Kind::API_RECIPE, true});
    return true;
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
  // Manual diagnostic only. Does not require a ready TF state: the intended
  // experiment is during SwitchOff Delay. Never send during an observed operation.
  bool test_an01() {
    if (session_ != Session::READY || model_mismatch_)
      return reject_("AN:01 test requires a connected AT session");
    if (busy_() || brew_pending_ || api_recipe_pending_ || probe_pending_ ||
        active_ == Kind::SAVED_RECIPE || active_ == Kind::TEST_AN01 || !control_.empty())
      return reject_("AN:01 test rejected: another operation or control is pending");
    control_.push_back({"AN:01", Kind::TEST_AN01, false});
    text("command_result", "AN:01 test queued; effect unverified; no automatic retry");
    return true;
  }
  bool power_off() {
    if (!allow_power_) return reject_("Power-off control disabled");
    if (brew_pending_ || api_recipe_pending_ || probe_pending_ || active_ == Kind::SAVED_RECIPE ||
        active_ == Kind::API_RECIPE || busy_() || !control_.empty())
      return reject_("Machine busy");
    control_.push_back({"AN:02", Kind::POWER, false});
    text("command_result", "Power off queued");
    return true;
  }
  // Deliberately fixed to a read request. No settings writes or recipe application.
  bool probe_espresso_settings() {
    std::string why = probe_block_reason_();
    if (why.empty() && brew_pending_) why = "brew command still pending";
    if (why.empty() && (api_recipe_pending_ || active_ == Kind::API_RECIPE))
      why = "API settings read in progress";
    if (why.empty() && probe_pending_) why = "espresso settings read already pending";
    if (why.empty() && !control_.empty()) why = "another control command queued";
    if (why.empty() && active_ == Kind::SAVED_RECIPE) why = "selected drink settings read in progress";
    if (!why.empty()) {
      text("espresso_probe_result", "Rejected: " + why);
      trace("Espresso probe rejected: " + why + "; last command=" + active_command_);
      return false;
    }
    probe_pending_ = true;
    text("espresso_probe_raw", "");
    text("espresso_probe_settings", "");
    text("espresso_probe_result", "Queued read-only espresso settings request");
    control_.push_back({"@TM:41,02", Kind::RECIPE_PROBE, true});
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
  enum class Kind {
    NONE,
    TYPE,
    H0,
    H1,
    H2,
    H3,
    TF,
    PRODUCTS,
    MAINTENANCE,
    PERCENT,
    RAW,
    BREW,
    POWER,
    TEST_AN01,
    RECIPE_PROBE,
    SAVED_RECIPE,
    API_RECIPE,
    COOLER
  };
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
  std::string probe_block_reason_() const {
    if (model_mismatch_) return "machine/profile changed; restart required";
    if (!profile_ok()) return "machine profile unavailable";
    if (!profiles_.find(uint8_t(2))) return "profile has no espresso product (02)";
    if (session_ != Session::READY) return "AT session not connected";
    if (busy_())
      return "machine operation active: " + phase_ + " (last progress " +
             std::to_string(uint32_t(now_ - last_tv_) / 1000) + " s ago)";
    if (awaiting_tf_) return "waiting for fresh TF status after operation";
    if (!tf_valid_) return "no valid TF status received";
    if (uint32_t(now_ - last_tf_) >= 60000) return "TF status is older than 60 seconds";
    return "";
  }
  bool can_probe_() const { return probe_block_reason_().empty(); }
  bool can_read_recipe_() const {
    return !model_mismatch_ && profile_ok() && selection() && session_ == Session::READY && !busy_() &&
           !awaiting_tf_ && tf_valid_ && uint32_t(now_ - last_tf_) < 60000;
  }
  void publish_recipe_state_() {
    text("recipe_source", recipe_source_);
    binary("recipe_loading", recipe_loading_ ? 1 : 0);
    binary("recipe_modified", edited_mask_ ? 1 : 0);
    text("recipe_status",
         edited_mask_ ? "Edited for next brew; " + recipe_read_status_ : recipe_read_status_);
  }
  void saved_recipe_fallback_(const std::string &why) {
    recipe_loading_ = recipe_read_requested_ = false;
    recipe_source_ = "Profile defaults";
    recipe_read_status_ = "Profile defaults: " + why;
    publish_recipe_state_();
    trace("Saved recipe fallback: " + why);
  }
  void receive_saved_recipe_(const std::string &frame) {
    if (active_recipe_epoch_ != recipe_epoch_) {
      trace("Ignoring saved recipe response for previous selection/reset");
      return;
    }
    text("saved_recipe_raw", frame);
    std::array<int32_t, ATTRIBUTE_COUNT> values;
    std::string summary, error;
    if (!decode_saved_recipe_(frame, active_recipe_code_, values, summary, error)) {
      saved_recipe_fallback_(error);
      return;
    }
    baseline_ = values;
    for (uint8_t attr = 0; attr < ATTRIBUTE_COUNT; ++attr)
      if (!(edited_mask_ & (1u << attr))) selected_[attr] = baseline_[attr];
    recipe_source_ = "Machine";
    recipe_loading_ = recipe_read_requested_ = false;
    recipe_read_status_ = "Saved recipe loaded for " + selection()->name;
    publish_recipe_state_();
    recipe_changed();
    trace("Saved recipe " + selection()->name + ": " + summary);
  }
  bool flag_(unsigned bit) const { return bit / 8 < tf_.size() && (tf_[bit / 8] & (0x80u >> (bit % 8))); }
  // Only the observed five-byte EF533 format may omit the sixth alert byte.
  // Shorter frames and other profiles retain the incomplete-status guard.
  // Missing alert entities stay unknown even when this exception is enabled.
  bool tf_missing_(unsigned bit) const {
    const bool accepted_short = accept_short_status_ && profiles_.ef == "EF533" && tf_.size() == 5;
    return bit / 8 >= tf_.size() && !accepted_short;
  }
  bool blocked_() const {
    for (const auto &a : profiles_.alerts)
      if (a.blocking && flag_(a.bit)) return true;
    return false;
  }
  std::string milk_block_reason_(uint8_t code) const {
    const auto *p = profiles_.find(code);
    if (!p || (!profiles_.option_for(*p, MILK).valid() && !profiles_.option_for(*p, FOAM).valid())) return "";
    for (const auto &a : profiles_.alerts) {
      const auto name = Profiles::lower(a.name);
      if (name != "no milk (milk sensor)" && name != "error milk (milk sensor)" &&
          name != "no signal (milk sensor)") continue;
      if (tf_missing_(a.bit)) return "Milk sensor status unavailable";
      if (flag_(a.bit)) return a.name;
    }
    return "";
  }
  bool ready_for_brew_(int product_code = -1) const {
    if (model_mismatch_ || !profile_ok() || session_ != Session::READY || !tf_valid_ || busy_() ||
        awaiting_tf_ || uint32_t(now_ - last_tf_) >= 60000 || blocked_())
      return false;
    if (product_code >= 0 && !milk_block_reason_(uint8_t(product_code)).empty()) return false;
    for (const auto &a : profiles_.alerts)
      if (a.blocking && tf_missing_(a.bit)) return false;
    for (const auto &a : profiles_.alerts)
      if (Profiles::lower(a.name) == "coffee ready") return flag_(a.bit);
    return false;  // No positive readiness signal: don't guess.
  }
  void cancel_api_recipe_(const std::string &why) {
    if (!api_recipe_pending_) return;
    api_recipe_pending_ = brew_pending_ = brew_sent_ = false;
    text("command_recipe_source", "Not used");
    text("command_recipe_status", "Cancelled: " + why);
    text("command_result", "Cancelled: " + why);
    trace("API recipe cancelled: " + why);
  }
  void resolve_api_recipe_(bool received_response) {
    if (!api_recipe_pending_) return;
    if (!ready_for_brew_(api_recipe_code_)) {
      cancel_api_recipe_("Machine no longer ready after settings read");
      return;
    }
    const auto *p = profiles_.find(api_recipe_code_);
    if (!p) {
      cancel_api_recipe_("Requested product unavailable");
      return;
    }
    std::array<int32_t, ATTRIBUTE_COUNT> values;
    values.fill(-1);
    std::string summary, error;
    bool loaded = received_response &&
                  decode_saved_recipe_(api_recipe_response_, api_recipe_code_, values, summary, error);
    if (!loaded) {
      values.fill(-1);  // All-or-nothing fallback; never use partially decoded values.
      if (!received_response) error = "Settings read timed out";
    }
    text("command_recipe_source", loaded ? "Machine" : "Profile defaults");
    text("command_recipe_status",
         loaded ? "Saved settings loaded for " + p->name : "Profile defaults: " + error);
    text("command_recipe_raw", received_response ? api_recipe_response_ : "");
    for (uint8_t attr = 0; attr < ATTRIBUTE_COUNT; ++attr)
      if (api_overrides_[attr] >= 0) values[attr] = api_overrides_[attr];
    api_recipe_pending_ = brew_pending_ = false;
    submit_(*p, values.data());  // Rechecks readiness, validates/encodes, freezes one brew.
  }
  bool submit_(const Product &product, const int32_t *values) {
    if (!allow_brew_) return reject_("Brewing control disabled");
    if (brew_pending_ || api_recipe_pending_ || probe_pending_ || active_ == Kind::SAVED_RECIPE ||
        active_ == Kind::API_RECIPE || !control_.empty())
      return reject_("Another command is pending");
    if (!ready_for_brew_()) return reject_("Machine not ready or status unavailable");
    const auto milk_reason = milk_block_reason_(product.code);
    if (!milk_reason.empty()) return reject_(milk_reason);
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
      session_failed_(profile_ok()
                          ? "Machine/profile changed: restart to update native entities"
                          : "Model identified after discovery closed: configure profile_hint and restart");
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
    cooler_.set_model(profiles_.ef);
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
    cooler_.reset_session();
    session_ = Session::ACK0;
    session_started_ = now_;
    handshake_command_ = "@T0";
    handshake_kind_ = Kind::H0;
    tf_valid_ = false;
    tf_.clear();
    clear_operation_("Unknown (AT reconnecting)", "Unknown", -1);
    binary("connected", 0);
    text("session", "Handshake: T0");
    update_status_();
  }
  void session_failed_(const std::string &why) {
    cancel_api_recipe_("Connection lost during settings read");
    session_ = Session::BACKOFF;
    session_failed_at_ = now_;
    handshake_command_.clear();
    polls_.clear();
    control_.clear();
    if (active_ == Kind::SAVED_RECIPE && active_recipe_epoch_ == recipe_epoch_ && recipe_loading_)
      saved_recipe_fallback_("Connection lost; select drink or Reset Recipe to retry");
    if (probe_pending_) text("espresso_probe_result", "Connection lost; probe not retried");
    probe_pending_ = false;
    tf_valid_ = false;
    brew_pending_ = false;
    brew_sent_ = false;
    binary("connected", 0);
    clear_operation_("Unknown (AT disconnected)", "Unknown", -1);
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
      if (a.blocking && tf_missing_(a.bit)) {
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
    if (kind == Kind::API_RECIPE) resolve_api_recipe_(success);
    if (kind == Kind::SAVED_RECIPE && !success && active_recipe_epoch_ == recipe_epoch_ && recipe_loading_)
      saved_recipe_fallback_("Settings read timed out; select drink or Reset Recipe to retry");
    if (kind == Kind::RECIPE_PROBE) {
      probe_pending_ = false;
      if (!success) text("espresso_probe_result", "Timeout: no settings response; not retried");
    }
    if (!success) {
      trace("Timeout: " + active_command_);
      number("timeouts", float(++timeouts_));
      if (kind == Kind::H0 || kind == Kind::H1 || kind == Kind::H2 || kind == Kind::H3)
        session_failed_("Handshake command timeout");
      else if (kind == Kind::BREW) {
        text("command_result", "Awaiting start; no immediate reply (not retried)");
      } else if (kind == Kind::TEST_AN01)
        text("command_result", "AN:01 test reply timeout; effect unknown; not retried");
      else if (kind == Kind::POWER)
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
    // Accept HM requests with plaintext or encrypted framing.
    if (encrypted || frame == "@HM" || starts_(frame, "@HM:")) {
      receive_at_(frame);
      return;
    }
    if (starts_(frame, "ty:")) identify_(frame.substr(3));
    if (!transport_.waiting()) return;
    if (active_ == Kind::TYPE && starts_(frame, "ty:") && frame.size() > 3)
      transport_.complete(true, now_);
    else if (active_ == Kind::TEST_AN01 && (starts_(frame, "an:") || starts_(frame, "ok:"))) {
      text("command_result", "AN:01 test response: " + frame + "; effect unverified");
      transport_.complete(true, now_);
    } else if (active_ == Kind::POWER && starts_(frame, "an:")) {
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
    // HM may follow T3 before our final t3 acknowledgement has drained.
    if (cooler_.enabled() && session_ != Session::OFF && session_ != Session::BACKOFF &&
        (frame == "@HM" || (frame.size() == 6 && starts_(frame, "@HM:") && hex_(frame.substr(4))))) {
      if (frame == "@HM") {
        cooler_.request_status();
      } else {
        cooler_.negotiate(uint8_t(hexval_(frame.substr(4))));
        trace("Cool Control reporting flags: " + frame.substr(4));
      }
      trace("Cool Control status request: " + frame + "; cached response queued");
      return;
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
      // Select known accessory reply formats by byte length, not challenge contents.
      // Keep explicit overrides authoritative; unsupported lengths require one.
      if (response.empty()) {
        if (challenge.size() == 2)
          response = "81";
        else if (challenge.size() == 6)
          response = "818000";
        else if (challenge.size() == 10)
          response = "8120000000";
      }
      if (cooler_.enabled() && response_override_.empty() && !response.empty() &&
          hex_(challenge) && challenge.size() % 2 == 0) {
        // Acknowledge byte0, retain byte1 bit7 and advertise cooler capability in bit5.
        response = hexbyte_(uint8_t(hexval_(challenge.substr(0, 2))) | 0x80);
        if (challenge.size() >= 4) {
          response += hexbyte_((uint8_t(hexval_(challenge.substr(2, 2))) & 0x80) | 0x20);
          response.append(challenge.size() - 4, '0');
        }
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
      if (active_ == Kind::API_RECIPE && transport_.waiting() &&
          (starts_(frame, "@tm:") || starts_(frame, "@an:error"))) {
        api_recipe_response_ = frame;
        text("command_recipe_raw", frame);
        transport_.complete(true, now_);
      } else if (active_ == Kind::SAVED_RECIPE && transport_.waiting() &&
                 (starts_(frame, "@tm:") || starts_(frame, "@an:error"))) {
        receive_saved_recipe_(frame);
        transport_.complete(true, now_);
      } else if (active_ == Kind::RECIPE_PROBE && transport_.waiting() &&
                 (starts_(frame, "@tm:") || starts_(frame, "@an:error"))) {
        parse_espresso_probe_(frame);
        transport_.complete(true, now_);
      } else if (starts_(frame, "@TF:")) {
        if (parse_tf_(frame) && active_ == Kind::TF && transport_.waiting()) transport_.complete(true, now_);
      } else if (starts_(frame, "@TV:")) {
        parse_tv_(frame);
      } else if (frame == "@TB") {
        cancel_api_recipe_("Machine operation began during settings read");
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
      } else if (frame == "@tp" && brew_pending_ && brew_sent_) {
        text("command_result", "Accepted; waiting for start");
        if (active_ == Kind::BREW && transport_.waiting()) transport_.complete(true, now_);
      } else if (starts_(frame, "@tp:") && brew_pending_ && brew_sent_) {
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
    // A completed or expired operation is not the current product. Preserve
    // actively arriving TV progress even if an incidental TF frame also arrives.
    bool complete_status = tf_.size() >= 2;
    for (const auto &a : profiles_.alerts)
      if (a.blocking && tf_missing_(a.bit)) complete_status = false;
    if (!busy_() && complete_status) clear_operation_("Idle", "None", 0);
    if (api_recipe_pending_ && !ready_for_brew_(api_recipe_code_))
      cancel_api_recipe_("Machine reported not ready during settings read");
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
  void clear_operation_(const char *state, const char *product, int active) {
    operation_seen_ = false;
    operation_finished_ = false;
    phase_.clear();
    text("current_product", product);
    text("progress_state", state);
    binary("brew_active", active);
    number("product_code", NAN);
    number("stage_code", NAN);
    number("progress", NAN);
    clear_stage_();
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
    cancel_api_recipe_("Machine progress observed during settings read");
    // Milk sensor progress states indicate a
    // blocked attempt, not dispensing progress or successful completion.
    if (state >= 0x2D && state <= 0x2F) {
      const std::string reason = state == 0x2D ? "no milk" : state == 0x2E ? "milk cooler error" : "no milk cooler signal";
      const std::string result = "Blocked: " + reason;
      clear_operation_(result.c_str(), p ? p->name.c_str() : process, 0);
      number("product_code", code);
      number("stage_code", state);
      if (brew_pending_) {
        text("command_result", code == requested_code_ ? result : "Different operation blocked: " + reason);
      }
      brew_pending_ = brew_sent_ = brew_started_ = false;
      if (active_ == Kind::BREW && transport_.waiting()) transport_.complete(true, now_);
      awaiting_tf_ = tf_requested_ = true;
      trace("Milk preparation blocked: " + reason + "; requesting fresh TF status");
      text("status", "blocked");
      text("status_detail", reason);
      return;
    }
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
  void parse_espresso_probe_(const std::string &frame) {
    text("espresso_probe_raw", frame);
    std::array<int32_t, ATTRIBUTE_COUNT> values;
    std::string decoded, error;
    const bool valid = decode_saved_recipe_(frame, 2, values, decoded, error);
    text("espresso_probe_settings", decoded);
    text("espresso_probe_result",
         valid ? "Valid response; compare decoded settings with machine screen" : error);
    trace("Espresso settings probe: " + decoded);
  }
  bool decode_saved_recipe_(const std::string &frame, uint8_t code,
                            std::array<int32_t, ATTRIBUTE_COUNT> &values, std::string &decoded,
                            std::string &error) const {
    values.fill(-1);
    if (Profiles::lower(frame) == "@tm:c1") {
      error = "Unsupported: machine returned @tm:C1";
      return false;
    }
    if (!starts_(frame, "@tm:41,")) {
      error = "Unexpected response; see raw recipe response";
      return false;
    }
    const auto body = frame.substr(7);
    if (!hex_(body) || body.size() < 4 || body.size() % 2 || body.size() > 130) {
      error = "Invalid response: malformed payload";
      return false;
    }
    const auto payload = body.substr(0, body.size() - 2);
    unsigned sum = 0;
    for (unsigned char c : "41," + payload) sum += c;
    if (((~sum) & 255u) != hexval_(body.substr(body.size() - 2))) {
      error = "Invalid response: checksum mismatch";
      return false;
    }
    if (hexval_(payload.substr(0, 2)) != code) {
      error = code == 2 ? "Invalid response: product is not espresso (02)"
                        : "Invalid response: product code mismatch";
      return false;
    }
    const Product *product = profiles_.find(code);
    if (!product) {
      error = "Product absent from profile";
      return false;
    }
    const char *labels[] = {"strength", "grinder", "temperature", "water", "milk", "foam", "bypass", "pause"};
    bool complete = true;
    for (uint8_t i = 0; i < product->n_options; ++i) {
      const auto opt = profiles_.option(*product, i);
      if (!opt.valid() || !opt.argument) continue;
      const size_t offset = size_t(opt.argument - 1) * 2;
      if (!decoded.empty()) decoded += "; ";
      decoded += labels[opt.attr];
      decoded += "=";
      if (offset + 2 > payload.size()) {
        decoded += "missing";
        complete = false;
        continue;
      }
      const unsigned raw = hexval_(payload.substr(offset, 2));
      const unsigned value = (opt.attr == WATER || opt.attr == BYPASS) ? raw * 5 : raw;
      if (raw == 255 || !opt.accepts(value)) {
        decoded += "unknown (F" + std::to_string(opt.argument) + "=" + hexbyte_(raw) + ")";
        complete = false;
      } else if (!opt.choices.empty()) {
        values[opt.attr] = value;
        for (const auto &choice : opt.choices)
          if (choice.value == value) decoded += choice.name;
      } else {
        values[opt.attr] = value;
        decoded += std::to_string(value);
        decoded += (opt.attr == WATER || opt.attr == BYPASS) ? " ml" : " s";
      }
    }
    if (!complete) error = "Checksum valid; incomplete or unknown settings; compare raw response";
    return complete;
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
        // EF545 uses alternate counter slots; preserve the product-code fallback.
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
  CoolcontrolBridge cooler_;
  std::string cooler_diagnostic_;
  Session session_{Session::OFF};
  Kind active_{Kind::NONE}, handshake_kind_{Kind::H0};
  std::string active_command_, handshake_command_, detected_, response_override_, profile_hint_, phase_;
  std::deque<Request> polls_, control_;
  std::array<int32_t, ATTRIBUTE_COUNT> selected_;
  std::array<int32_t, ATTRIBUTE_COUNT> baseline_;
  std::array<int32_t, ATTRIBUTE_COUNT> api_overrides_;
  std::string api_recipe_response_;
  uint8_t api_recipe_code_{0};
  bool api_recipe_pending_{false};
  std::string recipe_source_{"Profile defaults"}, recipe_read_status_;
  uint32_t recipe_epoch_{0}, active_recipe_epoch_{0};
  uint8_t edited_mask_{0}, active_recipe_code_{0};
  bool recipe_read_requested_{false}, recipe_loading_{false};
  std::array<uint16_t, 256> counters_;
  size_t selected_product_{0};
  bool allow_brew_{false}, allow_power_{false}, raw_extras_{true}, discovery_done_{false},
      identify_pending_{false};
  bool tf_valid_{false}, tf_requested_{false}, awaiting_tf_{false}, operation_seen_{false},
      operation_finished_{false};
  bool alerts_invalidated_{false}, model_mismatch_{false};
  bool accept_short_status_{false};
  bool probe_pending_{false};
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
