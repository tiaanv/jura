# Change log

## 0.3.3 (dev)

- Auto handshake response now uses challenge byte length: 3 bytes → `818000`,
  5 bytes → `8120000000`, without exact challenge matching.
- Explicit manual overrides retain priority and must match the challenge length.
- Malformed hex and unsupported auto lengths are rejected without sending a reply.
- Added regression coverage for unfamiliar values, mixed-case hex, unsupported
  lengths, malformed challenges, override precedence and length mismatch.

## Unreleased

- End coffee-rinse progress immediately when a fresh complete TF confirms coffee
  ready and the coffee-rinsing flag is cleared, without waiting for a missing
  Finished frame or the progress timeout. Active drinks and other processes retain
  their TV lifecycle; incomplete or blocked status cannot end a rinse.
- Add regressions for rinse completion, five-byte EF533 compatibility, stale rinse
  identity after a new start, and incidental ready status during real drinks.

- Defer post-finish status/poll traffic until Finished frames have been quiet for
  three seconds, with a fifteen-second recovery bound from the first finish frame.
  Repeated finish frames no longer restart the delayed counter-refresh timer.
- Report TV state `0x4A` as Switching off without a drink identity or dispensing
  quantities. A pending `AN:02` can be confirmed by `@TV:4A02` without an `an:` reply;
  generic busy activity alone does not confirm power-off, and commands are not retried.
- Add regression coverage and record EF533M V01.18 hardware results from issue #31.

- Prioritise explicit Cool Control status replies over queued commands and routine
  polling after the active UART transaction finishes. Requested replies bypass
  the automatic status throttle and use the last coherent fresh snapshot while
  new Home Assistant inputs settle; offline and stale handling is unchanged.
- Add regression coverage for cooler requests during polling, coalescing and
  rapid successive requests, including retained updates and unavailable sources.

- Accept empty `@tp:` brew acknowledgements, as reported on the E8 EF1092,
  while retaining rejection handling for nonempty responses such as `@tp:00` (#35, #36).
- Cover both acknowledgement forms through product confirmation and completion,
  with and without a separate start announcement; verify pending requests block
  overlapping brews, are not resent, and ignore stray acknowledgements during settings reads.

- Publish the detected profile's recipe capabilities for all selectable drinks as
  bounded diagnostic text states with a versioned, checksummed manifest. This
  needs no extra UART polling and allows card validation before drink selection.
- Verify capability serialization against all 75 profiles, UTF-8 chunk boundaries,
  codegen entity capacity and native adapter publication.

- Add `read_saved_recipes` (default true). Disabling automatic recipe reads uses
  profile defaults plus one-shot overrides for native/API brewing, with explicit
  source/status reporting. The manual diagnostic probe remains available.
- Relabel the EF533 native raw extra as Legacy Coffee Rinse Count (unverified),
  retaining its `brew_movements` registration key and raw value.
- Add `Jura Stage Progress` for dispensing current/target quantities without
  changing overall `Jura Progress`. Card fallback requires a separate update.

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
