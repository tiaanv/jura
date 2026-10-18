Jura Native 0.3.0 — testing release
======================================

Native ESPHome integration using harryzz's complete 75-profile JPRF v4 database.
No MQTT broker, MQTT discovery, additional HA integration or profile-download
service is required. The profile database ships with the component.

Target/reference: ESPHome 2026.9.1, ESP32-C3, ESP-IDF, EF533M V02.09 (Jura E8).
Arduino and ESP8266 configuration validation also passes. This release has not
been flashed to an ESP8266 or checked for its final flash/RAM consumption.
The protocol code and flash-reader branch are covered by host tests.

INSTALL
-------
The complete component is available on the V2 branch of tiaanv/jura.
Use ref: V2 and components: [jura_native] in external_components.
Keep the existing components/jura directory until you finish migrating devices.
Change the YAML as described in MIGRATION.txt. example-e8.yaml is a complete
local-source build example; migration-e8.yaml is the GitHub-source merge snippet.
Preserve the existing device name, Wi-Fi, API encryption, secrets and OTA config.

Only one Jura component may consume the machine's UART. Remove the old jura:
section and raw UART drink-button automations when enabling jura_native:.

WHAT YOU GET
------------
* Automatic EF profile selection and model/version reporting.
* TF status and individual profile-specific alerts, including maintenance due
  alerts where the model provides them. Status and Status Detail combine these.
* TV product names, stage descriptions, overall progress and available stage
  current/target values. Water uses ml; milk and pauses use seconds. Unknown
  stages keep their raw code and do not invent quantities.
* Named product counters, model-labelled maintenance counts/percentages and
  total products. Unsupported responses remain unavailable rather than zero.
* Native Drink selector, supported strength/temperature/grinder selectors,
  recipe number controls, Brew, Reset Recipe, Refresh and Turn Off buttons.
* Native API actions for named drinks and an atomic custom recipe request.
* EF533 extras retained from your component: single/double espresso counters,
  grounds, brew movements, brews since cleaning and since descaling. Sixteen
  raw counters and a change summary remain available for investigation.
* Optional separate nonblocking Cool Control receiver (see example file).
* Nonblocking UART/session state machines, bounded reception, quiet time,
  handshaking, timeout recovery and a serialised read/command queue.

DRINK CONTROLS
--------------
Select a named drink, wait for Jura Recipe Loading to turn off, edit supported
settings, then press Jura Brew. Selection loads the machine's saved recipe.
Unsupported/invalid/timed-out reads use clearly labelled profile defaults.
Reset Recipe discards HA edits and reads the selected drink again. A successful
Brew queue request consumes the HA edits and restores the baseline controls.
Rejected requests retain edits. See SAVED_RECIPES.txt for details and test steps.

Strength, temperature and grinder ratio use the displayed profile labels. For
example EF533 strength "6" is encoded differently from raw value 6. Do not use
raw protocol codes for these choices. Strength is a level, not grams of grounds.
The interface exposes milk duration, not a guessed milk volume.

Native number bounds cover the union of drinks in the selected profile because
HA learns entity traits at connection time. The component checks the selected
drink's actual bounds/step before accepting a value or brewing. Per-drink min,
max, step and choice-list sensors allow your card to show only valid settings.
A setting unsupported by the current drink has an unavailable number state or
"Not available" choice. A rejected setting leaves the previous recipe intact.

brew_named(name)/brew_drink read that product's saved settings before brewing.
brew_custom() uses the same saved baseline, then overlays explicit arguments.
Empty choice strings (or "default") and numeric -1 mean machine-saved defaults,
with a labelled all-or-nothing profile fallback if the read fails. These actions
read afresh even when another drink is selected in HA; they never use or consume
unrelated native control edits. Names/arguments in existing YAML stay the same.

brew_selected() / Jura Brew use the currently loaded/edited native controls.
All paths send one-shot @TP recipes. No EEPROM or saved-recipe write is added.
The API actions first send @TM:41,<product>, validate its reply, then send the
full @TP recipe. They do not use model-specific FA button numbers or assume
an ID-only @TP command means "brew my saved preset".

