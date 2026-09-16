# Runtime releases

The release ZIP contains a single `CustomShellSystem/` folder to extract into
`MortalShell2/Binaries/Win64/ue4ss/Mods/`. Outfit packages and UE4SS are separate.
The canonical version is the root `VERSION` file. CMake generates the loader's
version from it. Tags use `vX.Y.Z`; GitHub release titles use `MSII - CSS vX.Y.Z`.

## Build

Commit the intended source, update VERSION, and create an annotated version tag.
Build from a clean checkout at that tag:

```sh
python3 tools/css_release.py build --sdk /absolute/path/to/ue4ss-sdk-d7e7826d
python3 tools/css_release.py verify dist/releases/MSII-CSS-v0.3.2.zip
```

The script builds both Release DLLs from that checkout and uses an explicit
seven-file payload allowlist plus generated release metadata. It never copies
from the installed mod. The only runtime interface image needed is inventory-logo-v1.png;
old outfit seals and developer catalogs are excluded. One loader and one core
ship, with core.json pointing to that core. No state, backups, requests, cache,
logs, diagnostics, game content, SDK, import libraries or debug symbols ship.
Third-party notices and short installation instructions accompany the runtime.
The generated manifest records version, source revision and payload hashes.
The ZIP checksum is written beside it. Existing release archives are not overwritten.

## Fresh-start verification

`css_startup_tests` calls the production state loader, catalog discovery and
atomic writer. It starts with no state or catalog folder, verifies default-file
creation, settings persistence, backup recovery, and regeneration after deletion.
With a pak fixture directory, it checks package discovery and thumbnail/cache
creation for every variant, deletes the cache and confirms regeneration. It does not mutate the
installed mod or the game save.

```sh
cmake -S native -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
ctest --test-dir build/host --output-on-failure
python3 -m unittest discover -s tests -p 'test_release.py'
# Extract the finished ZIP to a temporary directory first:
build/host/css_startup_tests /temporary/extracted/CustomShellSystem /path/to/Paks/~mods
```

The extra test files belong only to the temporary extraction. Reverify the
original ZIP afterwards. These checks cover production persistence/discovery
code and archive cleanliness. A new in-game cold start of the exact ZIP is a
separate check, not something a host test establishes.

## 0.1.1 scope

Includes the confirmed inventory material fix and first-run initialization fixes.
The working-tree inventory-derived lighting experiment is excluded because its
visual behavior has not been confirmed. The known wardrobe lighting difference
remains in the user-facing notes. No authored or converted outfit is bundled.

