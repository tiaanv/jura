# Configuration

## Install the component

Use `ref: main` and `components: [jura_native]` in `external_components`. The
component's location remains `components/jura_native`. During testing,
`refresh: 0s` checks the source on each compile; pin a known commit when you
want a repeatable build. If ESPHome keeps old source, use Clean Build Files.

Merge the following into your existing device configuration. Do not duplicate
existing top-level blocks. Keep Wi-Fi, API encryption, OTA and secrets.

```yaml
esp32:
  board: esp32-c3-devkitm-1
  framework:
    type: esp-idf

external_components:
  - source:
      type: git
      url: https://github.com/tiaanv/jura
      ref: main
    refresh: 0s
    components: [jura_native]

uart:
  id: uart_bus
  tx_pin: GPIO4
  rx_pin: GPIO3
  baud_rate: 9600
  rx_buffer_size: 2048

jura_native:
  id: jura_coffee
  uart_id: uart_bus
  allow_brew: true
  allow_power_off: true
  startup_delay: 8s
  handshake_response: auto
  update_interval: 60s
  raw_extras: false
```

GPIO4 TX / GPIO3 RX are the reference E8/C3 wiring, not a universal board mapping.
The Jura link requires TX/RX at 9600 baud, 8 data bits, no parity and one stop bit.
Use the [hardware guide](hardware.md) for the 5 V RX divider, direct 3.3 V TX,
shared ground and connector reference.
Remove the old `jura:` block and any raw UART drink automations: only one
component may own this UART.

Complete examples:

- [E8 from GitHub](../examples/e8.yaml), with all three native API actions.
- [E8 from a local checkout](../examples/e8-local.yaml); the source path is
  relative to the YAML and points to `../components`.
- [Merge snippet for migration](../examples/migrate-e8.yaml).

The examples keep the existing `raw_extras: true` testing option. The snippet
above disables those legacy interpretations by preference; this is optional.

## Component options

