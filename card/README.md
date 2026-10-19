# Jura Graphical Card

Home Assistant dashboard card for this repository's `jura_native` ESPHome
component. Current card version: **2.1.4**. Component versions remain in the
repository-root `VERSION`; card versions are tracked separately in `card/VERSION`.

The card combines an outlined Jura machine, optional Cool Control, graphical
drink selection, saved-recipe adjustments, live stages and progress, maintenance
reminders and profile-labelled drink counters. It uses native Home Assistant
entities and services, without MQTT or external images, fonts or runtime packages.

- [Installation, configuration and troubleshooting](../docs/card.md)
- [Card changelog](CHANGELOG.md)
- [Machine-only example](examples/machine-only.yaml)
- [Cool Control example](examples/with-coolcontrol.yaml)
- [Custom recipes and hold-to-brew](../docs/card-recipes.md)
- [Complete custom recipe example](examples/custom-recipes.yaml)
- [Building, testing and releasing](CONTRIBUTING.md)

## Quick configuration

```yaml
type: custom:jura-native-card
name: Jura E8
entity_prefix: jura_e8
coolcontrol: false
```

Replace `entity_prefix` with the prefix of your actual Home Assistant entity IDs.
The card has YAML configuration; there is no graphical configuration editor.

Tapping a drink **does not brew it**. Review its settings, place a cup, and
press **Brew** explicitly. Optionally enable `hold_to_brew: true` to prepare and
brew with a 1.5-second hold. Brewing requires `allow_brew: true` in the ESPHome
component. Unsupported recipe settings are hidden. Other supported Jura profiles
can expose different drinks; only the E8/EF533 has been physically verified with
this card so far.

MIT licensed under the repository [LICENSE](../LICENSE). The machine and drink
artwork is inline SVG; Home Assistant supplies Material Design icons.
