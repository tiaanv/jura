# Card changelog

## 2.0.9

- Suppress the expected TF stale-status banner during active TV-reported
  operations. Physical machine alerts remain visible; stale warnings resume
  after the active operation ends. Readiness and Brew gates are unchanged.

## 2.0.8 — first public card package

Includes the live-tested development series 2.0.0–2.0.8:

- Native Jura entity mapping, graphical drink selection and explicit Brew.
- Machine-saved recipes, supported per-drink controls and one-shot adjustments.
- Observed product, stage quantities and independent overall progress.
- Grinding, coffee, milk, foam, heating, rinse and completion visuals.
- Optional independent Cool Control milk level and temperature.
- Friendly machine alerts and drink-stage messages; raw values in Diagnostics.
- Fresh Ready status clears retained operation panels. Offline hides alerts.
- Fresh switch-off status clears retained coffee-rinse visuals when its rinse
  flag is no longer set; fresh active rinse remains visible.
- Milk highlight travels from cooler to machine; heating glow excludes cooler.
- Grounds container uses the hockey-puck icon.
- HACS packaging, build verification, examples and installation documentation.

The initial public package retains the development version number. It does not
change the firmware version or introduce new drink commands.
