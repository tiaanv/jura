# Building and releasing the card

Run these commands from the repository root with Python 3 and Node.js:

```sh
python card/build.py
python card/build.py --check
node --check dist/jura-native-card.js
python -m http.server 8767 --bind 127.0.0.1
```

Open `http://127.0.0.1:8767/card/tests/card.html` in a browser. Every check must
show PASS. Its Home Assistant service calls are stubbed: this page cannot brew
a real drink. Scenario buttons let you inspect the graphical states. These are
synthetic examples, not live machine screenshots.

Edit `card/src/`, then rebuild and commit `dist/jura-native-card.js`. The build
is deterministic and embeds the CSS and machine SVG in one JavaScript file.
No generated bundle depends on files outside this repository. Keep the version
header in `card/src/jura-native-card.js` aligned with `card/VERSION`.

## Release checklist

1. Work on `dev`; review and merge to `main` before public release.
2. Run the build check, JavaScript syntax check and browser regression harness.
3. Validate HACS using the `Validate HACS card` workflow against the relevant ref.
4. Check the README, examples, card changelog and screenshots. Screenshots should
   contain only the card, with no sidebar, URLs, credentials or unrelated devices.
5. Test a clean manual install and a HACS custom-repository install; check that
   only one resource registers `custom:jura-native-card`.
6. Create a GitHub release on the merged main commit, using a unique version
   such as `v2.0.8`. Attach `dist/jura-native-card.js` as `jura-native-card.js`.
7. Describe card and ESPHome changes separately: GitHub releases are shared by
   the repository, while component and card version files are independent.

HACS targets the exact `filename` in root `hacs.json`; it can download the bundle
from `dist/` or the matching release asset. Keep that filename stable. Release
assets matter because HACS can prefer a matching asset on the latest release.
For future component-only releases, also attach the current card bundle so HACS
continues to offer the card consistently.

Nothing in these workflows publishes a release or merges branches automatically.
The `dev` package is a review candidate; ordinary HACS users should install after
it is merged and a release is published. Manual installation can test dev first.