| Option | Default | Meaning |
| --- | --- | --- |
| `id` | generated | Use `jura_coffee` for the example lambdas |
| `uart_id` | ESPHome UART selection | Set explicitly when more than one bus exists |
| `allow_brew` | `false` | Exposes native recipe controls/Brew and allows API brewing |
| `allow_power_off` | `false` | Exposes Turn Off (`AN:02`) |
| `read_saved_recipes` | `true` | Read machine recipes for native controls and API brews. Set `false` to use profile defaults plus explicit overrides without automatic `@TM:41` reads; the manual espresso probe remains available. |
| `raw_extras` | `true` | EF533 raw diagnostics and inherited, unverified raw labels |
| `startup_delay` | `8s` | Delay before starting the AT handshake |
| `handshake_response` | `auto` | Response selected by challenge byte length, or an explicit hex override |
| `profile_hint` | `auto` | Optional EF fallback if boot identification fails |
| `accept_short_status` | `false` | Accept the five-byte EF533 TF format with its sixth byte absent; shorter frames and other profiles retain normal completeness checks. See [diagnostics](diagnostics.md#tf-and-tv-freshness) |
| `update_interval` | `60s` | Counter/maintenance refresh requests; not the live progress rate |

`handshake_response: auto` selects by decoded challenge byte length, regardless
of its contents (hex is validated first before any reply is sent):

| Challenge length | Automatic response |
| --- | --- |
| 1 byte / 2 hex characters | `81` |
| 3 bytes / 6 hex characters | `818000` |
| 5 bytes / 10 hex characters | `8120000000` |
| Other lengths | No automatic response; configure a manual override |

This retains the observed EF533 (`010001B228`), EF1092M V01.07 (`01C041B628`),
EF536 (`01C000`) and EF533M V01.18 (`01`) combinations while allowing unfamiliar
challenges of the same lengths. It is a length-based compatibility policy, not a discovered
cryptographic calculation or proof of support for every model.

An explicit hex `handshake_response`, such as `"8120000000"`, takes precedence
over auto and may support other challenge lengths. It must contain valid hex
and have the same byte length as the challenge. Malformed challenges, odd-length
hex and mismatched responses are rejected. Jura Challenge reports the received
value. Model support in the database does not establish handshake compatibility
or full feature support on every firmware.

A hint such as `profile_hint: EF533` can finalise entity discovery if the machine
cannot identify itself at boot. Use the actual EF code, not simply `E8`.
A later incompatible model requires restarting the ESP to rebuild discovery.

## Native controls

| Entity | Purpose |
| --- | --- |
| Jura Drink | Select a named product and read its saved recipe |
| Jura Coffee Strength | Displayed profile label; not grams or a raw byte |
| Jura Temperature | Displayed level such as Normal/High; not an exact °C target |
| Jura Grinder Ratio | Only exposed where the model profile supports it |
| Jura Water Amount / Bypass Amount | Millilitres of that recipe parameter |
| Jura Milk Amount / Milk Foam Amount / Milk Pause | Seconds, where supported |
| Jura Brew | Send the loaded recipe plus HA overrides once |
| Jura Reset Recipe | Discard overrides and reload the saved recipe |
| Jura Refresh | Request counters and maintenance readings |
| Turn Off | Power-off command when enabled; no power-on action is implemented |

Unsupported settings show unavailable or `Not available`. Native slider ranges
cover the profile-wide union of drinks; each submitted value is checked against
the selected product's actual range and step. The per-drink Min/Max/Step and
Choices entities let a custom card narrow the controls appropriately.

Fixed fields can be renamed using their configuration keys, for example:

```yaml
jura_native:
  id: jura_coffee
  uart_id: uart_bus
  status:
    name: "Machine Status"
```

Merge this into the same `jura_native:` block. Profile drink counters and alerts
are generated automatically; they are not individually declared in YAML.

## Home Assistant API

The native controls require the usual `api:` connection. The action definitions
are in [the E8 example](../examples/e8.yaml). Merge `api.actions` into your
existing encrypted API block to add `brew_drink`, `brew_custom` and optionally
`brew_selected`. See [Home Assistant](home-assistant.md) for request payloads.

All brewing paths require a fresh ready state without blocking alerts. Requests
made while busy are rejected rather than queued as future drinks. A brew is
never automatically retried. No saved-recipe/EEPROM write is implemented.

## Cool Control

The optional receiver uses its own UART or ESP device. It does not read milk
level or temperature from the Jura machine's UART.

- [GitHub-source receiver example](../examples/coolcontrol.yaml)
- [Local-checkout receiver example](../examples/coolcontrol-local.yaml)

It retains the earlier observed ASCII offsets, publishes milk level/temperature
and invalidates stale readings. Its new implementation has host-test coverage,
not fresh physical verification. Keep an existing working receiver if desired.

## Compatibility

The reference configuration is ESPHome 2026.9.1 on ESP32-C3 / ESP-IDF. C3 Arduino
and ESP8266 configurations validate, and the ESP8266 flash-reader path has host
coverage. Full Arduino/ESP8266 builds and final memory usage are not established.
See [testing](testing.md) for the precise validation boundary.


## Cool Control status and older coolers

The receiver accepts bare `@mo`, `@mn`, `@me`, `@me1` lines and extended
`@mo:XXYY` / `@mn:XXYY` lines, terminated by CRLF. `XX` is interpreted as
milk level (0-100%), and `YY` as temperature in tenths of a degree Celsius;
`FF` marks an unknown measurement. These retain the earlier observed field order
and scale. The bridge forwards these bytes unchanged when extended reporting is enabled.

Optional `status` (text sensor) exposes `mo`, `mn`, `me`, `me1`, or `unknown`.
These are protocol labels: we have not established the physical distinction
between `mn` and `mo`, so no empty/full meaning is invented.
Optional `online` (binary sensor) means recognized serial messages are arriving.
A received `me1` therefore leaves serial online true while clearing readings;
it is distinct from our own communication timeout.

`communication_timeout` and `measurement_timeout` independently default to `10s`.
Bare messages keep communication fresh without refreshing old measurements.
`me` and `me1` invalidate both measurements immediately; communication expiry
also clears published measurements and reports status `unknown`. Older coolers
that only send bare messages have usable status/online entities with unavailable
measurement sensors. The default publication interval remains `2s`; accepted
frames also publish immediately.

Reception remains passive unless `startup_probe` is configured. Some older
coolers reportedly begin reporting only after receiving text. This is optional
compatibility behavior, not a verified Jura wake-up command. A blank CRLF is the
default probe; if it does not work, configure the text known to work with your
cooler. Text is sent exactly as configured, without automatic line endings.
Enabling it requires an actual UART TX pin as well as RX, at 9600 baud, 8N1.

```yaml
jura_coolcontrol:
  uart_id: cool_uart
  status:
    name: "Cooler Status"
  online:
    name: "Cooler Online"
  communication_timeout: 10s
  measurement_timeout: 10s
  startup_probe:
    text: "\r\n"
    delay: 1s
    retry_interval: 2s
    max_attempts: 3
```

Probing defaults to one attempt, permits at most five, and stops permanently for
that boot after any recognized status, including bare older-cooler messages.
It does not restart automatically when a previously reporting cooler goes silent.
See the Cool Control examples for the existing RX wiring; choose TX for your
hardware and avoid sharing that UART with serial logging.


### Raw messages for a future machine bridge

Enable optional `raw_message` (text sensor) and `message_counter` (numeric sensor):

```yaml
jura_coolcontrol:
  uart_id: cool_uart
  raw_message:
    name: "Cooler Raw Message"
  message_counter:
    name: "Cooler Message Counter"
  online:
    name: "Cooler Online"
```

`raw_message` preserves each accepted message exactly, including hexadecimal
letter case, with only the trailing CRLF removed; for example `@mo:321E`, `@mn`,
`@me`, or `@me1`. Only frames accepted by the existing status/telemetry validator
are published. Malformed input and rejected out-of-range levels are not counted.
The raw payload is published before the counter for that frame.

`message_counter` starts at zero on boot and advances once for every accepted
frame, including identical repeats and older-cooler bare statuses. It wraps from
16,777,215 to zero so every increment remains exactly representable by ESPHome's
floating-point sensor. Treat it as a changing sequence marker, not a lifetime
total; detect any change rather than requiring a higher value. Avoid filters
that throttle, average or round this sensor if using it as a heartbeat.

Polling and timeout expiry do not advance the counter or manufacture raw
messages. The raw sensor retains the last received payload when offline; consumers
must check `online` and use their own receive timeout before forwarding it.
At boot there is no raw message until a valid frame arrives. These entities feed
the optional Home Assistant bridge described below.
Home Assistant/API delivery can batch updates, so this is a latest-status and
liveness interface rather than a guaranteed lossless queue of serial messages.


### Direct device-to-device bridge

To avoid HA counter traffic and remove HA from the forwarding path, use the
[direct UDP bridge](direct-coolcontrol.md). It uses an internal packet sensor,
periodic heartbeats and local freshness checks, without MQTT. This is an optional
alternative to the existing HA bridge; both devices need components 1.1.0 or later.

### Forwarding Cool Control status to the coffee machine

The machine component's optional `coolcontrol` block references ESPHome entity
IDs, including entities imported from Home Assistant. Without this block, the
existing T2 replies, machine polling, controls and UART behavior are unchanged.
There is no dependency on a cooler or Home Assistant for ordinary operation.
See [the complete Home Assistant bridge example](../examples/e8-coolcontrol-ha.yaml).

```yaml
jura_native:
  id: jura_coffee
  uart_id: uart_bus
  coolcontrol:
    raw_message: cooler_raw
    message_counter: cooler_counter
    online: cooler_online
    reporting: auto
    timeout: 15s
    coalesce_delay: 250ms
```

The example imports your separate cooler device's raw message, counter and online
entities with `platform: homeassistant`; replace their `entity_id` strings with
those on your installation. Both devices must be integrated into Home Assistant.
The machine's existing native API connection receives those states; no HA
forwarding automation or API action is required. Keep the imported entities
internal and do not add averaging/throttling filters to the counter.

The bridge accepts only validated cooler statuses. A changing counter supplies
liveness, including identical status repeats and counter resets/wraps. A brief
coalescing delay allows raw-message and counter updates to arrive separately;
this is latest-state forwarding, not guaranteed delivery of every serial frame.
The settling window is bounded to twice `coalesce_delay`, so frequent updates
cannot postpone forwarding indefinitely.
Unchanged cached Home Assistant states do not keep the bridge alive. The local
`timeout` protects against a failed network or Home Assistant as well as a silent
cooler. A false/unavailable online state or invalid input withdraws cooler status.
Raw and counter import states must both be available. Entity updates do not
interrupt an active UART transaction and at most one cooler status is sent per
second; control commands retain priority and repeated pending states coalesce.

With `handshake_response: auto`, bridge-enabled sessions use the protocol's
T2 response rule for supported challenge lengths: first byte OR 0x80; second byte
preserves challenge bit7 and sets cooler capability 0x20; remaining bytes zero.
For example, a three-byte challenge starting `0180` receives `81A000`.
Explicit handshake overrides remain authoritative; users supplying one must
include the appropriate cooler capability themselves.

Reporting policy is controlled by `reporting`:

- `auto` (default): EF533 uses a legacy bare-status fallback when no `@HM`
  arrives. Other profiles wait for negotiation.
- `negotiated`: require `@HM` on every model, including EF533.
- `legacy`: allow bare status forwarding without `@HM` on any model. Use this
  only as an explicit compatibility choice for a machine you are testing.

An actual plaintext or encrypted `@HM:XX` always takes precedence, including an explicit
disable: bit7 enables reports and bit6 permits extended telemetry. A new session
clears that negotiation and reapplies the selected fallback policy. No `@HM`
command is sent to the machine to manufacture its settings.

Bare `@HM` requests one cached status reply without changing the reporting
settings. `@HM:00` requests one reply and disables automatic reporting. Other
valid `@HM:XX` settings also request a reply. Requests use the same coalescing
and one-message-per-second limit as notifications; repeated requests can receive
the same status again, including `@me1` when the source is unavailable or stale.

When fresh cooler data is available, the bridge sends plaintext `@H1` once per
machine session, before forwarding status. A cooler returning after an offline
or stale period is announced again. This announcement does not depend on HM
reporting permission, and missing or stale input does not trigger it.

The EF533 fallback is a compatibility policy for sending bare status before
negotiation. Hardware testing on EF533M V02.09 confirmed that announcing `@H1`
elicits `@HM:80`, and forwarded `@mn`/`@mo` sets and clears the machine's
no-milk alert. Other models still require testing.

### Manual cooler override

When `coolcontrol` is configured, the component also creates a **Jura Cool Control
Override** switch. No extra YAML is required. Turn it on to announce cooler
availability and send healthy bare `@mo`, even if the source is missing, stale,
offline or reporting empty. It also bypasses the milk-specific brew checks;
tray, water, connection and other machine checks still apply. The override does
not invent temperature or level measurements, and the real imported data stays
unchanged. The machine will not receive empty/offline cooler protection while
this switch is on, so confirm milk is available before making a milk drink.

Turning it off sends the actual latest status, or `@me1` if no fresh source is
available. Source freshness continues to be monitored while the override is on.
The switch restores its saved ON/OFF state after a device reboot. On first boot,
with no saved state, it defaults OFF. It also remains enabled across an ordinary
machine-session reconnect until switched off. Persistence uses ESPHome switch
preferences and its configured flash-write interval. DEBUG logs show `Cool Control override ON` / `OFF`,
and bridge diagnostics identify the forced healthy state.

The optional `coolcontrol.override_switch` entity configuration can customize
its name or ID. Its default `restore_mode` is `RESTORE_DEFAULT_OFF`;
`ALWAYS_OFF` is also available if you explicitly prefer resetting it at boot.

The bridge preserves mo/mn/me/me1 without assigning unverified physical meanings.
Fallback mode strips telemetry and sends only bare statuses. An HM enabling
extended reporting allows unchanged telemetry, except a first telemetry byte of
FF produces a bare status. Expired/offline input generates `@me1` once, including
when reporting begins without fresh data. Restoring a fresh counter resumes
reporting. A new machine session can replay the latest still-fresh status.

Unchanged notifications are suppressed after formatting for the negotiated mode.
Counter heartbeats still establish freshness, but do not resend identical status.
In bare mode, changes only to temperature or level do not resend `@mo`/`@mn`;
in extended mode, changed telemetry is forwarded. An explicit HM request always
receives a cached reply even if unchanged, subject to the existing pacing limit.

Before brewing a product with milk or foam options, the native controller checks
the profile's no-milk, milk-sensor error and no-signal alerts. These alerts reject
milk products without blocking coffee-only products. The checks run at request
time, during the saved-settings read and before the brew command is sent. They
use machine TF status and also work without the Cool Control bridge configured.

Progress states `2D`, `2E` and `2F` report blocked milk preparation: no milk,
milk cooler error and no milk cooler signal respectively. Brew activity clears,
progress remains unknown, and a fresh TF status is requested. These states do
not count as a completed drink and do not trigger a false progress timeout.

Messages use plaintext CRLF framing and the paced four-symbol Jura UART transport with no
reply expected. This feature has protocol simulation/configuration coverage;
physical machine interoperability and the appropriate input timeout for each
cooler's reporting interval still need hardware testing. DEBUG logs now show bridge configuration, the imported raw/counter/online values,
missing inputs, selected reporting policy and stale-counter withdrawal. Counter-only
input logs are limited to once per five seconds; raw/online/availability changes
are logged immediately. Unchanged inputs, including missing or offline states,
are repeated every 15 seconds together with the bridge reporting/freshness state,
so attaching the log client after boot does not hide the diagnostic. Look for, for example:

```text
Cool Control bridge enabled: reporting=auto, timeout=15000 ms, coalesce=250 ms
Cool Control HA inputs: raw='@mo:321E', counter=123.000000, online=ON; received valid inputs
Cool Control bridge: legacy bare fallback (no @HM required); fresh cooler snapshot
Cool Control announcement: @H1 (plaintext; fresh cooler available in this machine session)
Sending @H1
Sending @mo
Cool Control status request: @HM; cached response queued
Cool Control request response: @mo (plaintext; fresh cooler snapshot)
Sending @mo
```

A `<missing>` input means its Home Assistant state has not arrived, not that
cooler telemetry itself lacks the optional two bytes. `Sending` confirms UART
submission, not machine acknowledgement. Disabling the YAML block restores the
original component behavior.
