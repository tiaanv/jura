#pragma once
#include <map>
#include <numeric>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "jura_controller.h"

namespace esphome {
namespace jura_native {
// Slots are registered by codegen. Names/visibility are finalised before API setup.
template <class Base>
class ProfileEntity : public Base {
 public:
  void describe(const std::string &name, bool show) {
    owned_name_ = name;
    this->name_ = StringRef(owned_name_.c_str(), owned_name_.size());
    this->flags_.has_own_name = true;
    this->set_internal(!show);
  }

 private:
  std::string owned_name_;
};
using ProfileSensor = ProfileEntity<sensor::Sensor>;
using ProfileBinarySensor = ProfileEntity<binary_sensor::BinarySensor>;
class JuraNative;
class CoolcontrolOverrideSwitch : public switch_::Switch {
 public:
  void set_parent(JuraController *parent) { parent_ = parent; }
  void restore_state() {
    if (get_initial_state_with_restore_mode().value_or(false)) turn_on();
    else turn_off();
  }
 protected:
  void write_state(bool value) override {
    parent_->set_coolcontrol_override(value);
    publish_state(parent_->coolcontrol_override());
  }
  JuraController *parent_{nullptr};
};
class RecipeSelect : public select::Select {
 public:
  void set_parent(JuraNative *parent, int role) {
    parent_ = parent;
    role_ = role;
  }
  void options(std::vector<std::string> values) {
    labels_ = std::move(values);
    FixedVector<const char *> pointers;
    pointers.init(labels_.size());
    for (const auto &label : labels_) pointers.push_back(label.c_str());
    this->traits.set_options(pointers);
  }

 protected:
  void control(const std::string &value) override;
  JuraNative *parent_{nullptr};
  int role_{-1};
  std::vector<std::string> labels_;
};
class RecipeNumber : public number::Number {
 public:
  void set_parent(JuraNative *parent, int role) {
    parent_ = parent;
    role_ = role;
  }

 protected:
  void control(float value) override;
  JuraNative *parent_{nullptr};
  int role_{-1};
};
class CommandButton : public button::Button {
 public:
  void set_parent(JuraNative *parent, int role) {
    parent_ = parent;
    role_ = role;
  }

 protected:
  void press_action() override;
  JuraNative *parent_{nullptr};
  int role_{0};
};

class JuraNative : public PollingComponent, public uart::UARTDevice, public JuraController {
 public:
  void set_allow_brew(bool v) { allow_brew_ = v; }
  void set_allow_power_off(bool v) { allow_power_ = v; }
  void set_raw_extras(bool v) { raw_extras_ = v; }
  void set_startup_delay(uint32_t v) { startup_delay_ = v; }
  void set_handshake_response(const std::string &v) { response_override_ = v; }
  void set_profile_hint(const std::string &v) { profile_hint_ = v; }
  void set_coolcontrol(text_sensor::TextSensor *raw, sensor::Sensor *counter, binary_sensor::BinarySensor *online,
                       uint32_t timeout, uint32_t coalesce, const std::string &reporting = "auto") {
    cooler_raw_ = raw;
    cooler_counter_ = counter;
    cooler_online_ = online;
    cooler_timeout_ = timeout;
    cooler_coalesce_ = coalesce;
    configure_coolcontrol(timeout, coalesce, reporting);
    cooler_reporting_ = reporting;
  }
  void set_coolcontrol_override_switch(CoolcontrolOverrideSwitch *value) {
    cooler_override_switch_ = value;
    value->set_parent(this);
  }
  void register_sensor(const std::string &key, sensor::Sensor *s) { sensors_[key] = s; }
  void register_text(const std::string &key, text_sensor::TextSensor *s) { texts_[key] = s; }
  void register_binary(const std::string &key, binary_sensor::BinarySensor *s) { binaries_[key] = s; }
  void register_profile_sensor(const std::string &key, ProfileSensor *s) {
    dynamic_sensors_[key] = s;
    register_sensor(key, s);
  }
  void register_alert(unsigned bit, ProfileBinarySensor *s) {
    alert_sensors_[bit] = s;
    register_binary(key_("alert_", bit), s);
  }
  void register_select(int role, RecipeSelect *s) { selects_[role] = s; }
  void register_number(int role, RecipeNumber *s) { numbers_[role] = s; }
  void register_button(int role, CommandButton *s) { buttons_[role] = s; }
  float get_setup_priority() const override { return setup_priority::DATA; }
  bool can_proceed() override { return discovery_ready(); }
  void setup() override {
    if (cooler_override_switch_) cooler_override_switch_->restore_state();
    begin(millis());
    if (cooler_raw_ && cooler_counter_ && cooler_online_)
      ESP_LOGD("jura_native", "Cool Control bridge enabled: reporting=%s, timeout=%u ms, coalesce=%u ms",
               cooler_reporting_.c_str(), unsigned(cooler_timeout_), unsigned(cooler_coalesce_));
  }
  void loop() override {
    uint8_t raw;
    for (size_t i = 0; i < 128 && available(); ++i) {
      if (!read_byte(&raw)) break;
      input(raw, millis());
    }
    if (cooler_raw_ && cooler_counter_ && cooler_online_) {
      const bool available = cooler_raw_->has_state() && cooler_counter_->has_state() && cooler_online_->has_state();
      const uint32_t now = millis();
      log_coolcontrol_inputs_(now);
      coolcontrol_snapshot(cooler_raw_->state, cooler_counter_->state, available && cooler_online_->state, now);
    }
    advance(millis());
  }
  void update() override { refresh(); }
  void select_value(int role, const std::string &value) {
    if (role < 0) {
      choose(value);
      return;
    }
    const auto *p = selection();
    if (!p) return;
    const auto opt = profile().option_for(*p, uint8_t(role));
    for (const auto &item : opt.choices)
      if (item.name == value) {
        set_parameter(uint8_t(role), item.value);
        return;
      }
    reject_("Choice unavailable for selected drink");
    recipe_changed();
  }
  void number_value(int role, float value) {
    if (!std::isfinite(value) || std::floor(value) != value) {
      reject_("Recipe values must be whole numbers");
      recipe_changed();
      return;
    }
    if (!set_parameter(uint8_t(role), int32_t(value))) recipe_changed();
  }
  void press(int role) {
    if (role == 0)
      brew_selected();
    else if (role == 1)
      reset_recipe();
    else if (role == 2)
      refresh();
    else if (role == 3)
      power_off();
    else if (role == 4)
      probe_espresso_settings();
  }

