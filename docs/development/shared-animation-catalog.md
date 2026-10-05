# Shared animation catalog

The discovery layer recognizes animation libraries independently of outfits.
This is the first runtime integration step. It does not yet add menu choices,
play clips, resolve live asset failures or expose a CINE service. Those remain
part of the animation-port goal.

An embedded library uses the existing uncompressed V8B metadata pak and
CSS.Package version 1 envelope, with `kind: "animation_library"`. Old packages
without `kind` continue to mean outfits. The embedded manifest remains at
`MortalShell2/Content/CSS/Packages/<id>/manifest.json`.

The package's `catalog` contains exactly `schema: 1` and `animation_library`.
The library's id, name, author and version must match the package envelope.
It has no outfit thumbnail, customization resources or wearable variants.
Container filename, size and table checksum checks are the same as outfit
packages. Full bulk hashing still belongs to the offline release verifier.
The existing Python outfit packager has not yet been extended to build this
new kind, so do not use it to claim an animation-pack release is verified.

Example library body, using illustrative assets:

```json
{
  "schema": 1,
  "animation_library": {
    "id": "css.locomotion",
    "name": "CSS Locomotion",
    "author": "eins0fx",
    "version": "0.1.0",
    "entries": [{
      "id": "css.eve.idle",
      "name": "Eve Default Idle",
      "category": "idle",
      "playback": "loop",
      "slot": "idle",
      "hide_weapons": true,
      "assets": [{
        "asset": "/Game/CSS/Locomotion/Eve/AN_Idle.AN_Idle",
        "skeleton": "/Game/CSS/Shared/SKEL_Base.SKEL_Base"
      }],
      "aliases": [{
        "slot": "idle",
        "id": "eve",
        "asset": "/Game/CSS/Eve/Anim/AN_I2_Idle.AN_I2_Idle"
      }]
    }]
  }
}
```

Stable entry IDs are independent of menu names and asset filenames. Aliases
require the old slot, saved ID and exact asset path. An outfit with a different
wing-driving asset cannot match by the word "Eve" alone. Equivalence must be
audited before putting an alias in the shipping manifest.

`playback` is `loop`, `once` or `pose`. A performance without a slot stays a
performance; discovery never silently turns a dance into a looping idle.
Movement slots require looping playback and preserved weapons. An idle slot
can loop or hold a pose, but cannot declare a one-shot performance. Beacon
montages and facial-expression descriptors need their own integration and
are not accepted through this sequence schema.

Limits are 16 libraries, 256 entries per library, 16 skeleton alternatives per
entry and 64 explicit aliases per entry. Outfits retain their existing 64-choice
slot limit. Shared assets must be under `/Game/CSS/Locomotion/`. Unknown fields,
duplicate alternatives, invalid paths, control characters and conflicting aliases
are rejected. Metadata does not prove that a cooked asset loads or that its
skeleton matches; the playback resolver must verify that separately.

Conflicting package identities, entry identities or alias claims reject all
involved libraries. No filename or installation order wins. Bad installed
libraries receive individual diagnostics without invalidating old outfits.
Loose authoring catalogs remain strict and throw on malformed or conflicting
data. Removing a library leaves the existing outfit catalog intact.

Verification:

```sh
ctest --test-dir build/host --output-on-failure -R 'css_animation_library|css_animations|css_data'
python3 tests/animation_library_packages.py \
  --repak build/repak/release/repak \
  --test-binary build/host/css_animation_library_tests
```

The package check uses real V8B paks with deliberately synthetic companion
files. It exercises metadata discovery, two conflicting packages, an invalid
asset path and a corrupt table checksum alongside an existing outfit. It is
not a cooked-asset or game-load test.
