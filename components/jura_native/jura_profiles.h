#pragma once
// Flash-safe JPRF v4 reader. Layout/data originate from harryzz (MIT).
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "jura_profiles_blob.h"

namespace esphome {
namespace jura_native {
enum Attribute : uint8_t {
  STRENGTH,
  GRINDER,
  TEMPERATURE,
  WATER,
  MILK,
  FOAM,
  BYPASS,
  PAUSE,
  ATTRIBUTE_COUNT
};
struct Choice {
  uint8_t value;
  std::string name;
};
struct Option {
  uint8_t attr{255}, argument{0};
  uint16_t def{0}, min{0}, max{0}, step{1};
  std::vector<Choice> choices;
  bool valid() const { return attr < ATTRIBUTE_COUNT; }
  bool accepts(int32_t value) const {
    if (!choices.empty()) {
      for (const auto &item : choices)
        if (value == item.value) return true;
      return false;
    }
    return value >= min && value <= max && (value - min) % step == 0;
  }
};
struct Product {
  uint8_t code, flags, n_options;
  std::string name;
  uint32_t options;
};
struct Alert {
  uint16_t bit;
  bool blocking;
  std::string name;
};

class Profiles {
 public:
  static uint8_t byte(size_t offset) {
    if (offset >= sizeof(JURA_PROFILES)) return 0;
#ifdef USE_ESP8266
    return pgm_read_byte(JURA_PROFILES + offset);
#else
    return JURA_PROFILES[offset];
#endif
  }
  static uint16_t word(size_t p) { return uint16_t(byte(p)) | (uint16_t(byte(p + 1)) << 8); }
  static uint32_t dword(size_t p) { return uint32_t(word(p)) | (uint32_t(word(p + 2)) << 16); }
  static std::string string(uint16_t ref) {
    const size_t p = dword(8) + ref;
    std::string value;
    for (size_t i = p; i < sizeof(JURA_PROFILES) && byte(i); ++i) value += static_cast<char>(byte(i));
    return value;
  }
  bool select(const std::string &model) {
    if (word(4) != 4) return false;
    size_t best = 0;
    uint32_t record = 0;
    uint16_t family = 0;
    std::string selected;
    for (uint16_t i = 0; i < word(6); ++i) {
      const size_t p = 12 + i * 8;
      const std::string code = string(word(p));
      if (code.size() > best && model.compare(0, code.size(), code) == 0) {
        best = code.size();
        record = dword(p + 4);
        family = word(p + 2);
        selected = code;
      }
    }
    if (!record || record >= dword(8)) return false;
    ef = selected;
    name = string(family);
    products.clear();
    alerts.clear();
    maintenance_counts.clear();
    maintenance_percentages.clear();
    size_t p = record;
    const uint8_t n = byte(p++);
    for (uint8_t i = 0; i < n; ++i) {
      Product product{byte(p), byte(p + 3), byte(p + 4), string(word(p + 1)), uint32_t(p + 5)};
      p += 5;
      for (uint8_t j = 0; j < product.n_options; ++j) p += 11 + size_t(byte(p + 10)) * 3;
      products.push_back(product);
    }
    const uint8_t nc = byte(p++);
    for (uint8_t i = 0; i < nc; ++i, p += 2) maintenance_counts.push_back(string(word(p)));
    const uint8_t np = byte(p++);
    for (uint8_t i = 0; i < np; ++i, p += 2) maintenance_percentages.push_back(string(word(p)));
    const uint16_t na = word(p);
    p += 2;
    for (uint16_t i = 0; i < na; ++i, p += 4)
      alerts.push_back({uint16_t(word(p) & 0x7FFF), bool(word(p) & 0x8000), string(word(p + 2))});
    return true;
  }
  Option option(const Product &product, uint8_t index) const {
    if (index >= product.n_options) return {};
    size_t p = product.options;
    for (uint8_t i = 0; i < index; ++i) p += 11 + size_t(byte(p + 10)) * 3;
    Option o;
    o.attr = byte(p);
    o.argument = byte(p + 1);
    o.def = word(p + 2);
    o.min = word(p + 4);
    o.max = word(p + 6);
    o.step = std::max<uint16_t>(word(p + 8), 1);
    const uint8_t count = byte(p + 10);
    p += 11;
    for (uint8_t i = 0; i < count; ++i, p += 3) o.choices.push_back({byte(p), string(word(p + 1))});
    return o;
  }
  Option option_for(const Product &product, uint8_t attr) const {
    for (uint8_t i = 0; i < product.n_options; ++i) {
      auto o = option(product, i);
      if (o.attr == attr) return o;
    }
    return {};
  }
  const Product *find(const std::string &label) const {
    for (const auto &p : products)
      if ((p.flags & 1) && lower(p.name) == lower(label)) return &p;
    return nullptr;
  }
  const Product *find(uint8_t code) const {
    for (const auto &p : products)
      if (p.code == code) return &p;
    return nullptr;
  }
  static std::string lower(std::string value) {
    for (char &c : value)
      if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
  }
  bool recipe(const Product &product, const int32_t requested[ATTRIBUTE_COUNT], std::string &hex,
              std::string &error) const {
    uint8_t values[16]{};
    values[0] = product.code;
    values[8] = 1;
    for (uint8_t attr = 0; attr < ATTRIBUTE_COUNT; ++attr) {
      if (requested[attr] < -1) {
        error = "Only -1 means profile default";
        return false;
      }
      const auto opt = option_for(product, attr);
      if (!opt.valid()) {
        if (requested[attr] >= 0) {
          error = "Parameter unsupported by selected drink";
          return false;
        }
        continue;
      }
      if (opt.argument < 2 || opt.argument > 16 || opt.argument == 9) {
        error = "Unsupported recipe layout";
        return false;
      }
      int32_t value = requested[attr] >= 0 ? requested[attr] : opt.def;
      if (!opt.accepts(value)) {
        error = "Parameter outside profile range, step or allowed choices";
        return false;
      }
      if (attr == WATER || attr == BYPASS) {
        if (value % 5) {
          error = "Water/bypass must be multiples of 5 ml";
          return false;
        }
        value /= 5;
      }
      if (value > 255 || value < 0) {
        error = "Parameter cannot be encoded";
        return false;
      }
      values[opt.argument - 1] = static_cast<uint8_t>(value);
    }
    const char digits[] = "0123456789ABCDEF";
    hex.clear();
    for (auto value : values) {
      hex += digits[value >> 4];
      hex += digits[value & 15];
    }
    return true;
  }
  std::string ef, name;
  std::vector<Product> products;
  std::vector<Alert> alerts;
  std::vector<std::string> maintenance_counts, maintenance_percentages;
};
}  // namespace jura_native
}  // namespace esphome
