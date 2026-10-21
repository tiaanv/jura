# Development, testing and validation

## Current evidence

Reference environment: ESPHome **2026.9.1**, ESP32-C3 / ESP-IDF,
Jura E8 **EF533M V02.09**. Validation was performed during the 0.1–0.3 development
on 2026-10-18. The later legacy documentation correction adds example validation
and queue tests without changing either component's firmware.

| Check | Status and scope |
| --- | --- |
| Portable C++ controller tests | Pass with C++17, `-Wall -Wextra -Werror` |
| All 75 profile records | Parse, longest EF match and active default recipe validation |
| ESP8266 flash reader | Same host suite with `USE_ESP8266` and a pgmspace shim; not an ESP8266 binary |
| Native adapter tests | Pass with lightweight ESPHome headers; include AT handshake and typed recipe controls |
| ESPHome schemas | Native C3 IDF/Arduino and ESP8266, monitor-only/profile hint, separate Cool Control UART; legacy C3 IDF/Arduino, ESP8266 and model button examples |
| Legacy queue tests | FIFO, limits, poll coalescing, wire encoding, split RX, publishing, timeouts, recovery, invalid input and rollover |
| Invalid configs | Bad baud, handshake hex and unknown profile rejected |
| C++ generation | Complete E8 actions/entities and Cool Control example |
| Real C3 compiler check | Generated source checked with riscv32-esp-elf GCC 14.2.0 and actual ESP-IDF/ESPHome headers using `-fsyntax-only` |
| Legacy C3 compiler check | Generated legacy E8 configuration and queued button lambdas pass the same real ESP-IDF compiler syntax check |
| Physical E8 observations | Successful AT startup, TF ready/alerts, product/maintenance reads and TV espresso stage/progress logs |
| Saved espresso water read | Screen change 45 → 60 → 45 ml followed by valid `@TM:41,02` responses |
| Physical saved-default API brewing | Still needs acceptance testing after the 0.3 update |
| Physical E8 EF1092M V01.07 | AT handshake with challenge `01C041B628` → `8120000000`, saved Flat White read, full Flat White brew with TV stages/progress, TF alerts and AT counters (ESP32-C3 / ESP-IDF, 0.3.2) |
| Physical E8 EF1092 Cool Control bridge | Confirmed on `milky` from owner-supplied logs reviewed 2026-10-21: `@HM:80` requests receive cached bare `@mn`; two Milk Foam attempts report `@TV:2D08` (no milk), clear brew activity and refresh TF without a progress timeout. A later forwarded `@mo` clears the machine's milk/no-milk alerts. Advancing HA counters maintain freshness without duplicate notifications |
| Physical E8 EF533M V01.18 | AT handshake with 1-byte challenge `01` → `81`, TF alerts, AT product/maintenance counters and TV coffee stages/progress (classic ESP32 / Arduino, ESPHome 2026.9.1, 0.3.3). `@TM:41` saved-recipe reads time out on this firmware |
| Dev AN:01 diagnostic | Host-tested blocked-TF dispatch, offline/busy/duplicate rejection, replies, cancellation and no retry; generated YAML checked with the C3 IDF compiler; physical effect unverified |
| Other models/parameters | Not established by their presence in the database |
| Arduino/ESP8266 firmware and RAM usage | Classic ESP32 (`esp32dev`) Arduino: complete build, OTA flash and run on the EF533M V01.18 above — RAM 59,776 B (33.1%), flash 1,128,455 B (61.5%) with `raw_extras: true`. ESP8266 and C3 Arduino not established |
| New Cool Control receiver | Host-tested parsing/stale expiry; no fresh physical validation |

The compiler syntax check is not a complete firmware link or flash test. The
user has compiled/uploaded earlier V2 revisions; do not infer that every current
control path or model has been exercised on hardware.

### EF1092 Cool Control bridge hardware evidence

The supplied capture starts with reporting already negotiated; it does not
include the startup T2/T3 handshake or `@H1` announcement. It confirms the
following machine-side behavior:

- At 19:34:21 and 19:34:42, `@HM:80` requests are answered with cached `@mn`.
  Each Milk Foam attempt then produces `@TV:2D08`, handled as **Blocked: no milk**
  with brew activity OFF and a fresh TF query, rather than a progress timeout.
- At 19:34:52, the cooler changes to `@mo`; forwarding it changes TF from
  `01060000000000` to `00040000000000`, clearing the milk and no-milk alerts
  within approximately half a second.
