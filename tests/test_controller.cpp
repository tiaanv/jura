#include <cassert>
#include <iostream>
#include <map>
#include <set>

#include "../components/jura_native/jura_controller.h"
using namespace esphome::jura_native;
namespace cipher = jura_protocol::atproto;

static std::vector<uint8_t> encode(const std::string &s) {
  std::vector<uint8_t> out;
  for (unsigned char c : s)
    for (unsigned n = 0; n < 8; n += 2)
      out.push_back(uint8_t(0xDB | (((c >> n) & 1) << 2) | (((c >> (n + 1)) & 1) << 5)));
  return out;
}
static std::string decode(const std::vector<uint8_t> &v) {
  assert(v.size() % 4 == 0);
  std::string s;
  for (size_t i = 0; i < v.size(); i += 4) {
    uint8_t c = 0;
    for (unsigned n = 0; n < 4; ++n)
      c |= uint8_t((((v[i + n] >> 2) & 1) << (2 * n)) | (((v[i + n] >> 5) & 1) << (2 * n + 1)));
    s += char(c);
  }
  return s;
}
struct Fake : public JuraController {
  std::map<std::string, std::string> texts;
  std::map<std::string, float> nums;
  std::map<std::string, int> bins;
  std::vector<uint8_t> output;
  unsigned installed = 0, recipe_updates = 0;
  void text(const std::string &k, const std::string &v) override { texts[k] = v; }
  void number(const std::string &k, float v) override { nums[k] = v; }
  void binary(const std::string &k, int v) override { bins[k] = v; }
  void profile_changed(bool) override { ++installed; }
  void recipe_changed() override { ++recipe_updates; }
  uint32_t write_transport(const uint8_t *p, size_t n) override {
    output.insert(output.end(), p, p + n);
    return now_;
  }
  bool ready() const { return session_ == Session::READY; }
  bool backoff() const { return session_ == Session::BACKOFF; }
  size_t queued_polls() const { return polls_.size(); }
  bool waiting() const { return transport_.waiting(); }
  std::string active() const { return active_command_; }
};
struct Machine {
  Fake dev;
  uint32_t clock = 0;
  size_t consumed = 0;
  std::string outgoing;
  std::string model = "EF533M V02.09", challenge = "010001B228", tf = "000400000000";
  bool answer_at = true, answer_brew = true, reject_brew = false, auto_tf = true;
  std::vector<std::string> commands;
  void inject(const std::string &s, bool at = true) {
    for (auto raw : encode(at ? cipher::build_frame(s + "\r\n", 0xA0) : s + "\r\n")) dev.input(raw, clock);
  }
  void begin(bool control = true, const std::string &override = "") {
    dev.configure(control, true, true, 100, override);
    dev.begin(clock);
  }
  void tick(unsigned n = 1) {
    for (unsigned i = 0; i < n; ++i) {
      ++clock;
      dev.advance(clock);
      pump();
    }
  }
  size_t count(const std::string &value) {
    size_t n = 0;
    for (const auto &c : commands) n += c == value;
    return n;
  }
  void pump() {
    while (consumed + 4 <= dev.output.size()) {
      outgoing +=
          decode(std::vector<uint8_t>(dev.output.begin() + consumed, dev.output.begin() + consumed + 4));
      consumed += 4;
      if (outgoing.size() < 2 || outgoing.compare(outgoing.size() - 2, 2, "\r\n")) continue;
      std::string command;
      bool at = outgoing[0] == '&';
      if (at)
        assert(cipher::decode_frame(outgoing, command));
      else
        command = outgoing.substr(0, outgoing.size() - 2);
      outgoing.clear();
      commands.push_back(command);
      if (command == "TY:")
        inject("ty:" + model, false);
      else if (command == "AN:02")
        inject("an:OK", false);
      else if (command == "RT:0000")
        inject("rt:00F40004014C2D3E003A0014003534F400490024BCB300320000011600000066", false);
      else if (at && answer_at) {
        if (command == "@T0")
          inject("@t0");
        else if (command == "@T1") {
          inject("@t1");
          inject("@T2:" + challenge);
        } else if (command.compare(0, 4, "@t2:") == 0)
          inject("@T3:3BA4" + model);
        else if (command == "@TF:" && auto_tf)
          inject("@TF:" + tf);
        else if (command.compare(0, 7, "@TR:32,") == 0)
          inject("@tr:32," + command.substr(7, 2) + ",0001000200030004");
        else if (command == "@TG:43")
          inject("@tg:43000100020003000400050006");
        else if (command == "@TG:C0")
          inject("@tg:C00A141E");
        else if (command.compare(0, 4, "@TP:") == 0 && answer_brew) {
          if (reject_brew)
            inject("@tp:00");
          else
            inject("@TB");
        }
      }
    }
  }
};
static void test_profiles() {
  assert(Profiles::word(6) == 75);
  std::set<std::string> names;
  for (unsigned i = 0; i < Profiles::word(6); ++i) {
    const auto ef = Profiles::string(Profiles::word(12 + i * 8));
    names.insert(ef);
    Profiles p;
    assert(p.select(ef + "M V02.09"));
    assert(p.ef == ef);
    assert(!p.products.empty());
    for (const auto &product : p.products) {
      for (uint8_t j = 0; j < product.n_options; ++j) {
        const auto o = p.option(product, j);
        assert(o.valid());
        assert(o.argument >= 2 && o.argument <= 16);
      }
      if (product.flags & 1) {
        std::array<int32_t, ATTRIBUTE_COUNT> defaults;
        defaults.fill(-1);
        std::string encoded, error;
        const bool valid = p.recipe(product, defaults.data(), encoded, error);
        // Preserve the database but reject its one inconsistent default choice.
        if (p.ef == "EF562" && product.name == "2 Ristretti") {
          assert(!valid);
          defaults[TEMPERATURE] = 1;
          assert(p.recipe(product, defaults.data(), encoded, error));
        } else {
          assert(valid && encoded.size() == 32);
        }
      }
    }
  }
  assert(names.size() == 75);
  Profiles p;
  assert(p.select("EF533M V02.09"));
  assert(p.ef == "EF533");
  assert(p.select("EF533V2M V02.09"));
  assert(p.ef == "EF533V2");
  assert(!p.select("Unknown"));
  assert(p.select("EF533M V02.09"));
  const auto *espresso = p.find("espresso");
  assert(espresso && espresso->code == 2);
  std::array<int32_t, ATTRIBUTE_COUNT> values;
  values.fill(-1);
  std::string blob, error;
  assert(p.recipe(*espresso, values.data(), blob, error));
  assert(blob == "02000409000002000100000000000000");
  values[WATER] = 50;
  assert(p.recipe(*espresso, values.data(), blob, error));
  assert(blob.substr(6, 2) == "0A");
  values[WATER] = 52;
  assert(!p.recipe(*espresso, values.data(), blob, error));
  values[WATER] = 0;
  assert(!p.recipe(*espresso, values.data(), blob, error));
  values.fill(-1);
  values[MILK] = 20;
  assert(!p.recipe(*espresso, values.data(), blob, error));
  values.fill(-1);
  values[WATER] = -2;
  assert(!p.recipe(*espresso, values.data(), blob, error));
  assert(p.select("EF536M V02.01"));
  values.fill(-1);
  assert(p.recipe(*p.find("Espresso"), values.data(), blob, error));
  assert(blob == "02000809000002000100000000000000");
}
static void test_cipher() {
  std::string all;
  for (unsigned c = 0; c < 256; ++c) all += char(c);
  assert(decode(encode(all)) == all);
  for (unsigned key = 0; key < 256; ++key) {
    assert(cipher::crypt(cipher::crypt(all, uint8_t(key)), uint8_t(key)) == all);
    std::string out;
    assert(cipher::decode_frame(cipher::build_frame("@TF:000400000000\r\n", uint8_t(key)), out));
    assert(out == "@TF:000400000000");
  }
  std::string out;
  assert(!cipher::decode_frame("&\x1b\r\n", out));
}
static void test_startup_and_statistics() {
  Machine m;
  m.begin();
  m.tick(16000);
  assert(m.dev.discovery_ready() && m.dev.ready());
  assert(m.dev.installed == 1);
  assert(m.dev.texts["profile"] == "EF533" && m.dev.bins["connected"] == 1);
  assert(m.count("@t2:8120000000") == 1);
  assert(m.dev.texts["status"] == "ready");
  assert(m.dev.nums["grounds_level"] == 0 && m.dev.nums["single_espresso_made"] == 244);
  assert(m.dev.nums["maintenance_count_5"] == 6 && m.dev.nums["maintenance_percent_2"] == 30);
  const auto before = m.dev.queued_polls();
  for (unsigned i = 0; i < 100; ++i) m.dev.refresh();
  assert(m.dev.queued_polls() <= before + 20);
  m.tick(16000);
  assert(m.dev.power_off());
  m.tick(1000);
  assert(m.count("AN:02") == 1);
  m.inject("@TF:400400000000");
  assert(m.dev.texts["status"] == "blocked");
  assert(!m.dev.brew_named("Espresso"));
  m.inject("@TF:000400000000");
  assert(m.dev.texts["status"] == "ready");
}
static void test_brew() {
  Machine m;
  m.begin();
  m.tick(16000);
  assert(m.dev.brew_custom("Espresso", "6", 50, -1, -1, "High", -1, -1, ""));
  assert(!m.dev.brew_named("Coffee"));
  m.tick(2000);
  bool found = false;
  for (const auto &c : m.commands)
    if (c.compare(0, 4, "@TP:") == 0) {
      assert(c == "@TP:0200040A000002000100000000000000");
      found = true;
    }
  assert(found);
  assert(m.dev.texts["command_result"] == "Machine active; waiting for product confirmation");
  m.inject("@TV:390206060009FFFFFFFF22FFFF1100FF");
  assert(m.dev.texts["command_result"] == "Started");
  assert(m.dev.texts["current_product"] == "Espresso" && m.dev.texts["progress_state"] == "Grinding");
  const auto tf_queries = m.count("@TF:");
  for (unsigned i = 0; i < 40; ++i) {
    m.tick(2000);
    m.inject("@TV:3C0206060409FFFFFFFF22FFFF1132FF");
  }
  assert(m.dev.ready() && m.dev.bins["connected"] == 1);
  assert(m.count("@TF:") == tf_queries);
  assert(m.dev.nums["progress"] == 50 && m.dev.nums["stage_current"] == 20 &&
         m.dev.nums["stage_target"] == 45);
  assert(m.dev.texts["stage_unit"] == "ml" && m.dev.bins["alerts_stale"] == 1);
  assert(!m.dev.brew_named("Coffee"));
  m.inject("@TV:3E02");
  assert(m.dev.texts["command_result"] == "Completed");
  assert(m.dev.texts["status"] == "finishing");
  assert(m.dev.nums["progress"] == 100 && m.dev.bins["brew_active"] == 0);
  m.tick(1000);
  assert(m.dev.texts["status"] == "ready");
  assert(!m.dev.brew_custom("Espresso", "99", 50, -1, -1, "High", -1, -1, ""));
  assert(!m.dev.brew_custom("Espresso", "6", 52, -1, -1, "High", -1, -1, ""));
  assert(!m.dev.brew_custom("Espresso", "6", 50, 20, -1, "High", -1, -1, ""));
}
static void test_failure_and_rejection() {
  Machine m;
  m.reject_brew = true;
  m.begin();
  m.tick(16000);
  assert(m.dev.brew_named("Espresso"));
  m.tick(2000);
  assert(m.dev.texts["command_result"].find("Rejected by machine") != std::string::npos);
  auto sent = m.commands.size();
  m.tick(25000);
  for (size_t i = sent; i < m.commands.size(); ++i) assert(m.commands[i].compare(0, 4, "@TP:") != 0);
  Machine lost;
  lost.answer_at = false;
  lost.begin();
  lost.tick(12000);
  assert(lost.dev.backoff());
  lost.answer_at = true;
  lost.tick(70000);
  assert(lost.dev.ready());
  Machine unsupported;
  unsupported.challenge = "01FFFF";
  unsupported.begin();
  unsupported.tick(8000);
  assert(unsupported.dev.backoff());
  Machine override;
  override.challenge = "01FFFF";
  override.begin(true, "818000");
  override.tick(15000);
  assert(override.dev.ready());
  Machine monitor;
  monitor.begin(false);
  monitor.tick(15000);
  assert(!monitor.dev.brew_named("Espresso"));
  Machine different;
  different.begin();
  different.tick(16000);
  assert(different.dev.brew_named("Espresso"));
  different.tick(2000);
  different.inject("@TV:390306060009FFFFFFFF22FFFF1100FF");
  assert(different.dev.texts["command_result"].find("Different operation") != std::string::npos);
  Machine silent;
  silent.answer_brew = false;
  silent.begin();
  silent.tick(16000);
  assert(silent.dev.brew_named("Espresso"));
  silent.tick(26000);
  unsigned brews = 0;
  for (const auto &c : silent.commands) brews += c.compare(0, 4, "@TP:") == 0;
  assert(brews == 1);
  assert(silent.dev.texts["command_result"].find("Uncertain") != std::string::npos);
}
static void test_banks_and_progress() {
  Machine m;
  m.model = "EF1092M V02.01";
  m.begin();
  m.tick(25000);
  assert(m.count("@TR:32,0C") && m.count("@TR:32,0D"));  // Includes codes above 0x2F.
  m.inject("@TB");
  m.inject("@TV:3C028F06060409FFFFFFFF22FFFF1132FF");
  assert(m.dev.nums["progress"] == 50 && m.dev.nums["stage_current"] == 20);
  m.inject("@TV:3C02");
  assert(std::isnan(m.dev.nums["progress"]));
  m.tick(21000);
  assert(m.dev.bins["brew_active"] == -1);
  Machine z;
  z.model = "EF545M V02.00";
  z.begin();
  z.tick(25000);
  assert(z.count("@TR:32,04"));
  Machine wrap;
  wrap.clock = 0xFFFFF000u;
  wrap.begin();
  wrap.tick(16000);
  assert(wrap.dev.ready());
  Machine changed;
  changed.begin();
  changed.tick(16000);
  changed.inject("ty:EF536M V02.09", false);
  assert(changed.dev.backoff());
  assert(changed.dev.texts["session"].find("restart required") != std::string::npos);
  const auto handshakes = changed.count("@T0");
  changed.tick(70000);
  assert(changed.count("@T0") == handshakes);
  assert(!changed.dev.brew_named("Espresso"));
  Machine short_tf;
  short_tf.begin();
  short_tf.tick(16000);
  short_tf.inject("@TF:0004");
  assert(short_tf.dev.texts["status"] == "unknown");
  assert(!short_tf.dev.brew_named("Espresso"));
}
int main() {
  test_profiles();
  test_cipher();
  test_startup_and_statistics();
  test_brew();
  test_failure_and_rejection();
  test_banks_and_progress();
  std::cout << "PASS: all 75 profiles, cipher, statistics, native recipe semantics, brewing lifecycle, busy "
               "rejection, TV liveness, retry/rejection and rollover\n";
}
