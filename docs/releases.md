# Component releases

`jura_native` and `jura_coolcontrol` share a release version, recorded in the root
[VERSION](../VERSION). Stable releases have immutable `components-vX.Y.Z` tags
and matching GitHub Releases. The legacy `jura` source remains available in the
same repository snapshot; migration is optional.

The card retains its own version in `card/VERSION`, changelog and release tags.
A component release does not release a new card or change its version.

## Installing and updating

Pin both devices to the same component release when using the cooler bridge:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/tiaanv/jura
      ref: components-v1.0.0
    components: [jura_native]
```

On the cooler device, use `components: [jura_coolcontrol]` with the same tag.
Retain the rest of your existing configuration. Change the tag deliberately,
validate the YAML and rebuild/upload each affected device to update. To roll
back, restore the previous tag and its compatible YAML, then rebuild/upload.

`main` follows stable development and can change between releases; `dev` is for
new features and hardware testing. Tags provide reproducible source snapshots,
not precompiled device firmware or a pinned ESPHome toolchain.

## Version policy

- Patch (1.0.1): compatible fixes.
- Minor (1.1.0): compatible optional features.
- Major (2.0.0): breaking configuration or behaviour changes.

Releases describe any mixed-version bridge limitations and whether both devices
must be updated. Prefer matching versions; do not assume compatibility across
major versions. Pre-release testing can use `components-v1.1.0-rc.1` tags.

## Maintainer checklist

1. Accumulate changes under Unreleased in the root changelog. Record affected
   components, migration steps, compatibility and known issues.
2. Test on dev, then merge the intended changes into main. Distinguish host/schema
   checks from hardware confirmation; list ESPHome versions and machine evidence.
3. Set root VERSION, move the accumulated notes into a dated release entry and
   add detailed notes under `docs/releases/`. Leave a new Unreleased section.
4. Run the relevant regression suites, schema/code-generation checks for changed
   schemas, documentation checks and checksum refresh. Preserve contributor
   notices and the disclaimer. Commit the reviewed release preparation to main.
5. Tag that exact commit `components-vX.Y.Z`; never move a published tag. Publish
   a GitHub Release using the reviewed notes. GitHub's source archives are enough
   for external_components; no card bundle or generic firmware binary is needed.
6. Publish component releases with `make_latest: false`. Keep the card release as
   GitHub's Latest release for HACS. Publish card updates separately with their
   existing tags/assets; do not reuse the component version for the card.

## Releases

- [Components 1.0.0](releases/components-v1.0.0.md)
