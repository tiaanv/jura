# Components 1.2.0

Released on 2026-11-13 as the immutable Git tag `components-v1.2.0`. This shared
component version introduces saved-recipe caching and fixes delayed Cool Control
responses in `jura_native`. The release index and notes are stored in the repository.

## Saved-recipe cache

Validated machine recipes are cached per drink in RAM and reused by selection,
Reset Recipe, named brewing and custom brewing. A cache miss reads the machine.
A normal API brew can share an in-flight selection read for the same drink.
Profile defaults and one-shot overrides remain separate from cached values.

The cache clears on component restart or a change in reported machine identity.
Edits made on the machine screen require an explicit refresh. The example
`refresh_recipe` action refreshes a named drink without brewing. The example
`brew_drink_with_refresh` action accepts `refresh_recipe: true` to require a fresh
read before brewing. A failed required refresh cancels that brew and retains
the last successful cache entry. Custom YAML actions can also pass a final
boolean to `brew_custom(...)`. Existing action fields continue to work.

See [saved recipes and refresh actions](../saved-recipes.md). The first read still
has UART latency. Subsequent cache hits avoid that read, while Home Assistant
confirmation, configured hold duration and machine preparation still take time.

## Cooler response priority

An explicit `@HM`, `@HM:00` or other valid `@HM:XX` request can interrupt an
outstanding reply wait after transmitted bytes finish and the receive quiet
interval passes. The cached cooler reply takes priority over queued work.
An unanswered status query therefore no longer holds a milk reply behind its
five-second timeout.

Interrupted settings-based API brewing is cancelled. A brew already transmitted
keeps its machine start-confirmation deadline and is never resent. Interruptions
are logged separately from UART timeouts:

```text
Cool Control priority: interrupted reply wait for @TF:
Cool Control request response: @mo ...
```

Freshness rules and the healthy-state override still apply. See
[Cool Control configuration](../configuration.md).

## Upgrade and compatibility

Release pinning is optional. Use `ref: components-v1.2.0` for this snapshot or
continue following `main`. Refresh external component sources as needed,
rebuild and upload the Jura ESP. Existing configuration needs no migration to
use caching or the cooler reply fix. Add the optional API actions from the
[E8 example](../../examples/e8.yaml) to expose explicit refresh controls.

The Cool Control sender has no source changes since 1.1.0, so an existing
1.1.0 direct UDP sender can work with the updated receiver. Matching tags remain
a convenient choice for a new paired installation. The direct bridge retains
its ESPHome 2026.9.1 minimum.

Card 2.1.12 additionally reuses confirmed selected settings during hold-to-brew,
including one-shot edits. Older cards benefit from the component cache but may
still reset those edits during hold. Update the card through its separate release.

## Validation

Host tests cover the controller, native adapter, direct and HA cooler bridges,
cooler receiver, legacy component, ESP8266 flash-reader path and all 75 profile
catalogues. Cache regressions cover reuse, refresh failure, shared reads,
overrides and cancellation. Priority regressions replay an unanswered TF query
followed by HM and TB, and cover late replies, RX/transmission boundaries and
prevention of duplicate brews.

The card fixture passed 50 browser checks. ESPHome 2026.9.1 schema validation
and C++ generation passed during cache implementation. These checks include
generated example API actions. A complete device firmware build and hardware
verification of the cooler priority fix remain outstanding.

The observed hardware failure that motivated the fix had a 4.84-second delay
between the cooler request and cached reply dispatch. The new host regression
expects a complete bare reply within 280 ms under its simulated timing. Actual
hardware timing and perceived recipe responsiveness still require confirmation.

Read the [disclaimer](../disclaimer.md) and [contributor notices](../notices.md).
