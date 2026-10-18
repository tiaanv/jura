# Bundled profiles

The runtime includes all **75 JPRF v4 profiles** from the bundled upstream
snapshot. It chooses the longest matching EF model prefix and loads only the
selected profile's metadata into RAM. The full data stays in firmware; no model
profile download or MQTT service is involved.

- [Model and named-product index](models.md)
- [Readable database export](database.json): products, codes, parameter positions,
  defaults, limits, choice labels, maintenance names and alert bits/flags.
- [Sources and licensing](../sources.md)

These exports document the data; editing them does not change firmware. The
single source for code generation/runtime is
[`jura_profiles_blob.h`](../../components/jura_native/jura_profiles_blob.h).
Regenerate the exports from the repository root with:

```sh
python tools/export_profiles.py
```

The blob's defaults describe the model profile. They are not the recipes saved
on an individual machine. V2 reads saved recipes separately and reports a
profile fallback if that read cannot be validated; see [saved recipes](../saved-recipes.md).

A listed model does not establish a working handshake, sufficient target memory,
or physical validation of every alert, parameter or brew path. Blank families
are shown as unspecified in the index rather than guessed. See [testing](../testing.md).