Jura Command Recipe Source/Status/Raw expose the API request's default source,
fallback reason and raw response independently of the selected recipe's state.
An API request is asynchronous; a second request is rejected while pending.
If readiness is lost or another operation starts during the settings read,
the request is cancelled. A settings-read timeout can use profile defaults only
while the machine is still ready. Brew commands are never automatically retried.

Brewing requires allow_brew: true and a fresh positive ready state without
blocking alerts. Only one control request is pending at a time. Requests made
while busy are rejected, not accumulated as future drinks. A brew request is
never automatically retried. Command Result distinguishes queued, sent, started,
completed, rejected and unconfirmed outcomes. @TB alone is not confirmation of
the requested drink; a matching TV product confirms its start.

allow_power_off: true exposes Turn Off (legacy AN:02). This release does not
implement power-on, mid-drink stop, screen navigation, starting maintenance
programmes or editing saved machine recipes. It covers monitoring and profile
drink preparation, rather than claiming every screen operation is supported.

STARTUP AND STATUS
------------------
The native entities are registered at build time. During setup the component
tries TY identification and, if needed, takes the model from the first AT T3
reply. It names/unhides the selected profile's slots before the native API
starts. Only that profile's slots appear. AT identification starts after
startup_delay; discovery stays open for a further 30 seconds (38 seconds total
with the default startup delay). A working TY response can finish discovery
earlier. This waiting is nonblocking and continues to service the watchdog.
For a machine that cannot answer during boot, set profile_hint: EF533 (or its
actual EF code); a hint can finalise discovery after 8 seconds even without a
TY reply. A later different/unknown profile requires an ESP restart to
rebuild discovery; control is blocked rather than using incompatible metadata.

startup_delay defaults to 8s before starting the AT session. handshake_response:
auto handles the two captured challenges 010001B228 and 01C000. Your EF533 uses
8120000000 automatically. Other challenges are reported in Jura Challenge and
require a captured, correctly sized handshake_response override. Having a model
in the database does not prove that its AT handshake or firmware was tested.

TF alerts may stop during a drink. Valid TV traffic keeps the AT session alive;
read polling pauses while an operation is active. After 60 seconds without TF,
Alerts Stale becomes true and individual alerts become unavailable, while TV
progress can continue. The component requests fresh TF after the operation.
Progress silence for 20 seconds makes operation state unknown; it is not
treated as a successful finish. An idle link without valid AT traffic reconnects
after a backoff. Counters retain their last known values across link loss.

COUNTERS AND PROFILE DATA
-------------------------
Product and maintenance labels come from the database; they are not guessed
from the old raw RT positions. EF533's six verified raw extras remain separate.
Do not add the duplicate raw and AT espresso counters together. Product counters
currently read the 16-bit base banks; overflow/high-word or day-specific banks
are not implemented. 0xFFFF/0xFF sentinel readings stay unavailable.

PROFILE_REFERENCE.txt lists all 75 models and their named products.
PROFILE_DATABASE.json is a readable export with options, defaults and alerts.
These exports document the included data; changing them does not change firmware.
The embedded jura_profiles_blob.h remains the single build/runtime data source.
The new flash-safe reader uses only the selected profile's metadata in RAM.
See SOURCES.txt for upstream attribution and the precise source files.

TESTING
-------
See VALIDATION.txt for what passed and the boundary between simulated tests,
real ESP32-C3 SDK syntax checks and physical machine verification. First deploy
this release to your E8 and check model, ready/alert states and counters, then
test one named espresso and one customised espresso. Your earlier logs establish
the E8 handshake/TF/TV behaviour; this fresh implementation still needs that
on-device acceptance test.

READ-ONLY SAVED ESPRESSO SETTINGS PROBE
--------------------------------------
Press Jura Read Espresso Settings while connected and idle. The diagnostic
Result, Raw and Settings text entities show the outcome of @TM:41,02. There
is no automatic polling, settings write, brew or change to the recipe controls.
EF533 saved espresso water reads were confirmed by a 45 -> 60 -> 45ml screen test. Compare
the returned values with the machine screen before trusting the interpretation.
See ESPRESSO_PROBE.txt for the exact test steps and response meanings.
