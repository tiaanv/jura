# Architecture

The runtime remains a native ESPHome UART component. It has no MQTT transport,
profile-download service or external Home Assistant discovery router.

| File | Responsibility |
| --- | --- |
| `components/jura_native/__init__.py` | Schema, code generation and native entity pools |
| `components/jura_native/profiles.py` | Build-time view of the complete bundled database |
| `components/jura_native/jura_profiles_blob.h` | JPRF v4 data, 75 profiles |
| `components/jura_native/jura_profiles.h` | Flash-safe selected-profile reader and recipe encoding |
| `components/jura_native/jura_cipher.h` | AT escaping/cipher/frame validation |
| `components/jura_native/jura_transport.h` | Incremental encoded UART transmission/reception and transaction timeout |
| `components/jura_native/jura_controller.h` | Session, TF/TV, read queues, recipes, brewing and counter state |
| `components/jura_native/jura_native.h` | Native sensors, selects, numbers and buttons |
| `components/jura_native/jura_progress.h` | Known progress stage glossary |

The four protocol/controller headers and profile data can be exercised without
ESPHome by the host tests. Native adapter tests use small ESPHome test headers.

## Discovery

Entity slots are allocated at code generation. During setup, TY or the first AT
T3 model reply selects a profile, and its entity names/visibility are finalised
before native API discovery. A bounded timeout permits boot without a machine.
A profile hint can supply fallback metadata. An incompatible late model is
blocked until restart rather than mutating an already discovered entity set.

Only the selected profile's runtime metadata is loaded into RAM; the full blob
remains bundled, with flash-safe access on ESP8266.

## Transactions

One UART transaction transmits at a time. Encoded bytes are sent incrementally,
with inter-byte/quiet timing and bounded reception. Session, control and counter
operations are serialised. Handshake recovery uses backoff; active operations
pause idle reads. Valid TV traffic preserves session liveness when TF stops.
Alert freshness is tracked independently from progress freshness.

## Recipe paths

- Native selection/reset schedules a saved-settings read. A selection epoch
  prevents an earlier reply/timeout from applying to a newer selection. Touched
  HA attributes are retained over the arriving baseline.
- Native Brew freezes the loaded baseline plus overrides into one queued recipe,
  then restores controls to baseline. Rejected requests retain edits.
- Named/custom API requests reserve a product and overrides independently of the
  native selector. They read fresh saved settings, merge explicit values and
  recheck readiness before queuing one brew. Preparation cancels if another
  operation begins or the connection/readiness is lost.

Read failures use an explicit all-or-nothing profile fallback. The recipe encoder
validates supported fields, choices, ranges, steps and layout. No stored-recipe
write or automatic brew retry exists. A matching TV product confirms start;
`@TB` alone does not identify the requested drink.

## Cool Control

`components/jura_coolcontrol` is a separate passive UART receiver, retaining the
observed level/temperature field order and scale, with validated status frames,
independent communication/measurement expiry and optional bounded startup probing. It
is not part of the Jura machine's TF/TV protocol path.
