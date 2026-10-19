# Change log

## 0.3.3 (dev)

- Auto handshake response now uses challenge byte length: 3 bytes → `818000`,
  5 bytes → `8120000000`, without exact challenge matching.
- Explicit manual overrides retain priority and must match the challenge length.
- Malformed hex and unsupported auto lengths are rejected without sending a reply.
- Added regression coverage for unfamiliar values, mixed-case hex, unsupported
  lengths, malformed challenges, override precedence and length mismatch.

## Unreleased

- `handshake_response: auto` now answers a 1-byte challenge with `81`
  (byte 0 = challenge[0] | 0x80, same length), as sent by the E8 EF533M V01.18
  (`@T2:01` → `@t2:81` → `@T3:…EF533M V01.18`), verified on hardware.
- New opt-in `accept_short_status` option for the observed five-byte EF533 TF
  format (E8 EF533M V01.18). The absent sixth byte no longer prevents readiness;
  its alert entities remain unknown and present blocking alerts still block.
  Shorter frames and other profiles retain normal completeness checks. Default
  unchanged; regression tests cover the restricted exception.
- `handshake_response: auto` now answers the E8 EF1092M V01.07 challenge
  `01C041B628` with `8120000000`, verified on hardware; no override needed.

## 0.3.2 (dev)

- Clear stale Current Product, product/stage codes, quantities and progress on
  startup, reconnect, session loss and progress timeout, including rinses that
  do not send a recognised finish frame.
- Publish Unknown while operation state is unavailable; fresh TF status with
  no active TV operation publishes None/Idle. Live TV remains authoritative.

## 0.3.1 (dev)

- Added a manual native `test_an01()` method and YAML test button for observing
  shutdown-delay behaviour. Sends only AN:01 through the serial queue, logs
  replies without claiming wake success, and never automatically retries.
- Diagnostic permits a connected, blocked shutdown-delay state but rejects
  active operations or pending controls and rechecks before dispatch.

## Legacy documentation and examples

- Aligned the legacy documentation with the existing non-blocking,
  framework-independent implementation already on main.
- Legacy buttons now use `queue_command()`; queue acceptance is not a brew
  acknowledgement, and commands are not automatically retried.
- Updated legacy E6/E8, F6 and F7 examples and added a C3/ESP-IDF E8 example.
- Added legacy queue regression coverage and C3 IDF/Arduino schema checks.
- Updated Git examples to `ref: main` following the V2 merge and branch deletion.
- No change to legacy or native component firmware, schemas or sensor keys.

## Legacy component installation compatibility

- Restored the unchanged legacy source to `components/jura`, so Git users can
  continue loading `components: [jura]` on V2 and after its merge into `main`.
- Historical documentation and examples remain under `legacy/`.
- Documented continued legacy use separately from optional native migration.

## Hardware and documentation visuals

- Restored the original connector reference and hardware connection guidance.
- Added a 1 kΩ / 2 kΩ divider diagram for Jura TX to ESP32-C3 RX, direct 3.3 V
  TX guidance, shared ground and power notes matching the GPIO4/GPIO3 example.
- Added the owner's native-controls screenshot and a labelled monitoring illustration.
- Documentation images are included in source packages and checksums.
- No firmware or device YAML changes are required by this documentation update.

## Repository layout update

- Current guides moved under `docs/`; root README now describes the native V2 component.
- Current ESPHome configurations and HA actions live under `examples/`.
- Profile exports live under `docs/profiles/`; export and packaging tools live under `tools/`.
- Historical examples and documentation are archived under `legacy/`; the legacy
  source remains installable at `components/jura` following the compatibility update.
- Documentation marks all inherited raw-register interpretations as unverified.
- Portable tool paths, example validation, source packaging and documentation links updated.
- No change to native firmware behaviour, component install paths or entity names.

## 0.3.0

Named/custom API actions read the requested product's saved settings before
brewing. Explicit custom fields override saved values; read failures use a
labelled profile-default fallback. Readiness is rechecked and pending preparation
is cancelled if an operation begins or the connection/readiness is lost.

## 0.2.0

Selecting a drink loads its saved settings into native controls. Source/loading/
modified state and fallback reasons are exposed. Native Brew consumes edits for
one accepted request; Reset Recipe reloads the machine's saved recipe. Selection
and reset races discard obsolete replies.

## 0.1.x

Initial native component with the full bundled profile database, TF/TV monitoring,
profile-labelled counters, native controls and nonblocking transactions. Follow-up
fixes cover AT-only boot identification and read-only saved-espresso diagnostics.
