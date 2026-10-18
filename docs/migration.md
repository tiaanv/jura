# Migration

## An existing native V2 device

The repository reorganisation does not change `components/jura_native`,
`components/jura_coolcontrol`, configuration keys, native entity names or API
method signatures. Existing GitHub-source device YAML continues to work.
A documentation-only layout update does not require reflashing the ESP.

File locations changed:

| Previous location | Current location |
| --- | --- |
| `example-e8-github.yaml` | [examples/e8.yaml](../examples/e8.yaml) |
| `example-e8.yaml` | [examples/e8-local.yaml](../examples/e8-local.yaml) |
| `migration-e8.yaml` | [examples/migrate-e8.yaml](../examples/migrate-e8.yaml) |
| `example-coolcontrol.yaml` | [examples/coolcontrol-local.yaml](../examples/coolcontrol-local.yaml) |
| Root text guides | This `docs/` directory |
| `PROFILE_DATABASE.json` | [docs/profiles/database.json](profiles/database.json) |
| `PROFILE_REFERENCE.txt` | [docs/profiles/models.md](profiles/models.md) |
| `tests/export_profiles.py` | [tools/export_profiles.py](../tools/export_profiles.py) |
| `tests/package_release.py` | [tools/package_release.py](../tools/package_release.py) |
| Old `components/jura` and examples | [legacy/](../legacy/README.md) |

Local examples now use `../components`, relative to their new `examples/` path.

## The legacy `jura` component

1. Change the Git source to `ref: V2` and `components: [jura_native]`.
2. Replace the old `jura:` block with the block in
   [the merge example](../examples/migrate-e8.yaml). The ID may remain `jura_coffee`.
3. Keep the existing UART/electrical wiring, device name, Wi-Fi, API encryption,
   OTA and secrets. The reference E8 UART is TX GPIO4, RX GPIO3, 9600 8N1.
4. Remove old `uart.write` drink sequences and old template Brew/Turn Off
   buttons. Use the generated native controls or the new API actions.
5. Merge `api.actions` from [the E8 example](../examples/e8.yaml) into your
   existing API block if your HA card needs the actions. Do not duplicate `api:`.
6. Compile/upload. Use Clean Build Files if an old component source persists.
7. Update card entity mappings from the discovered HA device. Similar names do
   not guarantee the same entity IDs or retained history; discovery identities
   differ. Keep old history until you have verified the replacements.

| Old option | Native V2 behaviour |
| --- | --- |
| `enable_at_protocol` | AT is always used |
| `status_source: tf` | TF/TV supply status and progress |
| `poll_ic` | No IC polling |
| `at_handshake_response` | `handshake_response`, normally `auto` on the tested E8 |
| `startup_delay` | Same option name |
| `model: E8` | Automatic EF identification; optional `profile_hint: EF533` |

`components/jura` is no longer an installable root component on V2. Its source
is preserved under [legacy/components/jura](../legacy/components/jura). Use Git
history/an appropriate older branch for the old installation, or migrate to
`jura_native`; the archive is not the current installation path.

Legacy FA button numbers are not interchangeable with AT product codes. The
new API accepts the profile's drink names, such as `1 Flat White`, and reads
saved settings before sending a full one-shot recipe.

All inherited raw-register labels are unverified, including grounds, brew
movements and brews since cleaning/descaling. Treat the profile-based AT counts
as primary readings and the raw values as investigation data; see
[diagnostics](diagnostics.md). The optional Cool Control remains a separate receiver.
