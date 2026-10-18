# Jura Native for ESPHome — V2

Monitor and prepare Jura drinks through Home Assistant using ESPHome's native
API. MQTT is not required. The `V2` branch contains the `jura_native` component;
its current component version is recorded in [VERSION](VERSION).

## Features

- **Model identification:** automatic EF detection with 75 bundled model profiles.
- **Live monitoring:** machine status, model-specific TF alerts, current drink,
  TV stage descriptions, overall progress and available stage quantities.
- **Counters:** profile-labelled AT product counts and maintenance readings.
- **Saved recipes:** selecting a drink reads its saved settings into native HA
  controls, with an explicit profile-default fallback when a read fails.
- **One-shot adjustments:** strength, temperature, water, milk duration and other
  supported settings apply to the next accepted Brew request; the machine's
  saved recipe is never overwritten.
- **Native API actions:** `brew_drink`, `brew_custom` and `brew_selected`.
  Named/custom actions use freshly read saved settings as their defaults.
- **Nonblocking UART:** serialised transactions, bounded reception, readiness
  checks, timeout recovery and no automatic brew retry.
- **Optional Cool Control receiver:** a separate `jura_coolcontrol` component
  for the existing observed milk-level/temperature frame format.

The reference machine is an **E8 / EF533M V02.09 on ESP32-C3 with ESP-IDF**.
A profile being present does not mean that its firmware, handshake or every
feature has been tested. See [validation status](docs/testing.md).
Legacy raw-register interpretations are **unverified**; use the profile-based
AT counters as the primary named readings and see [diagnostics](docs/diagnostics.md).

## Get started

Merge this source into your existing device YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/tiaanv/jura
      ref: V2
    refresh: 0s
    components: [jura_native]
```

Follow the [configuration guide](docs/configuration.md), or adapt the complete
[E8 example](examples/e8.yaml). Retain your device name, Wi-Fi, native API
encryption, OTA configuration and secrets. The examples contain placeholders.
Only one component may consume the Jura UART.

1. Compile/upload and wait for **Jura Connected** and sensible ready/alert states.
2. Select **Jura Drink** and wait for **Jura Recipe Loading** to turn off.
3. Check **Jura Recipe Source** (`Machine` or `Profile defaults`).
4. Adjust supported settings and press **Jura Brew**.

**Reset Recipe** discards HA edits and reloads the selected drink. Edits are
consumed when a Brew request is accepted into the queue; rejected requests
retain them. See [saved recipes](docs/saved-recipes.md) for the exact behaviour.

## Home Assistant actions

The [complete E8 YAML](examples/e8.yaml) defines the actions. Existing devices
that already define them need only the updated component implementation.

Saved Flat White on a device named `jura-e8`:

```yaml
action: esphome.jura_e8_brew_drink
data:
  drink: "1 Flat White"
```

For a 60 ml water override while keeping the other saved settings, use the
[custom Flat White example](docs/home-assistant.md#custom-flat-white).
[Action examples](examples/ha-actions.yaml) must be run individually; they are
not an ESPHome device configuration.

## Guides

| Guide | Contents |
| --- | --- |
| [Configuration](docs/configuration.md) | Installation, wiring example, options, native controls, Cool Control |
| [Saved recipes](docs/saved-recipes.md) | Loading, fallback, one-shot edits, API preparation and cancellation |
| [Home Assistant](docs/home-assistant.md) | Action YAML, native entity contract and card integration |
| [Diagnostics](docs/diagnostics.md) | Espresso probe, startup troubleshooting, stale state, raw registers |
| [Migration](docs/migration.md) | Replacing the old `jura` component and updating an existing V2 device |
| [Profiles](docs/profiles/README.md) | Model index and readable export of the full bundled database |
| [Development and testing](docs/testing.md) | Host tests, schema checks, compiler checks and hardware evidence |
| [Architecture](docs/architecture.md) | Transport, controller, profiles and native entities |
| [Sources and licensing](docs/sources.md) | Upstream attribution, protocol references and data limitations |

## Repository layout

```text
components/   Installable jura_native and jura_coolcontrol components
examples/     Current ESPHome configurations and HA action examples
docs/         Current guides and readable profile exports
tests/        Portable regression tests and ESPHome schema validation
tools/        Profile export, documentation checks and source packaging
legacy/       Archived old component, examples and register documentation
```

The old `components/jura` has moved to [legacy/components/jura](legacy/components/jura).
Its documentation and examples are historical references, not V2 instructions.
The install paths and entity names for **jura_native** and **jura_coolcontrol**
are unchanged. See the [legacy archive](legacy/README.md) and [change log](CHANGELOG.md).

## Credits and licence

The model database, AT cipher reference and progress glossary build on
[harryzz/jura-uart-esphome](https://github.com/harryzz/jura-uart-esphome).
Protocol work also draws on the sources listed in [sources](docs/sources.md).
The historical component credits are preserved in the legacy archive.

MIT licensed; see [LICENSE](LICENSE) and the retained
[harryzz notice](components/jura_native/LICENSE.harryzz).
