# Card update brief: dispensing-stage fallback

Work on branch `dirthvader`. The native component changes are implemented there;
this follow-up is for the HA card only. Preserve other component work on the
branch and refresh it before editing.

## Entity contract

- Existing `progress` / **Jura Progress** remains whole-drink progress. Prefer a
  finite value in 0..100, including zero.
- New numeric key `stage_progress` / **Jura Stage Progress** is 0..100 percent
  of the active dispensing stage, computed by the component from stage quantities.
  It can be valid even when overall progress is unavailable, or alongside it.
- Existing `progress_state`, `stage_current`, `stage_target`, `stage_unit`,
  `brew_active`, connection and status entities retain their meanings.
- Stage progress is unknown for non-dispensing stages, invalid/missing quantities,
  zero targets and quantities above target. It clears on finish/idle,
  disconnection and progress timeout. It may reset between stages.

## Card behaviour

1. Discover/map the new sensor using the card's existing entity mapping patterns;
   support an explicit configured entity and retain older-firmware compatibility.
2. Prefer valid overall progress. Otherwise use valid stage progress, labelled
   with the active phase, e.g. **Dispensing coffee - 50%**. Update the progressbar
   accessibility label as well; do not call the fallback overall drink progress.
3. Zero is valid. Unknown/unavailable/empty values must not become zero. If neither
   percentage is valid, hide the percentage/bar and keep the phase/quantities.
4. Stage 100% is not drink completion. Keep existing observed-finish/readiness
   logic; do not clear the brew lock or mark finished solely from stage progress.
5. Add tests for overall preference, stage fallback, zero, neither available,
   stage transitions, 100% without finish, disconnect/timeout and older firmware.
6. Update card docs, build the distributable with the existing build process,
   and run the existing card tests. No native API signature changes are needed.

## Other component changes

`read_saved_recipes` defaults to true. False skips automatic reads for controls
and API brews; profile defaults plus explicit one-shot overrides are used, and
recipe source/status explain this. The manual diagnostic probe remains enabled.
The EF533 raw extra's registration key `brew_movements` remains, but its display
label is now **Legacy Coffee Rinse Count (unverified)**. Do not rely on the old
label to identify it or treat it as a confirmed brew-unit movement counter.
