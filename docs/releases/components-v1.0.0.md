# Components 1.0.0

First versioned stable baseline for the **jura_native** and **jura_coolcontrol**
ESPHome components, released from main on 2026-10-31. This establishes a bounded
source release of the existing functionality; it is not a claim that all models
or features have been tested on hardware.

## Jura Native

- Native Home Assistant monitoring and controls without MQTT, with automatic
  model identification and 75 bundled profiles.
- Machine status and alerts, drink/stage progress, product/maintenance counters,
  saved-recipe reads, one-shot custom recipes and native brewing actions.
- Serialized UART operations, readiness checks, bounded timeouts and no automatic
  brew retry. Supports the observed short EF533 status format through an opt-in.
- Milk-aware brew checks, cooler request priority and blocked milk-attempt handling,
  including progress states 0x2D, 0x2E and 0x2F.
- Completed coffee-rinse handling based on fresh ready status, with protection for
  active drinks; repeated finish frames and observed shutdown progress handling.
- Empty brew acknowledgements accepted for the observed EF1092 behaviour.

## Cool Control and bridge

- Separate UART receiver for milk level, temperature and cooler status, including
  older status-only frames and communication timeout handling.
- Optional Home Assistant bridge using raw message, message counter and online
  entities. Cached status replies support machine cooler requests; routine
  unchanged transmissions are reduced.
- Persistent manual healthy-state override for use when the cooler connection is
  unavailable. Enabling it bypasses the bridge's milk-availability protection.
- Without bridge configuration, Jura Native continues normal standalone operation.

## Installation and upgrade

Use `ref: components-v1.0.0` in external_components on each device, selecting
`jura_native` or `jura_coolcontrol` as appropriate. See the
[release guide](../releases.md) for a complete source example and rollback steps.
Existing main configurations need no YAML migration for this baseline; rebuild
and upload after changing the source ref. Matching component versions are
recommended when using the bridge.

The direct UDP/packet_transport bridge currently on dev is **not included**.
This release uses the Home Assistant entity bridge and retains its optional
counter sensor. Users testing the direct bridge should remain on dev until a
later component release includes it.

## Compatibility and validation

The existing validation record includes ESPHome 2026.9.1, portable C++ regression
coverage, schema/code-generation checks and compiler syntax checks. Hardware
observations cover E8 EF533M V02.09, EF533M V01.18 and EF1092; the Cool Control
bridge has been confirmed on EF533 and EF1092. These observations span development
revisions and do not mean every feature was retested on every machine for 1.0.0.
Release preparation reran the portable controller, native adapter, cooler, bridge,
legacy and ESP8266 flash-reader host suites, plus the catalogue comparison against
all 75 profiles. Documentation links and the unchanged card bundle were checked.
No new hardware flash test was performed for the tag.
See [testing evidence and limitations](../testing.md) for the precise scope.

A bundled profile does not establish hardware compatibility. ESP8266 and C3
Arduino full firmware operation remain unestablished; saved-recipe reads time out
on the observed EF533M V01.18 firmware, where disabling automatic reads is the
available workaround. Some recent lifecycle fixes still require hardware
confirmation on that firmware. No precompiled firmware is supplied.

## Card, notices and safety

The graphical card is independently versioned and released. This component
release does not publish or upgrade the card. Existing card files in the source
snapshot retain their own version; use the separate card release for installation.

This is an unofficial community project. Read the [disclaimer](../disclaimer.md)
and [contributor notices](../notices.md). Existing licence and attribution terms
continue to apply.
