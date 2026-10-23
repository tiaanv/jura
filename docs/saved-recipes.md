# Saved recipes and one-shot HA edits

Update from V2, compile and upload. No YAML changes are required for the native
selectors, numbers, buttons and new recipe-state entities. Keep allow_brew: true
to use Brew. The optional brew_selected API action is included in the [GitHub](../examples/e8.yaml) and [local](../examples/e8-local.yaml) E8 examples;
merge it into your `api.actions` only if your card wants that action rather than
pressing the native Jura Brew button.

## Firmware without saved-settings replies

For firmware such as EF533M V01.18 where automatic `@TM:41` reads time out,
merge this setting into your existing component configuration:

```yaml
jura_native:
  read_saved_recipes: false
```

The default is `true`, retaining existing behaviour. When disabled, startup,
selection, Reset Recipe, `brew_drink` and `brew_custom` skip the saved-settings
request. Controls and API brews use profile defaults plus explicit one-shot
overrides. Recipe Loading stays off; Recipe Status and Command Recipe Status
identify the disabled reads rather than suggesting a retry. Automatic reads
therefore no longer generate settings-read UART timeouts. Other UART requests
can still time out.

Reset Recipe discards UI edits and restores profile defaults without reading.
The manually pressed Read Espresso Settings diagnostic button remains available;
it can still time out and does not initialise the controls. No recipe settings
are written back to the machine. Machine-customised recipes are not available
when reads are disabled. Readiness checks and duplicate/busy guards still apply.

## Normal use
Select a drink in Jura Drink. The component schedules `@TM:41,<product code>`,
waits for an idle connected machine/fresh status, and reads its saved settings.
Jura Recipe Loading stays on until that read succeeds or fails. While waiting,
the controls show provisional profile defaults; Brew rejects rather than silently
using them. Changing another control while loading is allowed and that explicit
HA edit is retained when the machine values arrive.

Once Recipe Loading is off, inspect:

| Entity | Meaning |
| --- | --- |
| Jura Recipe Source | Machine or Profile defaults (the baseline source) |
| Jura Recipe Status | Loaded product, pending read, or fallback explanation |
| Jura Recipe Modified | HA overrides are pending for one Brew request |
| Jura Saved Recipe Raw | Current-selection raw response for diagnostics |

Edit settings and press Jura Brew. The queued @TP command contains a frozen copy
of the loaded recipe plus overrides. The controls immediately return to the
baseline and Recipe Modified turns off. Rejected requests preserve the edits.
Edits are consumed when queued, including if the eventual command is rejected
or unconfirmed; a brew is never retried automatically. Changing controls while
a prior brew is pending cannot alter that prior command's frozen payload.

Reset Recipe discards overrides and requests a fresh read. Selecting even the
same drink also reloads it. The baseline stays in RAM until a selection/reset;
it is not a continuous mirror of changes made at the machine screen. After a
screen edit, leave Expert Mode, return to idle, then select/reset in HA to reload.

## Read validation and fallback
The reply must have a valid ASCII checksum, echo the requested product, contain
all options declared for that product, and decode to permitted profile values.
Unknown/sentinel/out-of-range values cause a complete profile-default fallback;
partial machine settings are not mixed silently into the baseline.

Unsupported (@tm:C1), malformed, unexpected, bad-checksum, wrong-product,
incomplete and timed-out responses use Profile defaults and explain why in
Recipe Status. There is no automatic retry loop. Select/reset to retry.
Existing user edits are retained over the fallback baseline for one brew.

A response for an earlier selection/reset is discarded. Selection requests
coalesce to the newest drink rather than accumulating a read per click. The
in-flight transaction finishes or times out, then the newest selection is read.
Reads defer during a machine operation and do not change its saved recipes.
The initial default selection (Espresso where available) loads after connection.

## API and card usage

The component also exposes **Jura Recipe Capabilities**, a versioned catalogue of
all selectable drinks and their supported settings for the detected profile.
This comes from the profile database and needs no additional machine requests.
The card uses it to validate custom recipes when the dashboard loads; machine-saved
values are still read only when a drink is selected. See the
[capability transport and fallback behaviour](card-recipes.md#capability-transport-component-and-card-developers).
Use button.press on the native Jura Brew entity, or brew_selected(), to use the
selected controls. The example's optional API action is normally exposed as
esphome.jura_e8_brew_selected for the existing device name jura-e8.

brew_drink(name)/brew_named(name) now read the requested product's current
saved settings and brew them. brew_custom(...) reads the same saved baseline
and then overlays the explicit arguments. Numeric -1 and empty/"default" choice
strings use the saved values. If the read is unsupported, invalid or times out,
the full baseline falls back to profile defaults; explicit overrides still apply.

Each API call snapshots its requested product and overrides. Changing the native
Drink selection or editing its controls cannot change the pending API request.
The API actions do not use or consume native UI edits. Use brew_selected for
those. They read the requested product afresh, even if a different one is selected.
Existing API names, fields and YAML lambdas do not need changing.

Jura Command Recipe Source reports Pending, Machine, Profile defaults or Not used
(for cancellation). Jura Command Recipe Status explains the read or fallback and
Jura Command Recipe Raw retains its reply. These describe the API request, while
Jura Recipe Source/Status continue to describe the native selected controls.

Requests must be made while the machine is ready; only one can be pending. The
read completes before a single @TP command is queued. Readiness is checked again
before brewing; a start/progress notification, not-ready TF status or disconnect
during the read cancels preparation. A later valid reply cannot revive it. No
future drink is queued behind a machine operation, and no brew is auto-retried.
An API timeout/fallback adds the settings-read timeout to the brew latency.
Watch Command Result/progress for the eventual outcome; the HA action returning
only means it submitted a request, not that the physical drink has finished.

These APIs use the full AT recipe format. Legacy FA commands simulate some known
model-specific drink buttons, with different numbers from AT product IDs. No
universal ID-only AT preset-start command is assumed here. No settings write,
EEPROM write, @TS lock/unlock or screen-navigation command has been added.

## First tests on your E8
1. Select Espresso; expect Machine source and your saved 8 / 45ml / High values.
2. Select another drink and compare every available control with its screen.
   Espresso's saved-water read is physically established; other products and
   field mappings still need these comparisons.
3. Edit water in HA and press Brew once. Verify the edited command/drink, then
   confirm the controls returned to the saved baseline and the screen recipe
   remains unchanged. Compare progress/command result as in previous tests.
4. Change one setting at the screen, exit menus, press Reset Recipe in HA, and
   verify the refreshed controls. Test a milk drink's duration separately.
5. If Source says Profile defaults, inspect Recipe Status and Saved Recipe Raw;
   the firmware may not expose saved settings for that product.
