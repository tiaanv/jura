# Custom recipes and hold-to-brew

These features require card **2.1.2** or later. The card's [installation guide](card.md)
explains HACS/manual installation, entity prefixes and optional Cool Control.

## Custom recipes

Define personal recipes in the card's YAML. **My Recipes** appears above the
standard drinks only when configured. Tiles use the base drink's cup artwork.

```yaml
type: custom:jura-native-card
name: Jura E8
entity_prefix: jura_e8
coolcontrol: false
recipes:
  - name: Milky White
    base_drink: "1 Flat White"
    overrides:
      milk_seconds: 30
  - name: Kids White
    base_drink: "1 Flat White"
    overrides:
      water_ml: 60
      milk_seconds: 22
```

`name` is the tile and Brew button label. `base_drink` must exactly match an
option from the component's drink select. On the reference E8 that is
`1 Flat White`, not the shortened display label `Flat White`.

Every override is optional. The names match the component API action:

| Override | Value |
| --- | --- |
| `strength` | Supported choice, such as `"8"` or `8`. |
| `water_ml` | Integer water amount in millilitres. |
| `milk_seconds` | Integer milk duration in seconds. |
| `foam_seconds` | Integer foam duration in seconds. |
| `temperature` | Exact supported choice, such as `High`. |
| `bypass_ml` | Integer bypass water amount in millilitres. |
| `pause_seconds` | Integer pause duration in seconds. |

Values must match the base drink's supported choices, range and step. Zero is
accepted only when that profile allows it. Unknown override names and malformed
configuration are rejected. Not every drink supports every parameter.

Selecting a custom tile always reloads the **machine-saved base recipe**, even
when the same base is selected. Unspecified settings therefore do not inherit
overrides from the previous custom recipe. All overrides are validated before
any are written, then applied sequentially with confirmation. A profile-default
fallback cannot silently enable a custom recipe: a saved baseline is required.

Unsupported settings, failed loads and unconfirmed writes keep Brew disabled.
Select a recipe successfully again or reset before brewing. Further manual
adjustments are allowed; **Reset to saved recipe** clears the custom selection
and restores the base. Custom recipes never overwrite permanent machine recipes.

## Hold to brew

This shortcut is **off by default**. Add this at the card's top level, alongside
`entity_prefix` and `recipes`:

```yaml
hold_to_brew: true
```

The tile section then displays **Tap or hold to brew**:

- **Tap:** select the standard drink or custom recipe; review settings and use
  the separate Brew button as before.
- **Hold for 1.5 seconds:** a bar fills along the tile. When it completes, the
  card loads the saved base, applies custom overrides where applicable, waits
  for confirmation, then requests one brew automatically.
- Releasing early keeps ordinary tap behaviour and never auto-brews. Leaving
  the tile, scrolling, window blur or touch cancellation cancels an unfinished hold.

After the bar completes, the hold is committed: releasing then does not cancel
preparation. Place a suitable cup before holding. A standard-drink hold reloads
its saved recipe; use the separate Brew button to brew manual adjustments instead.
Keyboard activation remains normal selection.

Machine readiness, fresh alerts, confirmed settings and duplicate-command checks
still apply. Invalid recipes, disconnection, loss of readiness or uncertain
previous commands prevent automatic brewing. There is no automatic brew retry.

![Live personal card with custom recipes and hold-to-brew enabled](assets/card/custom-recipes-hold.jpg)

This real E8/EF533 capture is from 19 October 2026. The owner confirmed the
hold-to-brew flow on the machine. Automated simulated-service tests cover standard
and custom holds, short taps, cancellation, invalid overrides, interrupted loads
and duplicate requests. Other profiles and touchscreen/browser combinations
still need community validation. Example settings and counters are machine-specific.

See the [complete YAML example](../card/examples/custom-recipes.yaml).
