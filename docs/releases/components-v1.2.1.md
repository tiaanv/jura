# Components 1.2.1

Released on 2026-11-19 as the immutable Git tag `components-v1.2.1`.

## ESPHome 2026.11 compatibility

Fix compilation of runtime profile sensor and alert names after ESPHome changed
the entity name field from `StringRef` to `ProgmemStringRef`.
The assignment now constructs the underlying reference from the owned string's
pointer and length. This supports both name-reference types and retains ownership
of the backing string in the profile entity. The fix was introduced in
`d8e2dda` and addresses [issue #38](https://github.com/tiaanv/jura/issues/38).

Native adapter regressions cover both reference types, temporary input strings,
reassignment and visibility. Additional host checks exercise the combined
ESP8266/new-name-type path and the real ESPHome 2026.11 `ProgmemStringRef`
ESP8266 branch with host memory-access shims. The patched adapter also passes
a GCC 14.2.0 syntax check using the actual failing ESP32-C3 build's ESPHome
2026.11.0 / ESP-IDF 5.5.6 headers and compiler flags.

## Successful full ESP8266 build

A complete firmware build succeeded on 2026-11-19 with the following configuration.

| Item | Result |
| --- | --- |
| ESPHome | 2026.11.0 |
| Target | ESP8266 NodeMCU, `nodemcuv2` |
| Framework | Arduino 3.1.2-esphome.4 |
| Component revision tested | `d8e2dda` |
| Included functionality | Full `jura_native`, native profile entities, `profile_hint: EF533`, `raw_extras: true`, Wi-Fi and brewing/recipe-refresh API actions |
| Build stages | C++ compilation, ELF linking and BIN generation passed |
| RAM | 53,924 / 81,920 bytes (65.8%) |
| Flash | 535,276 / 1,044,464 bytes (51.2%) |

Physical ESP8266 flashing, runtime behaviour and coffee-machine communication
remain untested. These build results apply to the configuration above.
See the [testing record](../testing.md) for validation scope.

## Updating

Release pinning remains optional. Use `ref: components-v1.2.1` for this fixed
snapshot, or continue following `main`. Refresh the external component source
as needed, rebuild and upload the Jura ESP. Existing component YAML and API
actions continue to work.

This patch changes `jura_native` entity naming. Existing Cool Control sender
firmware can continue to communicate with the updated receiver through the
supported HA or direct UDP bridge. The shared tag also includes the
`jura_coolcontrol` source snapshot.

The graphical card is separately versioned. Component versions are published
as Git tags with repository notes under the [release policy](../releases.md).
Read the [disclaimer](../disclaimer.md) and [contributor notices](../notices.md).