- HA counter updates continue without repetitive unchanged status transmissions.
  Temperature/level changes are suppressed on the machine link because `@HM:80`
  selects bare reporting. Explicit requests still receive unchanged replies.

No command or progress timeouts appear in this capture. It does not show a
successful milk preparation after recovery, extended reporting, loss/recovery
of the cooler connection, or override-switch operation on EF1092. The exact
software version is not present in this capture; do not infer it from the
separate EF1092M V01.07 test above.

## Regression coverage

- Opt-in five-byte EF533 status: default rejection, ready/heating and present
  blocking alerts, rejection of two-to-four-byte frames and five-byte EF545
  frames, and preservation of full-frame sixth-byte blocking alerts.
- Cipher and encoded framing, all-byte/key round trips, escaping, timeouts and rollover.
- Legacy non-blocking queue behaviour, including actual header execution with fake UART/time.
- Current-product cleanup after rinse progress silence, recognised finish, fresh TF,
  disconnect/reconnect and startup; live TV is preserved on incidental TF frames.
- Boot identification through TY or AT T3, bounded missing-device discovery,
  challenge overrides/backoff and persistent model mismatch handling.
- Product/maintenance/raw parsing, higher product banks, EF545 counter remap,
  unknown sentinel handling, refresh deduplication and power-off.
- Exact recipe vectors, displayed choice labels, parameter ranges/steps/layouts.
- Single pending brew, busy rejection, matching TV start, rejection/unconfirmed
  outcomes, no automatic brew retry, TF/TV freshness and progress windows.
- Espresso diagnostic read validation and repeated real EF533 response vectors.
- Selected saved recipes, partial/invalid/unsupported/timeout fallback, one-shot
  UI edits, reset, preserved edits during reads and obsolete reply/timeout races.
- Fresh named/custom API defaults, explicit overlays, independent product snapshots,
  fallback provenance, duplicate/invalid calls, readiness/busy/disconnect cancellation,
  late replies and unrelated ACKs during preparation.

EF562 / 2 Ristretti has an inconsistent upstream default temperature (0, while
allowed choices are Normal=1 / High=2). The tests cover rejection of that default
and an explicit valid override; the database is not silently corrected.

## Run the host tests

Use Python and a C++17 compiler:

```sh
python tests/run_tests.py
```

The default compiler is `g++`. Set `CXX` to a `clang++` or `g++` executable path
if needed. Executables are created under ignored `tests/build/`. The tests use
small adapter headers under `tests/esphome/`; those are not firmware dependencies.

## Validate ESPHome configurations

Install ESPHome 2026.9.1 and its dependencies in your development environment:

```sh
python tests/validate_schema.py
python -m esphome compile examples/e8-local.yaml --only-generate
python -m esphome compile examples/coolcontrol-local.yaml --only-generate
```

For a full target build, omit `--only-generate`. The script writes temporary
configurations under ignored `tests/generated-configs/`; it does not flash a device.
The local examples reference `../components` relative to their YAML files.
There are no task-specific Python runtime paths in the repository scripts.

## Maintain documentation and exports

```sh
python tools/export_profiles.py
python tools/check_docs.py
python tools/package_release.py
```

The exporter regenerates the readable JSON/model index from the bundled runtime
blob. The checker verifies local Markdown targets and YAML source paths. Packaging
writes an ignored ZIP under `dist/`, includes the documented repository layout,
regenerates `SHA256SUMS`, and verifies archive integrity/exclusions. The archived
legacy documentation/examples are included for historical reference. The legacy
`jura` source is included separately at its installable `components/jura` path.

Documentation diagrams can be regenerated with `python tools/render_doc_visuals.py`
after installing Pillow and a Segoe UI or DejaVu Sans font. This regenerates only
the wiring diagram and labelled monitoring illustration. The user screenshot and
original connector reference are retained unchanged. Image files are included in
the release package and checksum manifest.

## Physical acceptance checks

1. Confirm model, connected/ready states, alerts and plausible AT counts.
2. Select each drink you intend to use; compare every supported saved setting
   with the machine screen and inspect source/status/raw on fallback.
3. Change a single saved screen value, leave menus, reload and verify the field.
4. Test one native one-shot override and verify the controls return to baseline
   and the machine's saved recipe is unchanged.
5. Test saved/custom API actions separately, following Command Recipe Source,
   Command Result and matching TV progress. Never auto-repeat an unconfirmed brew.
6. Verify restart and power-cycle recovery with your actual wiring/power setup.
