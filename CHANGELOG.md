# Change log

## Repository layout update

- Current guides moved under `docs/`; root README now describes the native V2 component.
- Current ESPHome configurations and HA actions live under `examples/`.
- Profile exports live under `docs/profiles/`; export and packaging tools live under `tools/`.
- The old `jura` source, examples and documentation are archived under `legacy/`.
- Documentation marks all inherited raw-register interpretations as unverified.
- Portable tool paths, example validation, source packaging and documentation links updated.
- No change to native firmware behaviour, component install paths or entity names.

## 0.3.0

Named/custom API actions read the requested product's saved settings before
brewing. Explicit custom fields override saved values; read failures use a
labelled profile-default fallback. Readiness is rechecked and pending preparation
is cancelled if an operation begins or the connection/readiness is lost.

## 0.2.0

Selecting a drink loads its saved settings into native controls. Source/loading/
modified state and fallback reasons are exposed. Native Brew consumes edits for
one accepted request; Reset Recipe reloads the machine's saved recipe. Selection
and reset races discard obsolete replies.

## 0.1.x

Initial native component with the full bundled profile database, TF/TV monitoring,
profile-labelled counters, native controls and nonblocking transactions. Follow-up
fixes cover AT-only boot identification and read-only saved-espresso diagnostics.
