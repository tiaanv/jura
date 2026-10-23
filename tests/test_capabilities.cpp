#include <cassert>
#include <iostream>
#include "../components/jura_native/jura_capabilities.h"
using namespace esphome::jura_native;
int main() {
  assert(capability_hash("hello") == 1335831723u);
  assert(capability_quote("a\"b\\c\n") == "\"a\\\"b\\\\c\\u000a\"");
  for (uint16_t i = 0; i < Profiles::word(6); ++i) {
    Profiles profile;
    assert(profile.select(Profiles::string(Profiles::word(12 + i * 8))));
    const auto catalogue = recipe_capabilities(profile);
    const auto chunks = capability_chunks(catalogue);
    std::string rebuilt;
    for (const auto &chunk : chunks) { assert(chunk.size() <= 240); rebuilt += chunk; }
    assert(rebuilt == catalogue);
    std::cout << catalogue << '\n';  // Parsed and checked against the DB by Python.
  }
  const auto utf8 = capability_chunks(std::string(239, 'x') + "\xc3\xa9");
  assert(utf8.size() == 2 && utf8[0].size() == 239 && utf8[1].size() == 2);
}
