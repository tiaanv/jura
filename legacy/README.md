# Legacy Jura component

The legacy component remains installable at [`components/jura`](../components/jura)
on `main`, alongside `jura_native`. It uses **non-blocking UART and a command
queue**, with framework-independent C++ for ESP32/ESP-IDF, ESP32/Arduino and
ESP8266/Arduino. The original `jura:` schema and sensor keys are retained.

Start with the [complete ESP32-C3 / ESP-IDF E8 example](../examples/legacy-e8.yaml).
Keep your Wi-Fi, API encryption, OTA secrets, model and actual UART pins.
The [hardware guide](../docs/hardware.md) covers the RX divider and shared ground.
Only one component may consume the machine's UART.

## Source and board configuration

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
    components: [jura]

logger:
  baud_rate: 0

uart:
  id: uart_bus
  tx_pin: GPIO4
  rx_pin: GPIO3
  baud_rate: 9600
  rx_buffer_size: 1024

jura:
  id: jura_coffee
  uart_id: uart_bus
  model: E8
  update_interval: 2s
```

For ESP32 Arduino change `framework.type` to `arduino`. For ESP8266 replace the
`esp32:` block with `esp8266: {board: d1_mini}` and use your actual UART pins;
the model examples below use D1 TX / D2 RX. Preserve the electrical interface:
ESP GPIO inputs must not receive 5 V directly.

## Queued control buttons

Replace synchronous `cmd2jura()` calls and raw `uart.write` sequences with
`queue_command()`. There is no synchronous response return value.

```yaml
button:
  - platform: template
    name: "Make Coffee"
    on_press:
      - lambda: |-
          if (!id(jura_coffee).queue_command("FA:09"))
            ESP_LOGW("jura_button", "Command was not queued");
```

`true` means accepted into the queue, not that the machine made a drink. There
are eight waiting slots plus one active transaction. Commands run FIFO before
the next poll, never interrupt an active transaction and are not retried.
Repeated presses enqueue repeated requests. Responses are logged.

Polling requests coalesce. The default update interval is 2 seconds; RT counter
and IC status reads are scheduled independently. Each loop reads at most 128 raw
bytes and transmits at most one encoded character, with at least 8 ms between
character starts. Transmission scheduling and response waiting each have a
5-second deadline. Recovery includes a quiet gap and remains non-blocking.
Leave UART byte debugging disabled when validating timing; use the `jura`
Sending/response/timeout logs.

FA numbers are model-specific legacy commands, not native AT drink IDs. Verify
a mapping on your machine before using it. `AN:01` in the model examples is a
historical command, not a guarantee that an off machine can be powered on.

## Examples and references

- [E8 / ESP32-C3 / ESP-IDF](../examples/legacy-e8.yaml)
- [E6/E8 / ESP8266 Arduino](examples/e6_e8.yaml)
- [F6 / ESP8266 Arduino](examples/f6.yaml)
- [F7 / ESP8266 Arduino](examples/f7.yaml)
- [Original README, with current button syntax](docs/README-original.md)
- [Command notes](docs/commands.md)
- [Register interpretations](docs/uart-register-map.md)

The model examples now use queued commands. Their command mappings and
raw-register meanings remain hypotheses, including the old semantic sensor
labels. Preserving them does not validate those interpretations for every model.
This legacy implementation polls RT/IC; use `jura_native` for the full profile
database, TF alerts, TV progress and saved-recipe controls.

See [validation status](../docs/testing.md) for the distinction between host tests,
configuration validation, compiler checks and physical observations.
For optional migration, use the [migration guide](../docs/migration.md).
The historical Cool Control example refers to an older receiver configuration;
use the current [receiver example](../examples/coolcontrol.yaml).
