#pragma once
#include "jura_profiles.h"

namespace esphome {
namespace jura_native {
// Retained native-API text states, each <=240 UTF-8 bytes (HA allows 255 chars).
// Manifest is published last. The card verifies profile, byte hash and schema.
inline std::string capability_quote(const std::string &value) {
  std::string out = "\"";
  const char hex[] = "0123456789abcdef";
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
    else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
    else out += char(c);
  }
  return out + '"';
}
inline std::string recipe_capabilities(const Profiles &profile) {
  static const char *fields[] = {"strength", "grinder", "temperature", "water_ml", "milk_seconds",
                                  "foam_seconds", "bypass_ml", "pause_seconds"};
  std::string out = "{\"v\":1,\"p\":" + capability_quote(profile.ef) + ",\"d\":{";
  bool first = true;
  for (const auto &product : profile.products) {
    if (!(product.flags & 1)) continue;
    if (!first) out += ',';
    first = false;
    out += capability_quote(product.name) + ":{";
    bool first_option = true;
    for (uint8_t i = 0; i < product.n_options; ++i) {
      const auto option = profile.option(product, i);
      if (!option.valid()) continue;
      if (!first_option) out += ',';
      first_option = false;
      out += capability_quote(fields[option.attr]) + ':';
      if (option.choices.empty()) {
        out += "{\"n\":[" + std::to_string(option.min) + ',' + std::to_string(option.max) + ',' +
               std::to_string(option.step) + "]}";
      } else {
        out += '[';
        for (size_t j = 0; j < option.choices.size(); ++j) {
          if (j) out += ',';
          out += capability_quote(option.choices[j].name);
        }
        out += ']';
      }
    }
    out += '}';
  }
  return out + "}}";
}
inline uint32_t capability_hash(const std::string &value) {
  uint32_t hash = 2166136261u;
  for (unsigned char c : value) hash = (hash ^ c) * 16777619u;
  return hash;
}
inline std::vector<std::string> capability_chunks(const std::string &value) {
  std::vector<std::string> chunks;
  for (size_t pos = 0; pos < value.size();) {
    size_t end = std::min(pos + 240, value.size());
    while (end < value.size() && (uint8_t(value[end]) & 0xC0) == 0x80) --end;
    chunks.push_back(value.substr(pos, end - pos));
    pos = end;
  }
  return chunks;
}
}  // namespace jura_native
}  // namespace esphome
