#include <cassert>
#include <iostream>
#include "../components/jura_native/jura_coolcontrol_bridge.h"
using esphome::jura_native::CoolcontrolBridge;
int main() {
  CoolcontrolBridge b;
  b.set_override(true); assert(!b.overridden()); // No bridge means no bypass.
  assert(!b.take_announcement());
  std::string out;
  b.snapshot("@mo", 1, true, 0); b.advance(500);
  assert(!b.next(500, out));
  b.configure(15000, 250);
  b.snapshot("@mo:321E", 1, true, 1000); b.advance(1250);
  assert(b.take_announcement() && !b.take_announcement());
  assert(!b.next(1250, out)); // No reporting before machine negotiation.
  b.negotiate(0x80);
  assert(b.next(1250, out) && out == "@mo");
  b.snapshot("@mo:321E", 2, true, 2000); b.advance(2250);
  assert(!b.next(2250, out)); // Heartbeat refreshes freshness without duplicate wire traffic.
  b.negotiate(0xC0);
  assert(b.next(2251, out) && out == "@mo:321E");
  b.request_status();
  assert(b.next(2252, out) && out == "@mo:321E"); // Explicit replies bypass the automatic throttle.
  b.snapshot("@mo:321E", 3, true, 4000); // Counter arrives before raw entity.
  b.advance(4100); assert(!b.next(4100, out));
  b.snapshot("@mn:0028", 3, true, 4150);
  b.advance(4300); assert(!b.next(4300, out));
  b.advance(4400); assert(b.next(4400, out) && out == "@mn:0028");
  b.snapshot("@mn:0028", 0, true, 6000); // Counter reboot/wrap is a change too.
  b.advance(6250); assert(!b.next(6250, out));
  b.snapshot("@mn:0028", 0, true, 21000); b.advance(21000);
  assert(b.next(21000, out) && out == "@me1"); // Unchanged HA state is not liveness.
  b.advance(23000); assert(!b.next(23000, out));
  b.snapshot("@mo", 1, true, 24000); b.advance(24250);
  assert(b.next(24250, out) && out == "@mo");
  b.snapshot("@mo", 1, false, 25000); b.advance(25000);
  assert(!b.next(25000, out));
  assert(b.next(25250, out) && out == "@me1");
  b.snapshot("@mo:FF28", 2, true, 26000); b.advance(26250);
  assert(b.next(26250, out) && out == "@mo"); // Unknown level uses bare status.
  b.snapshot("@mo\r\n@TP:02", 3, true, 28000); b.advance(28000);
  assert(b.next(28000, out) && out == "@me1");
  b.snapshot("@me", 4, true, 30000); b.advance(30250);
  assert(b.next(30250, out) && out == "@me");
  b.negotiate(0); b.snapshot("@mo", 5, true, 32000); b.advance(32250);
  assert(b.next(32250, out) && out == "@mo");
  assert(!b.next(33250, out));
  b.reset_session(); b.negotiate(0xC0);
  assert(b.next(33000, out) && out == "@mo"); // Fresh snapshot survives machine reconnect.
  for (auto s : {"@mo:651E", "@mx", "@me:0000", "@mn:321G", "@mn:321E\n", "unknown"})
    assert(!CoolcontrolBridge::valid(s));
  assert(CoolcontrolBridge::valid("@me1"));
  CoolcontrolBridge fast;
  fast.configure(15000, 250); fast.negotiate(0xC0);
  for (uint32_t t = 0; t <= 500; t += 100) {
    fast.snapshot("@mo", float(t + 1), true, t);
    fast.advance(t);
  }
  assert(fast.next(500, out) && out == "@mo"); // Frequent heartbeats cannot postpone forever.
  CoolcontrolBridge priority;
  priority.configure(15000, 250); priority.negotiate(0xC0);
  priority.snapshot("@mo:321E", 1, true, 0); priority.advance(250);
  assert(priority.next(250, out) && out == "@mo:321E");
  priority.snapshot("@mn:0028", 2, true, 300);
  priority.request_status();
  bool requested = false;
  assert(priority.next(301, out, &requested, true) && requested && out == "@mo:321E");
  priority.advance(550);
  assert(!priority.next(550, out)); // Automatic update remains throttled.
  assert(priority.next(1301, out) && out == "@mn:0028"); // Pending new state was retained.
  priority.snapshot("@mo", 3, false, 1302); priority.request_status();
  assert(priority.next(1302, out, nullptr, true) && out == "@me1"); // Never reuse offline cache.
  priority.snapshot("@mo", 4, true, 1400); priority.advance(1650);
  priority.advance(16400); priority.request_status();
  assert(priority.next(16400, out, nullptr, true) && out == "@me1"); // Nor expired cache.
  CoolcontrolBridge initial;
  initial.configure(15000, 250); initial.request_status();
  initial.snapshot("@mo", 1, true, 0);
  assert(!initial.next(1, out, nullptr, true)); // No coherent cache yet.
  initial.advance(250); assert(initial.next(250, out, nullptr, true) && out == "@mo");
  CoolcontrolBridge automatic;
  automatic.configure(15000, 250); automatic.set_model("EF533"); automatic.reset_session();
  automatic.snapshot("@mo:321E", 1, true, 0); automatic.advance(250);
  assert(automatic.next(250, out) && out == "@mo");
  automatic.negotiate(0); automatic.snapshot("@mn", 2, true, 1500); automatic.advance(1750);
  assert(automatic.next(1750, out) && out == "@mn"); // HM disable still requests one reply.
  assert(!automatic.next(2750, out));
  automatic.reset_session(); assert(automatic.next(2000, out) && out == "@mn");
  CoolcontrolBridge other;
  other.configure(15000, 250); other.set_model("EF999"); other.reset_session();
  other.snapshot("@mo", 1, true, 0); other.advance(250); assert(!other.next(250, out));
  CoolcontrolBridge forced;
  forced.configure(15000, 250, CoolcontrolBridge::ReportingMode::LEGACY);
  forced.reset_session(); forced.snapshot("@mo", 1, true, 0); forced.advance(250);
  assert(forced.next(250, out) && out == "@mo");
  CoolcontrolBridge strict;
  strict.configure(15000, 250, CoolcontrolBridge::ReportingMode::NEGOTIATED);
  strict.set_model("EF533"); strict.reset_session(); strict.snapshot("@mo", 1, true, 0);
  strict.advance(250); assert(!strict.next(250, out));
  strict.request_status(); assert(strict.next(250, out) && out == "@mo");
  strict.request_status(); assert(strict.next(251, out) && out == "@mo");
  strict.snapshot("@mn", 2, true, 1500); strict.advance(1750);
  assert(!strict.next(2250, out)); // Bare poll did not enable automatic reporting.
  strict.advance(17000); strict.request_status();
  assert(strict.next(17000, out) && out == "@me1");
  strict.request_status(); assert(strict.next(18000, out) && out == "@me1");
  assert(!strict.take_announcement()); // Expired data must not advertise a cooler.
  strict.snapshot("@mo", 3, true, 19000);
  assert(!strict.take_announcement()); // Wait for coherent input.
  strict.advance(19250); assert(strict.take_announcement());
  strict.reset_session(); assert(strict.take_announcement());
  assert(!strict.take_announcement());
  CoolcontrolBridge dedup;
  dedup.configure(15000, 250); dedup.negotiate(0x80);
  dedup.snapshot("@mo:321E", 1, true, 0); dedup.advance(250);
  assert(dedup.next(250, out) && out == "@mo");
  dedup.snapshot("@mo:1428", 2, true, 2000); dedup.advance(2250);
  assert(!dedup.next(2250, out)); // Stripped telemetry changes do not change bare status.
  dedup.request_status(); assert(dedup.next(2250, out) && out == "@mo");
  dedup.negotiate(0xC0); assert(dedup.next(3250, out) && out == "@mo:1428");
  dedup.snapshot("@mo:1432", 3, true, 4000); dedup.advance(4250);
  assert(dedup.next(4250, out) && out == "@mo:1432"); // Extended telemetry change is forwarded.
  dedup.advance(19000); assert(dedup.next(19000, out) && out == "@me1");
  dedup.snapshot("@mo:1432", 4, true, 20000); dedup.advance(20250);
  assert(dedup.next(20250, out) && out == "@mo:1432");
  CoolcontrolBridge override;
  override.configure(15000, 250); override.negotiate(0);
  override.set_override(true);
  assert(override.overridden() && override.take_announcement());
  assert(override.next(0, out) && out == "@mo");
  for (uint32_t t = 1000; t < 4000; t += 1000) {
    override.snapshot("unknown", NAN, false, t); override.advance(t);
    assert(!override.take_announcement() && !override.next(t, out));
  }
  override.request_status(); assert(override.next(4000, out) && out == "@mo");
  override.snapshot("@mn:0028", 1, true, 5000); override.advance(5250);
  override.set_override(false);
  assert(override.next(5250, out) && out == "@mn"); // Cached source survived override.
  override.set_override(true); override.reset_session(); override.negotiate(0xC0);
  assert(override.take_announcement());
  assert(override.next(6000, out) && out == "@mo"); // Never fabricate telemetry.
  override.advance(21000); override.set_override(false);
  assert(override.next(21000, out) && out == "@me1"); // Expiry still tracked during override.
  std::cout << "PASS: cooler bridge negotiation, coalescing, freshness, expiry and input validation\n";
}
