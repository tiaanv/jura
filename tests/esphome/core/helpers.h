#pragma once
#include <string>
#include <vector>
namespace esphome {
struct StringRef {
  const char *p = "";
  size_t n = 0;
  StringRef() = default;
  StringRef(const char *v, size_t len) : p(v), n(len) {}
  const char *c_str() const { return p; }
};
class Entity {
 public:
  bool has_state() const { return flags_.has_state; }
  void set_internal(bool x) { flags_.internal = x; }
  bool is_internal() const { return flags_.internal; }
  std::string name() const { return std::string(name_.p, name_.n); }

 protected:
  StringRef name_;
  struct {
    bool has_state = false, internal = false, has_own_name = false;
  } flags_;
};
template <class T>
struct FixedVector : std::vector<T> {
  void init(size_t n) {
    this->clear();
    this->reserve(n);
  }
};
}  // namespace esphome
