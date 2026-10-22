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
  std::vector<std::string> traces;
  void trace(const std::string &s) override { traces.push_back(s); }
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
  void disconnect() { session_failed_("Test connection loss"); }
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
  bool answer_type = true, answer_at = true, answer_brew = true, reject_brew = false, auto_tf = true;
  std::vector<std::string> commands;
  std::string probe_reply = "@tm:41,0200040A00000200010000000000000000F4";
  std::string an01_reply, early_hm;
  bool early_hm_encrypted = true;
  std::map<std::string, std::string> recipe_replies;
  void inject(const std::string &s, bool at = true) {
    for (auto raw : encode(at ? cipher::build_frame(s + "\r\n", 0xA0) : s + "\r\n")) dev.input(raw, clock);
  }
  void begin(bool control = true, const std::string &override = "", uint32_t startup = 100,
             const std::string &hint = "") {
    dev.configure(control, true, true, startup, override, hint);
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
      // Assert framing at the wire boundary, before decoded commands hide it.
      if (command.compare(0, 2, "@m") == 0) assert(!at);
      if (command == "@H1") assert(!at);
      if (command == "TY:" && answer_type)
        inject("ty:" + model, false);
      else if (command == "AN:02")
        inject("an:OK", false);
      else if (command == "AN:01" && !an01_reply.empty())
        inject(an01_reply, false);
      else if (command == "RT:0000")
        inject("rt:00F40004014C2D3E003A0014003534F400490024BCB300320000011600000066", false);
      else if (at && answer_at) {
        if (command == "@T0")
          inject("@t0");
        else if (command == "@T1") {
          inject("@t1");
          inject("@T2:" + challenge);
        } else if (command.compare(0, 4, "@t2:") == 0) {
          inject("@T3:3BA4" + model);
          if (!early_hm.empty()) inject(early_hm, early_hm_encrypted);
        } else if (command == "@TF:" && auto_tf)
          inject("@TF:" + tf);
        else if (command.compare(0, 7, "@TR:32,") == 0)
          inject("@tr:32," + command.substr(7, 2) + ",0001000200030004");
        else if (command == "@TG:43")
          inject("@tg:43000100020003000400050006");
        else if (command == "@TG:C0")
          inject("@tg:C00A141E");
        else if (command == "@TM:41,02" && !probe_reply.empty())
          inject(probe_reply);
        else if (recipe_replies.count(command))
          inject(recipe_replies[command]);
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
// EF533M V01.18 sends a 1-byte challenge and a 5-byte TF frame, so the EF533
// profile's blocking bits 40 (close tab) / 47 (SwitchOff Delay) are absent.
static void test_short_tf_frame() {
  for (bool accept : {false, true}) {
    Machine m;
    m.model = "EF533M V01.18";
    m.challenge = "01";
    m.tf = "0004000060";
    m.dev.set_accept_short_status(accept);
    m.begin(true, "81");
    m.tick(16000);
    assert(m.dev.ready() && m.dev.texts["profile"] == "EF533");
    assert(m.count("@t2:81") == 1);
    assert(m.dev.texts["active_alerts"] == "decalc alert, cleaning alert");
    assert(m.dev.bins["alert_40"] == -1 && m.dev.bins["alert_47"] == -1);
    if (!accept) {
      assert(m.dev.texts["status"] == "unknown");
      assert(m.dev.texts["status_detail"] == "Incomplete machine status");
      assert(!m.dev.brew_named("Espresso"));
      continue;
    }
    assert(m.dev.texts["status"] == "ready");
    m.inject("@TF:8004000060");  // insert tray (bit 0, blocking) still blocks
    assert(m.dev.texts["status"] == "blocked");
    assert(!m.dev.brew_named("Espresso"));
    m.inject("@TF:0004000060");
    assert(m.dev.texts["status"] == "ready");
    m.inject("@TF:0008000060");  // heating up, coffee ready not yet set
    assert(m.dev.texts["status"] == "heating");
    assert(!m.dev.brew_named("Espresso"));
    // Even valid framed hex with coffee-ready set must not bypass the guard
    // when it is shorter than the observed five-byte EF533 format.
    for (const char *short_tf : {"0004", "000400", "00040000"}) {
      m.inject(std::string("@TF:") + short_tf);
      assert(m.dev.texts["status"] == "unknown");
      assert(m.dev.texts["status_detail"] == "Incomplete machine status");
      assert(!m.dev.brew_named("Espresso"));
    }
    // A full frame still honours blocking bits in the sixth byte.
    m.inject("@TF:000400006001");  // bit 47: SwitchOff Delay active
    assert(m.dev.texts["status"] == "blocked");
    assert(!m.dev.brew_named("Espresso"));
    m.inject("@TF:0004000060");
    assert(m.dev.brew_named("Espresso"));
  }
  // Five bytes on another profile must not receive the EF533 exception.
  Machine other;
  other.model = "EF545M V02.09";
  other.tf = "0004000000";
  other.dev.set_accept_short_status(true);
  other.begin();
  other.tick(16000);
  assert(other.dev.ready() && other.dev.texts["profile"] == "EF545");
  assert(other.dev.texts["status"] == "unknown");
  assert(other.dev.texts["status_detail"] == "Incomplete machine status");
  assert(!other.dev.brew_named("Espresso"));
}
static void test_disabled_saved_recipes() {
  Machine m;
  m.probe_reply.clear();  // This machine does not answer saved recipe reads.
  m.dev.set_read_saved_recipes(false);
  m.begin();
  m.tick(16000);
  assert(m.dev.bins["recipe_loading"] == 0);
  assert(m.dev.texts["recipe_source"] == "Profile defaults");
  assert(m.dev.texts["recipe_status"] == "Profile defaults: saved recipe reads disabled");
  assert(m.dev.selected_values()[WATER] == 45);
  m.dev.choose("Coffee");
  m.dev.reset_recipe();
  m.tick(7000);
  m.dev.choose("Espresso");
  m.tick(7000);
  assert(m.dev.bins["recipe_loading"] == 0);
  assert(m.dev.set_parameter(WATER, 60));
  assert(m.dev.brew_selected());
  assert(m.dev.selected_values()[WATER] == 45 && m.dev.bins["recipe_modified"] == 0);
  m.tick(1500);
  assert(m.count("@TP:0200040C000002000100000000000000") == 1);
  for (const auto &command : m.commands) assert(command.compare(0, 7, "@TM:41,") != 0);

  for (bool custom : {false, true}) {
    Machine api;
    api.dev.set_read_saved_recipes(false);
    api.begin();
    api.tick(16000);
    assert(api.dev.set_parameter(WATER, 80));
    const auto ui = api.dev.selected_values();
    if (custom)
      assert(api.dev.brew_custom("Espresso", "", 60, -1, -1, "", -1, -1, ""));
    else
      assert(api.dev.brew_named("Espresso"));
    assert(!api.dev.brew_named("Espresso"));  // Duplicate protection is unchanged.
    assert(api.dev.texts["command_recipe_source"] == "Profile defaults");
    assert(api.dev.texts["command_recipe_status"] == "Profile defaults: saved recipe reads disabled");
    assert(api.dev.texts["command_recipe_raw"].empty());
    assert(api.dev.selected_values() == ui && api.dev.bins["recipe_modified"] == 1);
    api.tick(1500);
    assert(api.count(custom ? "@TP:0200040C000002000100000000000000"
                            : "@TP:02000409000002000100000000000000") == 1);
    for (const auto &command : api.commands) assert(command.compare(0, 7, "@TM:41,") != 0);
  }
  Machine diagnostic;
  diagnostic.dev.set_read_saved_recipes(false);
  diagnostic.begin();
  diagnostic.tick(16000);
  assert(diagnostic.count("@TM:41,02") == 0);
  assert(diagnostic.dev.probe_espresso_settings());
  diagnostic.tick(2000);
  assert(diagnostic.count("@TM:41,02") == 1);
  assert(diagnostic.dev.texts["espresso_probe_result"].find("Valid response") == 0);
  diagnostic.dev.reset_recipe();
  diagnostic.tick(7000);
  assert(diagnostic.count("@TM:41,02") == 1);
  diagnostic.dev.disconnect();
  diagnostic.tick(70000);
  assert(diagnostic.dev.ready() && diagnostic.count("@TM:41,02") == 1);
}
static void test_dispensing_stage_progress() {
  Machine m;
  m.begin();
  m.tick(16000);
  assert(std::isnan(m.dev.nums["stage_progress"]));
  // Actual older-firmware format: no overall percentage field.
  m.inject("@TV:3C0304040018FFFFFFFF11FFFF00");
  assert(std::isnan(m.dev.nums["progress"]));
  assert(m.dev.nums["stage_progress"] == 0);
  m.inject("@TV:3C0304040C18FFFFFFFF11FFFF00");
  assert(m.dev.nums["stage_current"] == 60 && m.dev.nums["stage_target"] == 120);
  assert(m.dev.nums["stage_progress"] == 50);
  m.inject("@TV:3C0304041818FFFFFFFF11FFFF00");
  assert(m.dev.nums["stage_progress"] == 100 && m.dev.bins["brew_active"] == 1);
  for (const char *frame : {"@TV:3C0304040C00", "@TV:3C0304040CFF", "@TV:3C030404FF18",
                            "@TV:3C0304041918", "@TV:3C03", "@TV:210301", "@TV:39030404"}) {
    m.inject(frame);
    assert(std::isnan(m.dev.nums["stage_progress"]));
  }
  // A real overall percentage keeps its own meaning, independently of stage %.
  m.inject("@TV:3C0206060409FFFFFFFF22FFFF1132FF");
  assert(m.dev.nums["progress"] == 50);
  assert(std::abs(m.dev.nums["stage_progress"] - 100.0f * 4 / 9) < 0.01f);
  for (const char *frame : {"@TV:370304040C18", "@TV:3203FFFFFFFF0A14",
                            "@TV:3403FFFFFFFFFFFF0A14", "@TV:410304040C18"}) {
    m.inject(frame);
    assert(m.dev.nums["stage_progress"] == 50);
  }
  m.inject("@TV:3E03");
  assert(m.dev.nums["progress"] == 100 && std::isnan(m.dev.nums["stage_progress"]));
  m.inject("@TF:000400000000");
  assert(std::isnan(m.dev.nums["stage_progress"]));
  m.inject("@TV:3C0304040C18FFFFFFFF11FFFF00");
  m.tick(21000);
  assert(std::isnan(m.dev.nums["stage_progress"]));
  m.inject("@TV:3C0304040C18FFFFFFFF11FFFF00");
  m.dev.disconnect();
  assert(m.dev.backoff() && std::isnan(m.dev.nums["stage_progress"]));
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
static void test_ef1092_handshake() {
  Machine m;
  m.model = "EF1092M V01.07";
  m.challenge = "01C041B628";
  m.begin();
  m.tick(16000);
  assert(m.dev.discovery_ready() && m.dev.ready());
  assert(m.dev.texts["profile"] == "EF1092" && m.dev.texts["challenge"] == "01C041B628");
  assert(m.count("@t2:8120000000") == 1);
}
static void test_length_based_handshake() {
  for (const auto &challenge : {"01", "aB", "01C000", "01FFFF", "aBcDeF", "010001B228", "01C041B628",
                                "aBcDeF1234"}) {
    Machine m;
    m.challenge = challenge;
    m.begin();
    m.tick(16000);
    assert(m.dev.ready());
    const std::string response = m.challenge.size() == 2   ? "81"
                                 : m.challenge.size() == 6 ? "818000"
                                                           : "8120000000";
    assert(m.count("@t2:" + response) == 1);
  }
  for (const auto &challenge : {"", "0", "0G", "0102", "01020304", "010203040506", "01FFF", "01GG00",
                               "010001B22G"}) {
    Machine m;
    m.challenge = challenge;
    m.begin();
    m.tick(8000);
    assert(m.dev.backoff());
    for (const auto &command : m.commands) assert(command.compare(0, 4, "@t2:") != 0);
  }
  for (const auto &challenge : {"01C000", "01C041B628", "01020304"}) {
    Machine m;
    m.challenge = challenge;
    const std::string response = m.challenge.size() == 6 ? "810000" :
                                 m.challenge.size() == 10 ? "8100000000" : "81000000";
    m.begin(true, response);
    m.tick(16000);
    assert(m.dev.ready() && m.count("@t2:" + response) == 1);
  }
  Machine v0118;  // Captured E8 EF533M V01.18: @T2:01 -> @t2:81 -> @T3:3B35EF533M V01.18.
  v0118.model = "EF533M V01.18";
  v0118.challenge = "01";
  v0118.begin();
  v0118.tick(16000);
  assert(v0118.dev.ready() && v0118.dev.texts["profile"] == "EF533");
  assert(v0118.count("@t2:81") == 1);
  Machine mismatch;
  mismatch.begin(true, "818000");  // Three-byte override cannot answer a five-byte challenge.
  mismatch.tick(8000);
  assert(mismatch.dev.backoff());
  assert(mismatch.count("@t2:818000") == 0);
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
  unsupported.challenge = "01FFFFFF";  // Four-byte challenges still require an override.
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
  m.auto_tf = false;  // No fresh status to resolve the expired progress stream.
  m.tick(21000);
  assert(m.dev.bins["brew_active"] == -1);
  m.inject("@TF:00040000000000000000000000000000");  // Full EF1092 alert coverage.
  assert(m.dev.bins["brew_active"] == 0 && m.dev.texts["current_product"] == "None");
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
static void test_at_only_identification() {
  Machine m;
  m.answer_type = false;
  m.begin(true, "", 8000);
  m.tick(8001);
  assert(!m.dev.discovery_ready() && !m.dev.profile_ok());
  m.tick(10000);
  assert(m.dev.discovery_ready() && m.dev.profile().ef == "EF533" && m.dev.ready());
  assert(m.dev.installed == 1 && m.dev.bins["connected"] == 1);
  assert(m.count("@t3") == 1 && m.dev.brew_named("Espresso"));

  Machine no_machine;
  no_machine.answer_type = no_machine.answer_at = false;
  no_machine.begin(true, "", 8000);
  no_machine.tick(38001);
  assert(no_machine.dev.discovery_ready() && !no_machine.dev.profile_ok());
  assert(no_machine.dev.installed == 1);

  Machine hint;
  hint.answer_type = false;
  hint.begin(true, "", 8000, "EF533");
  hint.tick(8001);
  assert(hint.dev.discovery_ready() && hint.dev.profile().ef == "EF533");
  hint.tick(10000);
  assert(hint.dev.ready() && hint.dev.installed == 1);
}
static void test_espresso_probe() {
  Machine m;
  m.begin(false);  // Monitoring-only devices can run this read.
  m.tick(16000);
  assert(m.count("@TM:41,02") == 1);  // Selected recipe loads; diagnostic probe is still manual.
  assert(m.dev.texts["espresso_probe_result"] == "Not run");
  const auto selected = m.dev.selected_values();
  assert(m.dev.probe_espresso_settings());
  assert(!m.dev.probe_espresso_settings());
  assert(m.dev.texts["espresso_probe_result"].find("already pending") != std::string::npos);
  assert(!m.dev.brew_named("Espresso"));
  m.tick(2000);
  assert(m.count("@TM:41,02") == 2);
  assert(m.dev.texts["espresso_probe_result"].find("Valid response") == 0);
  const auto decoded = m.dev.texts["espresso_probe_settings"];
  assert(decoded.find("strength=6") != std::string::npos);
  assert(decoded.find("water=50 ml") != std::string::npos);
  assert(decoded.find("temperature=High") != std::string::npos);
  assert(m.dev.selected_values() == selected);
  assert(m.dev.texts["espresso_probe_raw"] == m.probe_reply);
  // A successful read releases the request; use the actual EF533 wire payload.
  m.probe_reply = "@tm:41,020006090000020000000000DB";
  assert(m.dev.probe_espresso_settings());
  m.tick(2000);
  assert(m.dev.texts["espresso_probe_settings"].find("strength=8") != std::string::npos);
  assert(m.dev.texts["espresso_probe_settings"].find("water=45 ml") != std::string::npos);
  m.probe_reply = "@tm:41,0200060A0000020000000000D3";
  m.inject("@TF:000400000000");
  assert(m.dev.probe_espresso_settings());
  m.tick(2000);
  assert(m.dev.texts["espresso_probe_settings"].find("water=50 ml") != std::string::npos);
  assert(m.dev.selected_values() == selected);
  for (const auto &command : m.commands) {
    assert(command.compare(0, 4, "@TP:") != 0);
    assert(command.compare(0, 4, "@TS:") != 0);
    if (command.compare(0, 4, "@TM:") == 0) assert(command == "@TM:41,02");
  }
  const std::pair<const char *, const char *> failures[] = {
      {"@tm:C1", "Unsupported"},
      {"@an:error", "Unexpected response"},
      {"@tm:41,0200040A0000020001000000000000000000", "checksum mismatch"},
      {"@tm:41,0300040A00000200010000000000000000F3", "not espresso"},
      {"@tm:41,020C", "incomplete or unknown"},
      {"@tm:41,0200FF0A00000200010000000000000000CC", "incomplete or unknown"},
      {"@tm:41,xyz", "malformed payload"}};
  for (const auto &failure : failures) {
    m.probe_reply = failure.first;
    assert(m.dev.probe_espresso_settings());
    m.tick(2000);
    assert(m.dev.texts["espresso_probe_result"].find(failure.second) != std::string::npos);
    assert(m.dev.selected_values() == selected);
  }
  m.probe_reply = "";
  assert(m.dev.probe_espresso_settings());
  m.tick(7000);
  assert(m.dev.texts["espresso_probe_result"].find("Timeout") == 0);
  const auto reads = m.count("@TM:41,02");
  m.tick(10000);
  assert(m.count("@TM:41,02") == reads);
  m.inject("@TB");
  assert(!m.dev.probe_espresso_settings());
  assert(m.dev.texts["espresso_probe_result"].find("operation active") != std::string::npos);
  m.inject("@TV:3E02");
  assert(!m.dev.probe_espresso_settings());
  assert(m.dev.texts["espresso_probe_result"].find("waiting for fresh TF") != std::string::npos);

  Machine queued;
  queued.begin();
  queued.tick(16000);
  assert(queued.dev.probe_espresso_settings());
  queued.inject("@TB");
  queued.tick(500);
  assert(queued.count("@TM:41,02") == 1);  // Only the initial selected-recipe read.
  assert(queued.dev.texts["espresso_probe_result"].find("Cancelled") == 0);
  Machine active;
  active.begin();
  active.tick(16000);
  active.probe_reply = "";
  assert(active.dev.probe_espresso_settings());
  active.tick(1500);
  assert(active.dev.active() == "@TM:41,02" && active.dev.waiting());
  assert(!active.dev.brew_named("Espresso") && !active.dev.power_off());
  active.inject("@TF:000400000000");  // Broadcasts must not complete the probe.
  assert(active.dev.waiting());
  active.tick(6000);
  assert(active.dev.texts["espresso_probe_result"].find("Timeout") == 0);
  Machine offline;
  offline.begin();
  assert(!offline.dev.probe_espresso_settings());
  Machine stale;
  stale.begin();
  stale.tick(16000);
  stale.auto_tf = false;
  for (int i = 0; i < 3; ++i) {
    stale.tick(21000);
    stale.inject("@tm:00");  // Keep AT alive while deliberately withholding TF.
  }
  assert(!stale.dev.probe_espresso_settings());
  assert(stale.dev.texts["espresso_probe_result"].find("older than 60") != std::string::npos);
}
static void test_saved_selected_recipes() {
  Machine m;
  m.probe_reply = "@tm:41,020006090000020000000000DB";
  m.begin();
  m.tick(16000);
  assert(m.dev.texts["recipe_source"] == "Machine");
  assert(m.dev.selected_values()[STRENGTH] == 6 && m.dev.selected_values()[WATER] == 45);
  assert(m.dev.bins["recipe_loading"] == 0 && m.dev.bins["recipe_modified"] == 0);
  assert(m.dev.choose("Espresso"));
  assert(!m.dev.brew_selected());  // Don't silently brew defaults while loading.
  assert(m.dev.set_parameter(WATER, 60));
  m.tick(2000);
  assert(m.dev.selected_values()[WATER] == 60 && m.dev.selected_values()[STRENGTH] == 6);
  assert(m.dev.bins["recipe_modified"] == 1);
  assert(m.dev.brew_selected());
  assert(m.dev.selected_values()[WATER] == 45 && m.dev.bins["recipe_modified"] == 0);
  m.tick(2000);
  assert(m.count("@TP:0200060C000002000100000000000000") == 1);
  assert(m.dev.set_parameter(WATER, 55));
  assert(!m.dev.brew_selected());  // Rejection retains edits for a later attempt.
  assert(m.dev.selected_values()[WATER] == 55 && m.dev.bins["recipe_modified"] == 1);

  Machine drinks;
  drinks.recipe_replies["@TM:41,03"] = "@tm:41,030006100000020000000000E2";
  drinks.recipe_replies["@TM:41,04"] = "@tm:41,0400060C0E00010000000000BB";
  drinks.begin();
  drinks.tick(16000);
  assert(drinks.dev.choose("Coffee"));
  drinks.tick(2000);
  assert(drinks.dev.texts["recipe_source"] == "Machine");
  assert(drinks.dev.selected_values()[WATER] == 80 && drinks.dev.selected_values()[TEMPERATURE] == 2);
  assert(drinks.dev.choose("Cappuccino"));
  drinks.tick(2000);
  assert(drinks.dev.selected_values()[MILK] == 14 && drinks.dev.selected_values()[WATER] == 60);
  assert(drinks.dev.selected_values()[TEMPERATURE] == 1);
  assert(drinks.dev.set_parameter(MILK, 20));
  drinks.dev.reset_recipe();
  assert(drinks.dev.bins["recipe_modified"] == 0 && drinks.dev.bins["recipe_loading"] == 1);
  drinks.tick(2000);
  assert(drinks.dev.selected_values()[MILK] == 14 && drinks.dev.texts["recipe_source"] == "Machine");

  for (const auto &reply : {"@tm:C1", "@tm:41,0200040A0000020001000000000000000000",
                            "@tm:41,0300040A00000200010000000000000000F3", "@tm:41,020C", ""}) {
    Machine fallback;
    fallback.probe_reply = reply;
    fallback.begin();
    fallback.tick(18000);
    assert(fallback.dev.texts["recipe_source"] == "Profile defaults");
    assert(fallback.dev.bins["recipe_loading"] == 0);
    assert(fallback.dev.selected_values()[WATER] == 45 && fallback.dev.selected_values()[STRENGTH] == 4);
    assert(fallback.dev.texts["recipe_status"].find("Profile defaults:") == 0);
    const auto requests = fallback.count("@TM:41,02");
    fallback.tick(10000);
    assert(fallback.count("@TM:41,02") == requests);  // No retry loop.
  }
  Machine late;
  late.begin();
  late.tick(16000);
  late.probe_reply.clear();
  late.recipe_replies["@TM:41,03"] = "@tm:41,030006100000020000000000E2";
  late.dev.choose("Espresso");
  late.tick(1500);
  assert(late.dev.active() == "@TM:41,02" && late.dev.waiting());
  late.dev.choose("Coffee");
  late.inject("@tm:41,020006090000020000000000DB");
  assert(late.dev.selected_values()[WATER] == 100);  // Coffee's provisional profile value, not espresso's 45.
  late.tick(2000);
  assert(late.dev.selection()->name == "Coffee" && late.dev.selected_values()[WATER] == 80);
  assert(late.dev.texts["saved_recipe_raw"] == late.recipe_replies["@TM:41,03"]);

  late.probe_reply.clear();
  late.dev.choose("Espresso");
  late.tick(1500);
  late.dev.reset_recipe();  // Reset the same product also invalidates its in-flight response.
  late.inject("@tm:41,020006090000020000000000DB");
  assert(late.dev.bins["recipe_loading"] == 1 && late.dev.selected_values()[STRENGTH] == 4);
  late.dev.choose("Coffee");
  late.tick(7000);
  assert(late.dev.texts["recipe_source"] == "Machine" && late.dev.selected_values()[WATER] == 80);
  Machine edited_failure;
  edited_failure.probe_reply.clear();
  edited_failure.begin();
  edited_failure.tick(5000);
  assert(edited_failure.dev.active() == "@TM:41,02" && edited_failure.dev.waiting());
  assert(edited_failure.dev.set_parameter(WATER, 60));
  edited_failure.tick(7000);
  assert(edited_failure.dev.texts["recipe_source"] == "Profile defaults");
  assert(edited_failure.dev.selected_values()[WATER] == 60);
  assert(edited_failure.dev.brew_selected());
  assert(edited_failure.dev.selected_values()[WATER] == 45);
  Machine old_timeout;
  old_timeout.probe_reply.clear();
  old_timeout.recipe_replies["@TM:41,03"] = "@tm:41,030006100000020000000000E2";
  old_timeout.begin();
  old_timeout.tick(5000);
  assert(old_timeout.dev.waiting());
  old_timeout.dev.choose("Coffee");
  old_timeout.tick(7000);
  assert(old_timeout.dev.texts["recipe_source"] == "Machine");
  assert(old_timeout.dev.selected_values()[WATER] == 80);
}
static size_t brew_count(const Machine &m) {
  size_t n = 0;
  for (const auto &command : m.commands) n += command.compare(0, 4, "@TP:") == 0;
  return n;
}
static void test_api_saved_defaults() {
  const std::string actual = "@tm:41,020006090000020000000000DB";
  Machine named;
  named.probe_reply = actual;
  named.begin();
  named.tick(16000);
  named.probe_reply = "@tm:41,0200060C0000020000000000D1";  // Saved water changed at screen.
  assert(named.dev.brew_named("Espresso"));
  assert(!named.dev.brew_named("Coffee"));
  named.dev.choose("Coffee");  // UI selection cannot change the reserved API product.
  named.tick(2500);
  assert(named.count("@TP:0200060C000002000100000000000000") == 1);
  assert(named.dev.texts["command_recipe_source"] == "Machine");
  assert(named.dev.selection()->name == "Coffee");

  Machine custom;
  custom.probe_reply = actual;
  custom.begin();
  custom.tick(16000);
  custom.dev.set_parameter(WATER, 80);  // API must not take or consume unrelated UI overrides.
  const auto ui = custom.dev.selected_values();
  assert(custom.dev.brew_custom("Espresso", "", 60, -1, -1, "default", -1, -1, ""));
  custom.tick(2500);
  assert(custom.count("@TP:0200060C000002000100000000000000") == 1);
  assert(custom.dev.selected_values() == ui && custom.dev.bins["recipe_modified"] == 1);

  Machine coffee;
  coffee.recipe_replies["@TM:41,03"] = "@tm:41,030006100000020000000000E2";
  coffee.begin();
  coffee.tick(16000);
  assert(coffee.dev.brew_custom("Coffee", "", 95, -1, -1, "", -1, -1, ""));
  coffee.tick(2500);
  assert(coffee.count("@TP:03000613000002000100000000000000") == 1);
  assert(coffee.dev.selection()->name == "Espresso");

  for (const auto &bad : {"@tm:C1", "@tm:41,0200040A0000020001000000000000000000",
                          "@tm:41,0300040A00000200010000000000000000F3", "@tm:41,020C", ""}) {
    Machine fallback;
    fallback.probe_reply = actual;
    fallback.begin();
    fallback.tick(16000);
    fallback.probe_reply = bad;
    assert(fallback.dev.brew_named("Espresso"));
    fallback.tick(10000);
    assert(fallback.count("@TP:02000409000002000100000000000000") == 1);
    assert(fallback.dev.texts["command_recipe_source"] == "Profile defaults");
    assert(fallback.dev.texts["command_recipe_status"].find("Profile defaults:") == 0);
    assert(fallback.dev.texts["recipe_source"] == "Machine");
    fallback.tick(20000);
    assert(brew_count(fallback) == 1);
  }
  Machine fallback_custom;
  fallback_custom.begin();
  fallback_custom.tick(16000);
  fallback_custom.probe_reply = "@tm:C1";
  assert(fallback_custom.dev.brew_custom("Espresso", "", 60, -1, -1, "Normal", -1, -1, ""));
  fallback_custom.tick(2500);
  assert(fallback_custom.count("@TP:0200040C000001000100000000000000") == 1);

  for (const auto &busy : {"@TB", "@TV:390206060009FFFFFFFF22FFFF1100FF", "@TF:400400000000"}) {
    Machine interrupted;
    interrupted.begin();
    interrupted.tick(16000);
    interrupted.probe_reply.clear();
    assert(interrupted.dev.brew_named("Espresso"));
    interrupted.tick(1200);
    interrupted.inject(busy);
    interrupted.inject("@TF:000400000000");
    interrupted.inject(actual);  // A late valid read must not revive a cancelled brew.
    interrupted.tick(8000);
    assert(brew_count(interrupted) == 0);
    assert(interrupted.dev.texts["command_recipe_status"].find("Cancelled:") == 0);
  }
  Machine before_read;
  before_read.begin();
  before_read.tick(16000);
  assert(before_read.dev.brew_named("Espresso"));
  before_read.inject("@TF:400400000000");
  before_read.tick(3000);
  assert(brew_count(before_read) == 0 && before_read.count("@TM:41,02") == 1);

  Machine native_read;
  native_read.probe_reply = actual;
  native_read.begin();
  native_read.tick(16000);
  native_read.dev.choose("Coffee");  // This native read has no simulated response.
  native_read.tick(1200);
  assert(native_read.dev.waiting());
  assert(native_read.dev.brew_named("Espresso"));
  native_read.dev.choose("Cappuccino");
  native_read.tick(10000);
  assert(native_read.count("@TP:02000609000002000100000000000000") == 1);

  Machine invalid;
  invalid.begin();
  invalid.tick(16000);
  const auto reads = invalid.count("@TM:41,02");
  assert(!invalid.dev.brew_custom("Espresso", "", 52, -1, -1, "", -1, -1, ""));
  assert(!invalid.dev.brew_custom("Espresso", "", -2, -1, -1, "", -1, -1, ""));
  invalid.tick(2000);
  assert(invalid.count("@TM:41,02") == reads && brew_count(invalid) == 0);
  Machine disconnected;
  disconnected.begin();
  disconnected.tick(16000);
  disconnected.probe_reply.clear();
  assert(disconnected.dev.brew_named("Espresso"));
  disconnected.tick(1200);
  disconnected.inject("ty:EF536M V02.09", false);
  disconnected.tick(12000);
  assert(brew_count(disconnected) == 0);
  assert(disconnected.dev.texts["command_recipe_status"].find("Cancelled:") == 0);
  Machine stray_ack;
  stray_ack.begin();
  stray_ack.tick(16000);
  stray_ack.probe_reply.clear();
  assert(stray_ack.dev.brew_named("Espresso"));
  stray_ack.tick(1200);
  stray_ack.inject("@tp:00");
  stray_ack.inject("@tp");
  assert(stray_ack.dev.texts["command_result"] == "Reading saved settings before brew");
  stray_ack.inject(actual);
  stray_ack.tick(2500);
  assert(stray_ack.count("@TP:02000609000002000100000000000000") == 1);
}
static void test_an01_diagnostic() {
  Machine disconnected;
  assert(!disconnected.dev.test_an01());
  assert(disconnected.count("AN:01") == 0);
  for (const auto &reply : {"ok:", "an:OK"}) {
    Machine m;
    m.begin(false);
    m.dev.configure(false, false, true, 100, "");  // Explicit manual diagnostic only.
    m.tick(16000);
    m.tf = "000000000001";  // SwitchOff Delay active, intentionally not ready.
    m.inject("@TF:" + m.tf);
    assert(m.dev.texts["active_alerts"].find("SwitchOff Delay") != std::string::npos);
    m.an01_reply = reply;
    assert(m.dev.test_an01());
    assert(!m.dev.test_an01());  // No duplicate queued test.
    m.tick(1500);
    assert(m.count("AN:01") == 1);
    assert(m.dev.texts["command_result"] == std::string("AN:01 test response: ") + reply +
                                               "; effect unverified");
  }
  Machine timeout;
  timeout.begin();
  timeout.tick(16000);
  assert(timeout.dev.test_an01());
  timeout.tick(200);
  assert(timeout.dev.active() == "AN:01");
  assert(!timeout.dev.test_an01());  // Reject another test while waiting for a reply.
  timeout.tick(9800);
  assert(timeout.count("AN:01") == 1);
  assert(timeout.dev.texts["command_result"].find("not retried") != std::string::npos);
  timeout.tick(10000);
  assert(timeout.count("AN:01") == 1);

  Machine busy;
  busy.begin();
  busy.tick(16000);
  busy.inject("@TB");
  assert(!busy.dev.test_an01());
  assert(busy.count("AN:01") == 0);

  Machine race;
  race.begin();
  race.tick(16000);
  assert(race.dev.test_an01());
  race.inject("@TB");  // An operation starts after acceptance, before dispatch.
  race.tick(1000);
  assert(race.count("AN:01") == 0);
  assert(race.dev.texts["command_result"].find("cancelled") != std::string::npos);
}

static void test_cooler_override() {
  Machine m;
  m.dev.configure_coolcontrol(15000, 250, "negotiated");
  m.begin(); m.tick(16000);
  assert(m.count("@H1") == 0);
  m.inject("@TF:000480000000");
  assert(!m.dev.brew_named("Milk Portion"));
  m.dev.set_coolcontrol_override(true); m.tick(2000);
  assert(m.count("@H1") == 1 && m.count("@mo") == 1);
  m.inject("@HM:00", false); m.tick(1500);
  assert(m.count("@mo") == 2);
  m.inject("@TF:800480000000"); // Override must not bypass missing tray.
  assert(!m.dev.brew_named("Milk Portion"));
  m.inject("@TF:000480000000");
  assert(m.dev.brew_named("Milk Portion"));
  m.dev.set_coolcontrol_override(false); m.tick(8000);
  assert(m.count("@me1") == 1);
  for (const auto &c : m.commands) assert(c.compare(0, 4, "@TP:") != 0);
}
static void test_milk_brew_checks() {
  for (const auto &tf : {"000600000000", "000500000000", "000480000000"}) {
    Machine m;
    m.begin(); m.tick(16000); m.inject(std::string("@TF:") + tf);
    assert(!m.dev.brew_named("Milk Portion"));
    assert(!m.dev.brew_named("Cappuccino"));
    assert(m.dev.brew_named("Espresso")); // Milk-only alerts do not block coffee.
    Machine selected;
    selected.begin(); selected.tick(16000);
    assert(selected.dev.choose("Milk Portion")); selected.tick(8000);
    selected.inject(std::string("@TF:") + tf);
    assert(!selected.dev.brew_selected());
  }
  Machine race;
  race.begin(); race.tick(16000);
  assert(race.dev.brew_named("Milk Portion"));
  race.inject("@TF:000600000000"); race.tick(8000);
  for (const auto &c : race.commands) assert(c.compare(0, 4, "@TP:") != 0);
  Machine queued;
  queued.begin(); queued.tick(16000);
  assert(queued.dev.choose("Milk Portion")); queued.tick(8000);
  assert(queued.dev.brew_selected());
  queued.inject("@TF:000500000000"); queued.tick(2000);
  for (const auto &c : queued.commands) assert(c.compare(0, 4, "@TP:") != 0);
  queued.inject("@TF:000400000000");
  assert(queued.dev.brew_selected());
}
static void test_milk_error_progress() {
  for (const auto &entry : {std::make_pair("2E", "Blocked: milk cooler error"),
                            std::make_pair("2F", "Blocked: no milk cooler signal")}) {
    Machine m;
    m.begin(); m.tick(16000); m.auto_tf = false;
    m.inject("@TB"); m.inject(std::string("@TV:") + entry.first + "0A");
    assert(m.dev.texts["progress_state"] == entry.second);
    assert(m.dev.bins["brew_active"] == 0 && std::isnan(m.dev.nums["progress"]));
    m.tick(21000);
    assert(m.dev.texts["progress_state"] == entry.second);
    m.inject("@TF:000400000000");
    assert(m.dev.texts["progress_state"] == "Idle");
  }
}
static void test_no_milk_progress() {
  Machine m;
  m.begin(); m.tick(12000);
  m.auto_tf = false;
  m.inject("@TB"); m.inject("@TV:2D0A");
  assert(m.dev.texts["progress_state"] == "Blocked: no milk");
  assert(m.dev.texts["current_product"] == "Milk Portion");
  assert(m.dev.texts["status"] == "blocked");
  assert(m.dev.bins["brew_active"] == 0);
  assert(m.dev.nums["stage_code"] == 0x2D);
  assert(std::isnan(m.dev.nums["progress"]));
  auto queries = m.count("@TF:");
  m.tick(21000);
  assert(m.count("@TF:") > queries);
  assert(m.dev.texts["progress_state"] == "Blocked: no milk");
  assert(m.dev.bins["brew_active"] == 0);
  m.inject("@TF:000600000000"); // No milk + ready (TF bits are MSB first).
  assert(m.dev.texts["active_alerts"].find("no milk (milk sensor)") != std::string::npos);
  m.inject("@TF:000400000000");
  assert(m.dev.texts["progress_state"] == "Idle");
  assert(m.dev.texts["status"] == "ready");
  m.inject("@TB"); m.inject("@TV:320AFFFFFFFF0014FFFFFFFFFF1100FF");
  assert(m.dev.bins["brew_active"] == 1);
  assert(m.dev.texts["progress_state"] == "Dispensing milk");
  m.inject("@TV:3E0A");
  assert(m.dev.bins["brew_active"] == 0 && m.dev.nums["progress"] == 100);
}
static void test_current_product_lifecycle() {
  Machine m;
  m.begin();
  assert(m.dev.texts["current_product"] == "Unknown");
  assert(std::isnan(m.dev.nums["product_code"]));
  m.tick(16000);
  assert(m.dev.texts["current_product"] == "None");
  m.inject("@TV:2322");  // Coffee rinsing, often no ENJOY frame follows.
  assert(m.dev.texts["current_product"] == "Coffee rinsing");
  m.inject("@TF:000400000000");  // Do not erase live TV on an incidental TF.
  assert(m.dev.texts["current_product"] == "Coffee rinsing");
  m.auto_tf = false;
  m.tick(21000);
  assert(m.dev.texts["current_product"] == "Unknown");
  assert(m.dev.texts["progress_state"] == "Unknown (progress timeout)");
  assert(m.dev.bins["brew_active"] == -1);
  assert(std::isnan(m.dev.nums["product_code"]) && std::isnan(m.dev.nums["stage_code"]));
  assert(std::isnan(m.dev.nums["stage_current"]) && std::isnan(m.dev.nums["stage_target"]));
  m.inject("@TF:00");  // An incomplete status cannot establish idle.
  assert(m.dev.texts["current_product"] == "Unknown");
  m.inject("@TF:000400000000");
  assert(m.dev.texts["current_product"] == "None");
  assert(m.dev.texts["progress_state"] == "Idle" && m.dev.bins["brew_active"] == 0);

  m.inject("@TV:2322");
  m.inject("@TV:3E22");
  assert(m.dev.texts["current_product"] == "Coffee rinsing");
  m.inject("@TF:000400000000");
  assert(m.dev.texts["current_product"] == "None");
  assert(std::isnan(m.dev.nums["progress"]));

  m.inject("@TV:2322");
  m.answer_at = false;
  m.tick(61000);
  assert(m.dev.backoff());
  assert(m.dev.texts["current_product"] == "Unknown");
  assert(m.dev.texts["progress_state"] == "Unknown (AT disconnected)");
  m.answer_at = true;
  m.auto_tf = true;
  m.tick(70000);
  assert(m.dev.ready() && m.dev.texts["current_product"] == "None");
}

static void test_coolcontrol_forwarding() {
  Machine disabled;
  disabled.begin(); disabled.tick(12000);
  disabled.dev.coolcontrol_snapshot("@mo:321E", 1, true, disabled.clock);
  disabled.inject("@HM:C0"); disabled.tick(1500);
  assert(disabled.count("@mo:321E") == 0 && disabled.count("@me1") == 0);
  assert(disabled.count("@H1") == 0);
  Machine m;
  m.dev.configure_coolcontrol(15000, 250, "negotiated");
  m.begin();
  m.tick(12000);
  assert(m.dev.ready() && m.count("@t2:8120000000") == 1);
  assert(m.count("@H1") == 0); // No source data yet.
  m.dev.coolcontrol_snapshot("@mn:321E", 1, true, m.clock);
  m.tick(500);
  assert(m.count("@H1") == 1); // Independent of automatic reporting permission.
  assert(m.count("@mn") == 0 && m.count("@mn:321E") == 0);
  m.inject("@HM:80", false);
  m.tick(1500);
  assert(m.count("@mn") == 1 && !m.dev.waiting());
  m.inject("@HM:C0"); m.tick(1500);
  assert(m.count("@mn:321E") == 1);
  m.dev.coolcontrol_snapshot("@mo:321E", 2, true, m.clock); m.tick(1500);
  assert(m.count("@mo:321E") == 1);
  m.dev.coolcontrol_snapshot("@me", 3, true, m.clock); m.tick(1500);
  assert(m.count("@me") == 1);
  m.tick(15000);
  assert(m.count("@me1") == 1 && m.dev.ready());
  m.tick(2000); assert(m.count("@me1") == 1);
  Machine legacy;
  legacy.dev.configure_coolcontrol(15000, 250);
  legacy.dev.coolcontrol_snapshot("@mn:321E", 1, true, legacy.clock);
  legacy.begin(); legacy.tick(12000);
  assert(legacy.count("@mn") == 1 && legacy.count("@mn:321E") == 0);
  legacy.inject("@HM:00", false);
  legacy.dev.coolcontrol_snapshot("@mo:321E", 2, true, legacy.clock);
  legacy.tick(1500); assert(legacy.count("@mo") == 1); // One requested reply despite disable.
  legacy.dev.coolcontrol_snapshot("@mo:321E", 3, true, legacy.clock);
  legacy.tick(1500); assert(legacy.count("@mo") == 1);
  legacy.inject("@HM:C0"); legacy.tick(1500);
  assert(legacy.count("@mo:321E") == 1);
  Machine early;
  early.early_hm = "@HM:C0";
  early.early_hm_encrypted = false;
  early.dev.configure_coolcontrol(15000, 250);
  early.dev.coolcontrol_snapshot("@mo:321E", 1, true, early.clock);
  early.begin(); early.tick(12000);
  assert(early.count("@mo:321E") == 1);
  Machine polled;
  polled.dev.configure_coolcontrol(15000, 250, "negotiated");
  polled.dev.coolcontrol_snapshot("@mn:321E", 1, true, polled.clock);
  polled.early_hm = "@HM"; polled.early_hm_encrypted = false;
  polled.begin(); polled.tick(12000);
  assert(polled.count("@mn") == 1);
  assert(polled.count("@H1") == 1);
  polled.inject("@HM", false); polled.tick(1500);
  assert(polled.count("@mn") == 2);
  polled.dev.coolcontrol_snapshot("@mn:321E", 2, true, polled.clock);
  polled.inject("@HM:00", false); polled.tick(1500);
  assert(polled.count("@mn") == 3);
  polled.dev.coolcontrol_snapshot("@mo", 2, true, polled.clock);
  polled.tick(1500); assert(polled.count("@mo") == 0);
  polled.inject("@HM", true); polled.tick(1500);
  assert(polled.count("@mo") == 1);
  bool logged_request = false, logged_response = false;
  for (const auto &line : polled.dev.traces) {
    logged_request |= line == "Cool Control status request: @HM; cached response queued";
    logged_response |= line.find("Cool Control request response: @mn (plaintext;") == 0;
  }
  assert(logged_request && logged_response);
  assert(polled.count("@H1") == 1); // Heartbeats and polls do not reannounce.
  polled.dev.coolcontrol_snapshot("@mo", 3, false, polled.clock);
  polled.tick(500);
  polled.dev.coolcontrol_snapshot("@mo", 4, true, polled.clock);
  polled.tick(1500);
  assert(polled.count("@H1") == 2); // Reconnect announces even with reporting disabled.
  bool logged_announcement = false;
  for (const auto &line : polled.dev.traces)
    logged_announcement |= line.find("Cool Control announcement: @H1 (plaintext;") == 0;
  assert(logged_announcement);
  Machine three;
  three.challenge = "018000";
  three.dev.configure_coolcontrol(15000, 250);
  three.begin(); three.tick(12000);
  assert(three.count("@t2:81A000") == 1);
  Machine manual;
  manual.dev.configure_coolcontrol(15000, 250);
  manual.begin(true, "8180000000"); manual.tick(12000);
  assert(manual.count("@t2:8180000000") == 1);
}

int main() {
  test_cooler_override();
  test_milk_brew_checks();
  test_milk_error_progress();
  test_no_milk_progress();
  test_coolcontrol_forwarding();
  test_current_product_lifecycle();
  test_an01_diagnostic();
  test_profiles();
  test_cipher();
  test_startup_and_statistics();
  test_short_tf_frame();
  test_ef1092_handshake();
  test_length_based_handshake();
  test_disabled_saved_recipes();
  test_dispensing_stage_progress();
  test_brew();
  test_failure_and_rejection();
  test_banks_and_progress();
  test_at_only_identification();
  test_espresso_probe();
  test_saved_selected_recipes();
  test_api_saved_defaults();
  std::cout << "PASS: all 75 profiles, cipher, statistics, native recipe semantics, brewing lifecycle, busy "
               "rejection, TV liveness, retry/rejection and rollover\n";
}
