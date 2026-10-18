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
  bool answer_type = true, answer_at = true, answer_brew = true, reject_brew = false, auto_tf = true;
  std::vector<std::string> commands;
  std::string probe_reply = "@tm:41,0200040A00000200010000000000000000F4";
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
      if (command == "TY:" && answer_type)
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
int main() {
  test_profiles();
  test_cipher();
  test_startup_and_statistics();
  test_brew();
  test_failure_and_rejection();
  test_banks_and_progress();
  test_at_only_identification();
  test_espresso_probe();
  test_saved_selected_recipes();
  std::cout << "PASS: all 75 profiles, cipher, statistics, native recipe semantics, brewing lifecycle, busy "
               "rejection, TV liveness, retry/rejection and rollover\n";
}
