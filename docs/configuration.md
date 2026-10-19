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
| `raw_extras` | `true` | EF533 raw diagnostics and inherited, unverified raw labels |
| `startup_delay` | `8s` | Delay before starting the AT handshake |
| `handshake_response` | `auto` | Response selected by challenge byte length, or an explicit hex override |
| `profile_hint` | `auto` | Optional EF fallback if boot identification fails |
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
