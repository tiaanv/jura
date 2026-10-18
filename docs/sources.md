# Sources and licensing

## harryzz model database and protocol reference

The native component uses the full bundled **JPRF v4, 75-profile database** from
[harryzz/jura-uart-esphome](https://github.com/harryzz/jura-uart-esphome/tree/main/components/jura_link),
inspected on 2026-10-18. The embedded data bytes are preserved; the local header
adds flash storage annotations for ESP8266.

- [Upstream profile blob](https://github.com/harryzz/jura-uart-esphome/blob/main/components/jura_link/jura_profiles_blob.h)
- Retrieved profile Git blob SHA: `d8b98e15e5e388899ff5d06b027b7b1c858ff553`
- Retrieved cipher Git blob SHA: `6c8765202a22aed2eaea2ff783058637fe948cf0`
- Licence: MIT, copyright (c) 2026 harryzz; the
  [full original notice](../components/jura_native/LICENSE.harryzz) is retained.

The cipher/framing port includes strict unescape/frame validation. Known progress
names draw on the upstream glossary; unknown stages retain their raw code.
The Python/C++ profile readers, controller and native entity layer are implemented
here. MQTT discovery and the upstream MQTT command router are not dependencies.

## Saved recipes and other AT commands

[makefu/jura-connect protocol notes](https://github.com/makefu/jura-connect/blob/main/docs/PROTOCOL.md)
and [client implementation](https://github.com/makefu/jura-connect/blob/main/jura_connect/client.py)
provide the read-only `@TM:41,<code>` format, echoed product/checksum validation
and full `@TP` recipe reference. The local decoder and nonblocking queue are C++.
Those sources distinguish live captures from APK-derived or untested behaviour;
their evidence must not be expanded into a claim that every model works here.

Legacy FA commands represent model-specific button mappings. They are different
from AT product codes. The native actions use a full recipe, not an assumed
universal ID-only AT preset command.

## This project's history and device evidence

The [legacy archive](../legacy/README.md) preserves the old component, its credits
and register hypotheses. Earlier work credits
[Ryan Alden](https://github.com/ryanalden/esphome-jura-component),
[alco28/Jura-F7-ESPHOME](https://github.com/alco28/Jura-F7-ESPHOME),
[Jutta-Proto](https://github.com/Jutta-Proto/protocol-cpp) and other contributors
listed in the archived README.

Tiaan's supplied EF533M V02.09 logs establish the tested AT challenge/startup,
TF ready information, TV espresso stages/progress and observed AT counters.
Saved espresso water followed a screen change of **45 → 60 → 45 ml**.
All old semantic raw-register labels remain unverified; a correlated change is
not an established meaning. See [diagnostics](diagnostics.md#raw-registers).

## Implementation distinctions

- Read banks follow the selected profile's product codes, including above `0B`.
- EF545 product counters preserve the `31/36 → 12/13` remap with a direct fallback.
- Unsupported sentinel values remain unavailable rather than zero.
- TV liveness and TF alert freshness are separate; idle reads pause while busy.
- Native entities are allocated at code generation and finalised during setup.
- Saved recipes use explicit fallback provenance, one-shot edits and request
  snapshots. Readiness is checked before transmission; no brew is auto-retried.
- The runtime never downloads profiles and never writes saved recipes/EEPROM.

## Known database inconsistency

EF562 / 2 Ristretti declares temperature default 0 while allowed choices are
Normal=1 and High=2. The invalid default is rejected rather than silently changed.
An explicit valid temperature works in the recipe tests. All other active
profile-default recipes validate in the bundled database.

## Licence

The repository is MIT licensed; see [LICENSE](../LICENSE) and the retained
[upstream notice](../components/jura_native/LICENSE.harryzz).
