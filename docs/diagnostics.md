# Diagnostics

For the dev-only manual shutdown-delay experiment, see
[AN:01 test](an01-test.md). Its wake/keep-awake effect is unverified.

## Saved espresso probe

**Jura Read Espresso Settings** sends `@TM:41,02` once while connected and idle.
It is a read-only diagnostic, independent of the native selected recipe loader.
It does not brew, change controls, write settings, lock the screen or retry.
It can be used with `allow_brew: false`.

Check these diagnostic text entities:

- **Jura Espresso Probe Result:** validation outcome, rejection reason or timeout.
- **Jura Espresso Probe Raw:** raw response, including payload/checksum.
- **Jura Espresso Probe Settings:** decoded values for comparison with the screen.

The button itself is manual. The normal saved-recipe loader separately reads
`@TM:41,<code>` on selection/reset, including the initial default selection.
An accepted probe clears its prior raw/decoded values. A rejected button press
retains the earlier response; it is not evidence of a second read.

The tested E8 returned:

```text
@tm:41,020006090000020000000000DB
strength=8; water=45 ml; temperature=High
```

Its saved espresso water followed a controlled screen change **45 → 60 → 45 ml**.
That confirms this field/read path on EF533; it does not independently establish
every other parameter, product or model. Compare those with the machine screen.
Leave Expert Mode and return to idle before repeating the test.

### Probe results

| Result | Meaning |
| --- | --- |
| Valid response | Correct checksum/product and fields allowed by the profile; compare with the screen |
| Unsupported | Machine returned `@tm:C1` |
| Unexpected / invalid | Error token, bad hex/checksum or another product's payload |
| Incomplete / unknown | Checksum valid, but missing or unsupported field values |
| Timeout | No matching reply within the transport timeout; not retried |
| Rejected / cancelled | Local state guard prevented sending; the reason is included |

Rejection reasons distinguish disconnected sessions, missing profiles, active
operations, pending commands, stale TF and waiting for fresh TF after an operation.
The reason and last command are also logged at DEBUG level. Do not clear those
guards blindly to force a read or brew.

Fixed probe entity names can be overridden under the existing `jura_native:`
block using `espresso_probe`, `espresso_probe_result`, `espresso_probe_raw` and
`espresso_probe_settings`, each with a standard `name:` setting.

## Startup and connection

At boot, the component tries TY identification and can identify the model from
the first AT T3 reply. Native entity names/visibility are finalised before the
API starts. Identification remains open until `startup_delay + 30s` (38 seconds
with defaults); a valid profile hint can finalise earlier after eight seconds.
This waiting is nonblocking and services the watchdog.

Check Detected Jura Model, Jura Profile, Jura Session and Jura Challenge.
`handshake_response: auto` handles the three known challenges; other challenges
need their own correctly sized captured response. If the discovered model no
longer matches the finalised native metadata, restart after resolving the hint/
identification issue. Repeated reset is not a substitute for inspecting the cause.

## TF and TV freshness

TF supplies alerts/status and may stop while TV progress is active. Valid TV
keeps the session alive. After 60 seconds without TF, alerts become stale and
individual alert states are invalidated even if TV remains live. Reads pause
during operations; fresh TF is requested afterwards.

Twenty seconds without progress makes an unfinished operation unknown. This is
not a successful finish. The controller waits for fresh status before a further
brew/read. Counters retain their last readings across link loss.

Useful diagnostics include Jura TF Raw, Jura TV Raw, Jura UART Timeouts and
Jura UART Framing Errors. A settings read that times out may increment the timeout
counter while falling back to profile defaults; inspect the accompanying status.

## Raw registers

`raw_extras: true` retains EF533 `RT:0000` raw values, Raw Counter 1–16 and Changed
Counters. Raw counter entities are disabled by default in HA. It also retains
older semantic entity names for compatibility, including Single/Double Espresso
Made, Grounds Level, Brew Movements and Brews Since Cleaning/Descaling.

**Every inherited semantic interpretation is suspect.** Observing a value change
alongside espresso production establishes correlation, not an exact meaning.
For example the old Double Espresso reading of 4 disagreed with the AT profile's
2 Espressi count of 0, while its Ristretto count was 4. Do not relabel these as
verified mappings or add raw counts to the overlapping named AT counts.

Use the model-labelled AT product/maintenance entities as primary named readings,
while still validating them. Use raw values only for controlled investigation.
Set `raw_extras: false` to disable both those extras and the raw polling.
Historical hypotheses remain in the [legacy register map](../legacy/docs/uart-register-map.md).

## Reporting a test

Include the EF model/version, the native/command Recipe Source and Status, the
relevant raw response/TF/TV frames, and the matching values on the machine screen.
Describe whether the machine was idle, brewing, heating or in Expert Mode.
No additional message or data is sent anywhere automatically.