 protected:
  CoolcontrolOverrideSwitch *cooler_override_switch_{nullptr};
  void log_coolcontrol_inputs_(uint32_t now) {
    const uint8_t availability = (cooler_raw_->has_state() ? 1 : 0) |
                                 (cooler_counter_->has_state() ? 2 : 0) |
                                 (cooler_online_->has_state() ? 4 : 0);
    const std::string raw = (availability & 1) ? cooler_raw_->state : "<missing>";
    const float counter = cooler_counter_->state;
    const bool online = (availability & 4) && cooler_online_->state;
    const bool counter_valid = (availability & 2) && std::isfinite(counter) && counter >= 0 &&
                               counter <= 16777215 && std::floor(counter) == counter;
    const bool important_change = !cooler_inputs_logged_ || availability != cooler_logged_availability_ ||
                                  raw != cooler_logged_raw_ || online != cooler_logged_online_ ||
                                  counter_valid != cooler_logged_counter_valid_;
    const bool counter_changed = counter != cooler_logged_counter_ &&
                                 !(std::isnan(counter) && std::isnan(cooler_logged_counter_));
    const bool periodic = uint32_t(now - cooler_inputs_logged_at_) >= 15000;
    if (!important_change && !periodic && !(counter_changed && uint32_t(now - cooler_inputs_logged_at_) >= 5000)) return;
    const char *state = availability != 7 ? "waiting for imported entity states"
                        : !online ? "source offline"
                        : !CoolcontrolBridge::valid(raw) ? "rejected: invalid raw message"
                        : !counter_valid ? "rejected: invalid message counter"
                        : "received valid inputs";
    const std::string counter_text = (availability & 2) ? std::to_string(counter) : "<missing>";
    ESP_LOGD("jura_native", "Cool Control HA inputs: raw='%s', counter=%s, online=%s; %s",
             raw.substr(0, 32).c_str(), counter_text.c_str(),
             (availability & 4) ? (online ? "ON" : "OFF") : "<missing>", state);
    ESP_LOGD("jura_native", "Cool Control bridge state: %s; %s",
             cooler_.reporting_description().c_str(), cooler_.input_description().c_str());
    cooler_inputs_logged_ = true;
    cooler_logged_availability_ = availability;
    cooler_logged_raw_ = raw;
    cooler_logged_online_ = online;
    cooler_logged_counter_valid_ = counter_valid;
    cooler_logged_counter_ = counter;
    cooler_inputs_logged_at_ = now;
  }
  void trace(const std::string &message) override { ESP_LOGD("jura_native", "%s", message.c_str()); }
  uint32_t write_transport(const uint8_t *data, size_t n) override {
    write_array(data, n);
    return millis();
  }
  void text(const std::string &key, const std::string &value) override {
    const auto it = texts_.find(key);
    if (it == texts_.end()) return;
    const std::string limited = value.substr(0, 250);
    if (it->second->has_state() && it->second->state == limited) return;
    it->second->publish_state(limited);
  }
  void number(const std::string &key, float value) override {
    const auto it = sensors_.find(key);
    if (it == sensors_.end()) return;
    auto *s = it->second;
    if (s->has_state() && (s->state == value || (std::isnan(s->state) && std::isnan(value)))) return;
    s->publish_state(value);
  }
  void binary(const std::string &key, int value) override {
    const auto it = binaries_.find(key);
    if (it == binaries_.end()) return;
    if (value < 0) {
      it->second->invalidate_state();
      return;
    }
    if (it->second->has_state() && it->second->state == (value != 0)) return;
    it->second->publish_state(value != 0);
  }
  void profile_changed(bool supported) override {
    for (auto &entry : buttons_)
      entry.second->set_internal(entry.first == 3  ? !allow_power_
                                 : entry.first < 2 ? (!supported || !allow_brew_)
                                                   : false);
    for (auto &entry : dynamic_sensors_) entry.second->describe(entry.first, false);
    for (auto &entry : alert_sensors_) entry.second->describe("Alert " + std::to_string(entry.first), false);
    if (!supported) {
      for (auto &s : selects_) {
        s.second->options({"Unavailable"});
        s.second->set_internal(true);
      }
      for (auto &n : numbers_) n.second->set_internal(true);
      return;
    }
    const auto &prof = profile();
    for (const auto &p : prof.products) {
      auto it = dynamic_sensors_.find(key_("product_", p.code));
      if (it != dynamic_sensors_.end()) it->second->describe(p.name + " Count", p.flags & 1);
    }
    for (const auto &a : prof.alerts) {
      auto it = alert_sensors_.find(a.bit);
      if (it != alert_sensors_.end()) it->second->describe(a.name, true);
    }
    for (size_t i = 0; i < prof.maintenance_counts.size(); ++i) {
      auto it = dynamic_sensors_.find(key_("maintenance_count_", unsigned(i)));
      if (it != dynamic_sensors_.end()) it->second->describe(prof.maintenance_counts[i] + " Count", true);
    }
    for (size_t i = 0; i < prof.maintenance_percentages.size(); ++i) {
      auto it = dynamic_sensors_.find(key_("maintenance_percent_", unsigned(i)));
      if (it != dynamic_sensors_.end())
        it->second->describe(prof.maintenance_percentages[i] + " Maintenance", true);
    }
    std::vector<std::string> products;
    for (const auto &p : prof.products)
      if (p.flags & 1) products.push_back(p.name);
    selects_[-1]->options(products.empty() ? std::vector<std::string>{"Unavailable"} : products);
    for (int attr : {int(STRENGTH), int(GRINDER), int(TEMPERATURE)}) {
      std::vector<std::string> choices{"Not available"};
      for (const auto &p : prof.products)
        if (p.flags & 1) {
          auto o = prof.option_for(p, uint8_t(attr));
          for (const auto &c : o.choices)
            if (std::find(choices.begin(), choices.end(), c.name) == choices.end()) choices.push_back(c.name);
        }
      auto it = selects_.find(attr);
      if (it != selects_.end()) {
        it->second->options(choices);
        it->second->set_internal(choices.size() == 1);
      }
    }
    for (auto &entry : numbers_) {
      bool any = false;
      float low = 0, high = 0, step = 1;
      for (const auto &p : prof.products)
        if (p.flags & 1) {
          const auto o = prof.option_for(p, uint8_t(entry.first));
          if (!o.valid() || !o.choices.empty()) continue;
          if (!any) {
            low = o.min;
            high = o.max;
            step = o.step;
            any = true;
          } else {
            const unsigned delta = unsigned(std::abs(int(low) - int(o.min)));
            step = float(std::gcd(std::gcd(unsigned(step), unsigned(o.step)), delta));
            low = std::min(low, float(o.min));
            high = std::max(high, float(o.max));
          }
        }
      entry.second->set_internal(!any);
      if (any) {
        entry.second->traits.set_min_value(low);
        entry.second->traits.set_max_value(high);
        entry.second->traits.set_step(step);
      }
    }
    if (!allow_brew_) {
      for (auto &s : selects_) s.second->set_internal(true);
      for (auto &n : numbers_) n.second->set_internal(true);
    }
    if (prof.ef != "EF533" || !raw_extras_) {
      for (auto &s : dynamic_sensors_)
        if (s.first.compare(0, 4, "raw_") == 0 || s.first == "single_espresso_made" ||
            s.first == "double_espresso_made" || s.first == "brew_movements" || s.first == "grounds_level" ||
            s.first == "brews_since_cleaning" || s.first == "brews_since_descaling")
          s.second->set_internal(true);
    } else {
      const std::pair<const char *, const char *> names[] = {
          {"single_espresso_made", "Single Espresso Made"},
          {"double_espresso_made", "Double Espresso Made"},
          {"brew_movements", "Legacy Coffee Rinse Count (unverified)"},
          {"grounds_level", "Grounds Level"},
          {"brews_since_cleaning", "Brews Performed Since Cleaning"},
          {"brews_since_descaling", "Brews Performed Since Descaling"}};
      for (const auto &item : names) {
        auto it = dynamic_sensors_.find(item.first);
        if (it != dynamic_sensors_.end()) it->second->describe(item.second, true);
      }
      for (unsigned i = 1; i <= 16; ++i) {
        auto it = dynamic_sensors_.find(key_("raw_", i));
        if (it != dynamic_sensors_.end()) it->second->describe("Raw Counter " + std::to_string(i), true);
      }
    }
  }
  void recipe_changed() override {
    const auto *p = selection();
    if (!p) return;
    if (selects_.count(-1)) selects_[-1]->publish_state(p->name);
    for (const auto &entry : selects_)
      if (entry.first >= 0) {
        const auto opt = profile().option_for(*p, uint8_t(entry.first));
        std::string label = "Not available";
        for (const auto &c : opt.choices)
          if (c.value == selected_[entry.first]) label = c.name;
        entry.second->publish_state(label);
        std::string choices;
        for (const auto &c : opt.choices) {
          if (!choices.empty()) choices += " | ";
          choices += c.name;
        }
        text(entry.first == STRENGTH      ? "strength_choices"
             : entry.first == TEMPERATURE ? "temperature_choices"
                                          : "grinder_choices",
             choices);
      }
    for (const auto &entry : numbers_) {
      const auto opt = profile().option_for(*p, uint8_t(entry.first));
      entry.second->publish_state(opt.valid() ? float(selected_[entry.first]) : NAN);
      const char *prefix = entry.first == WATER    ? "water"
                           : entry.first == MILK   ? "milk"
                           : entry.first == FOAM   ? "foam"
                           : entry.first == BYPASS ? "bypass"
                                                   : "pause";
      number(std::string(prefix) + "_min", opt.valid() ? float(opt.min) : NAN);
      number(std::string(prefix) + "_max", opt.valid() ? float(opt.max) : NAN);
      number(std::string(prefix) + "_step", opt.valid() ? float(opt.step) : NAN);
    }
  }
  std::string cooler_reporting_{"auto"};
  text_sensor::TextSensor *cooler_raw_{nullptr};
  sensor::Sensor *cooler_counter_{nullptr};
  binary_sensor::BinarySensor *cooler_online_{nullptr};
  uint32_t cooler_timeout_{15000}, cooler_coalesce_{250}, cooler_inputs_logged_at_{0};
  uint8_t cooler_logged_availability_{0};
  bool cooler_inputs_logged_{false}, cooler_logged_online_{false}, cooler_logged_counter_valid_{false};
  float cooler_logged_counter_{NAN};
  std::string cooler_logged_raw_;
  std::map<std::string, sensor::Sensor *> sensors_;
  std::map<std::string, text_sensor::TextSensor *> texts_;
  std::map<std::string, binary_sensor::BinarySensor *> binaries_;
  std::map<std::string, ProfileSensor *> dynamic_sensors_;
  std::map<unsigned, ProfileBinarySensor *> alert_sensors_;
  std::map<int, RecipeSelect *> selects_;
  std::map<int, RecipeNumber *> numbers_;
  std::map<int, CommandButton *> buttons_;
};
inline void RecipeSelect::control(const std::string &value) { parent_->select_value(role_, value); }
inline void RecipeNumber::control(float value) { parent_->number_value(role_, value); }
inline void CommandButton::press_action() { parent_->press(role_); }
}  // namespace jura_native
}  // namespace esphome