Version 0.1.1 was published at [GitHub](https://github.com/ngodn/CustomShellSystem/releases/tag/v0.1.1).

## 0.1.2 scope

Includes the confirmed beacon/player transition recovery, scrollable full catalog,
per-variant materials and color recipes, scoped asset lifetime protection during
loading, and fresh-install checks for every variant's color resources. The user
confirmed all four new port bundles work and mouse/controller scrolling reaches
the full catalog. The fourteen-variant live pass covered palettes, custom RGB,
Original restoration, preview animation/cloth, unchanged gameplay animation and
clean close.

The unrelated lighting experiment remains outside the release tree. Outfit
packages are separate downloads. Build from the clean `v0.1.2` checkout; the
prepared GitHub title is `MSII - CSS v0.1.2`, with notes in
`packaging/release-notes.md`. Creating a local tag and ZIP does not publish them.

## 0.2.0 scope

Inventory integration replaces the standalone N wardrobe. The release includes
Shell, Color and Templates sections, native character display/camera controls,
viewport-aware layout, menu transitions, and game-relative package discovery.
The package format remains CSS.Package v1 and state remains compatible.

Pre-release checks passed: 49 Python tests and five native suites. The existing
BeauteGenessa, BeauteKnightLady, HIT2 and Seductress ZIPs were extracted without
repacking and checked by the current native package reader, including cache
reuse, thumbnail/color repair and corrupt-index rejection. The final runtime ZIP
must additionally pass its hash/allowlist verifier and fresh-state checks before
publication. Exact-artifact cold-start gameplay remains a separate check.

## 0.3.0 packages

For the initial 0.3.0 release, all four versions matched VERSION. Build its optional framework and extensions from
that same clean annotated tag after building CSS:

```sh
python3 tools/cssx_release.py build --sdk /absolute/path/to/ue4ss-sdk-d7e7826d
python3 tools/cssx_release.py verify dist/releases/MSII-CSSX-v0.3.0.zip
```

CSSX installs inside CustomShellSystem; each extension installs under extensions/.
The framework ZIP includes exactly one versioned core, selector, logo, README,
notices and payload manifest. The extension packager includes only declared
runtime files and notices. Neither packager copies from the live installation.
The UI Kit is Lua; Cheat Menu is native C++. Development request interfaces are
compiled out of CSS release builds. No user state, logs or caches ship.

CSS.Package v1 is unchanged. Known outfit issues and untested encounter cases
remain in the [parity notes](development/cheat-menu-parity.md) and are not treated
as verified fixes. The latest live tests used Steam build 25265616.

## 0.3.1 scope

Fixes the white shredded-body displacement during dodges with CSS outfits while
preserving the separate native translucent trail. Compatibility materials reuse
the existing owned-MID and recovery path; standalone cloth-driver materials are
left intact. See [the investigation](development/0.3.0-user-bugs.md).

CSS.Package v1, state schema and extension ABI are unchanged. Existing outfit and
port ZIPs do not need repacking. CSSX and its two supplied extensions remain at
0.3.0 and need no update. The Seductress blood-mask
finding is documented but its asset correction is not part of this release.

Build only CSS from clean annotated tag `v0.3.1`. CSSX and extension releases
are independent; do not bump them solely to match a CSS patch. The existing
`cssx_release.py build` command is for the coordinated 0.3.0 build and is not
part of this patch release.
Exact-artifact startup, archive and checksum checks are recorded separately in
[the verification record](development/0.3.1-release-verification.md).

## 0.4.0 scope

A feature release, and the first one to change the COLOR tab since it was written.

**Colour.** The tab is now a palette band, then one section per group: OUTFIT and
BODY, each opening with a Tint row (hue, saturation, brightness) that moves everything
under it. Controls carry a `group`, a `role` and `hue_locked`, so metal, gems, skin and
the body's pigments take a group's brightness and saturation but keep their own hue.
Picking a part opens a strip of swatches (the author's colour, that part in each
palette, then hues and shades of it) with exact RGB behind a toggle. Choosing a palette
now takes over only the parts that palette sets and the tint of their groups, so a
custom skin survives changing the dress; `original` still clears everything.
[color-convention.md](color-convention.md) is the written standard the three new
manifest fields belong to, with `lint_convention()` in `tools/css_colors.py` for
package builders to fail their own build on. All three fields are optional: CSS infers
them from the control id, so every package published before this keeps working.

**Animation.** Adds the ANIMATION tab with Walk animation (Normal or Feminine), driven
natively from the game's own `BS_CultistSpearLady` blendspace plus a walk-speed
pre-hook, with no pak and no dependency on argisht's GenessaWalk or ProximaWalk mods.
Those mods are detected and named in the tab: on Normal CSS does not touch locomotion
so they keep working, on Feminine CSS re-asserts the animation on every mismatch so
they cannot take it back. The Jog and Sprint options from the 0.3.3 preview are gone.
Neither borrowed run kept up with the ground properly, so both gaits stay on the game's
own animation; a state or template written by the preview loads and comes back Normal.

**Stowed items.** Outfit manifests can carry per-socket corrections. A stowed prop is
welded to its socket and never tested against the body, so on a wider shape it starts
buried and the stride swings the body through it. CSS measures the prop against the
body's own physics asset every frame (`GetClosestPointOnCollision`) and holds it a
declared clearance off, following the live pose. A fixed offset is the fallback for a
mesh with no collision to measure. Nothing is touched while an animation borrows the
prop, such as a parry reaching for the seal.

**Also.** Fixes switching to a shell reverting its outfit to the first one installed (a
reconcile could finish against a shell that had already changed underneath it). Fixes
the random dark glossy body, where a dye render target was bound before its layers were
drawn. Fixes a mouse drag on a Tint slider reading the colour sliders' `channel` key.
Clipped sliding tab strip. Numbered-field lookup in the CSSX bridge.

Templates gain the animation setting (`{"selections":..., "walk_animation":...}`, older
files migrate). State schema stays 1 with additive keys, and saved looks gain `tints`.
Outfit package format stays v1 with the optional `attachments` field and the optional
`group`, `role` and `hue_locked` on a colour control.

Build only CSS from clean annotated tag `v0.4.0`. CSSX, UI Kit and Cheat Menu
remain at 0.3.0. The BBCode comment is [CSS 0.4.0](nexus/css-v0.4.0-changelog.bbcode.txt).

## 0.3.2 scope

Corrects missing accessory socket-parent bones using a hidden stock-mesh pose
follower. Live Long Hair checks pass for Tiel's dagger and Eredrim's diapason,
native draw/stow, six alternating shell swaps, Original restoration and core
reload cleanup. Three material recovery cycles preserve active effects; native
Inventory cleanup passes. See [the attachment investigation](development/jayluk3-longhair-attachments.md).

Build only CSS from clean annotated tag `v0.3.2`. Outfit package format, state
schema and extension ABI are unchanged. No outfit or port needs repacking, and
CSSX, UI Kit and Cheat Menu remain at 0.3.0. The separate portal warning and
shell-switch crash reports are not established fixes in this release.

The BBCode comment is [CSS 0.3.2](nexus/css-v0.3.2-changelog.bbcode.txt).
Artifact and exact-core verification are recorded in
[the release checks](development/0.3.2-release-verification.md).
