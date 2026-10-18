#pragma once
namespace esphome {
namespace setup_priority {
constexpr float DATA = 600;
}
class PollingComponent {
 public:
  virtual void setup() {}
  virtual void update() {}
  virtual void loop() {}
  virtual bool can_proceed() { return true; }
  virtual float get_setup_priority() const { return 600; }
};
}  // namespace esphome
