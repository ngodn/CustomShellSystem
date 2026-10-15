# CSS.Package v1 contract

This is the installed outfit format used by the current tools and runtime. `CSS.Project` is a separate author recipe. The CSS runtime DLL version, outfit version, project recipe version and package format version are independent.

## Required output

One stable outfit ID owns one self-contained trio and one wardrobe entry. A release ZIP contains a folder named after the trio's stem:

```text
CSS_My_Outfit_YourName_P/
  CSS_My_Outfit_YourName_P.pak
  CSS_My_Outfit_YourName_P.utoc
  CSS_My_Outfit_YourName_P.ucas
```

The stem is portable ASCII and ends in `_P`. The three names must match the manifest's companion names. Do not distribute multiple alternatives under the same stable outfit ID; put those alternatives in its variants array. Different outfits need different IDs.

IoStore assets use isolated `/Game/CSS/<id-derived-hash>/...` paths. The converter rewrites references with matching encoded lengths and verifies the result. It preserves external game dependencies. Isolation lets several CSS outfits coexist without all replacing one original mesh path.

The uncompressed V8B pak is mounted at `../../../` and contains:

```text
MortalShell2/Content/CSS/Packages/<outfit-id>/
  manifest.json
  thumbnail.png
  conversion.json
  dye-*.png         (only when referenced by a color recipe)
```

Generate these through the tools. Editing the final manifest or renaming containers after verification invalidates the release checks.

## Manifest fields

| Field | Contract |
| --- | --- |
| `format`, `format_version` | `CSS.Package`, `1` |
| `game`, `engine` | `MortalShell2`, `5.6`; this is not proof of compatibility with every 5.6 game build |
| `id`, `name`, `author`, `version` | Stable identity, display name, credit and outfit version |
| `source_url`, `thumbnail_source` | Source/artwork provenance |
| `thumbnail` | `thumbnail.png`, dimensions and SHA-256 |
| `containers` | `.utoc` and `.ucas` filenames, byte counts and SHA-256 |
| `catalog` | Schema 1, exactly one outfit with matching identity and thumbnail |
| `resources` | Exact union of referenced dye images, each with dimensions, bytes and SHA-256 |

The catalog outfit declares `shells`, `compatibility`, `variants`, optional shared `colors` and display metadata. The converter emits `compatibility: "same_skeleton"`; the runtime also verifies Skeleton object identity before applying a mesh. A source shell tag is neither an unlock nor a gameplay-shell-switch instruction.

Each variant declares a stable `id`, `name`, relocated mesh object path, optional material-slot overrides and optional colors. At least one variant is required. A variant color recipe replaces the shared one. Resource filenames reused by multiple variants must refer to identical bytes.

## Validation and generated data

An optional catalog-variant `mesh_scale` multiplies the appearance mesh's
component scale. It defaults to 1 and accepts finite values from 0.1 to 4.
CSS applies the same factor to the wardrobe preview and restores the original
component scale when leaving the appearance. The gameplay capsule is unchanged.
Equipped weapon roots receive an inverse scale correction while attached to the
scaled body, including body weapons and shell tools. Their native scale restores
on removal or an appearance change. Other attached cosmetics inherit the body's
scale, so authors must check accessory placement. Older runtimes ignore this
field; it requires CSS 1.0.0.

An optional catalog-variant `ground_offset_cm` sets a fixed world-mesh height
correction. CSS 1.0.0 accepts a finite number from -20 to 20 centimeters and defaults
to zero. Negative values lower the mesh. CSS preserves the original component
height and restores it when leaving the appearance. It does not resize the
body, move the gameplay capsule or alter preview framing. This requires the
Next-Gen grounding runtime; older CSS builds ignore the field. Include it in
the source catalog before packaging, then verify visual contact in game.
Players can override it per variant with Customize > Placement > Ground height
(1.0.0-beta.5); the declared value is where that setting starts and what its
reset returns to.

Earlier runtimes accept only -10 to 10 cm. Packages requiring a larger heel
offset need CSS 1.0.0; do not clamp their authored height to fit an older runtime.
This bounded range also applies to saved overrides and the Ground Height slider.

For removable footwear, a variant can instead select its authored height from
a section toggle (CSS 1.0.0):

```json
"ground_offset_toggle": {
  "control": "boots_visible",
  "shown_cm": 9.7697432,
  "hidden_cm": 0
}
```

The control must exist in the variant's effective customization recipe, have
kind `toggle` and declare `sections`. Both heights must be finite numbers from
-20 to 20 cm. This rule replaces the fixed authored `ground_offset_cm` while
present; it is not an additive offset. Visibility follows the same default,
palette and per-control override used by customization. Variant controls
replace shared outfit controls as usual. Invalid rules reject the package.

A player's explicit Ground Height setting still overrides the authored value.
Reset clears that override and follows the current footwear selection again.
No derived height is written into profiles, so saving hidden boots and loading
that profile retains automatic placement. Older CSS builds ignore this rule;
packages that require it must list CSS 1.0.0 as a dependency in their release
instructions. Test boots shown/hidden, profiles, facial mesh switches and
doubles before shipping the outfit.

An optional `fit_shell` (on the outfit, or on a variant to override it) names
the shell whose body the look is sized like, as a CharacterId tag such as
`CharacterId.Player.Shell.Genessa`. Each shell's character data tunes where its
stowed weapons, sidearm and seal hang on that shell's body. With MISC > Gear
position on Auto (the default), CSS hangs them by the fit shell's table instead
of the worn shell's, so a Genessa-sized look worn on Sariel or Gragu keeps its
gear against the body. Without the field, the outfit's first listed shell is
used. Use Original Shell looks fit their own shell, and replacement mods fit the
shell they redraw. NPC and enemy looks keep the worn shell's placement. A fit
shell CSS cannot find in the game's shell list leaves the game's placement in
place. Older CSS builds ignore the field. See
[socket fit](../development/socket-fit.md).

The converter records original/tool hashes, relocated paths, export identities and round-trip checks in `conversion.json`. `runtime_tested: false` means exactly that. In-game testing is recorded separately; an offline build does not mark itself gameplay-tested.

The offline verifier hashes the full `.ucas`. Runtime discovery checks the smaller metadata, thumbnail, resources and `.utoc`, and checks the bulk companion's name and size. It deliberately avoids hashing large bulk files on the game thread.

CSS extracts thumbnails and dye resources into a rebuildable package cache. User selections, favorite IDs and templates belong to CSS's own state. Cache files, loose developer catalogs, personal state and DLLs never belong in an outfit trio or outfit ZIP.

A new asset container requires restarting the game. Native core live reload does not hot-replace a mounted pak/IoStore set. See [verification and install details](../css-packages.md).

CSS's built-in Tarsus, Droeg, Nameless Captive and Sir Isaac appearance choices
use isolated assets supplied by CSS Shared Assets, under
`/Game/CSS/eins0fx/Bosses/`. They preserve the original game packages. Install the
matching shared pack and restart when updating these appearances. Merrick and
the existing native human-skeleton choices continue to use game assets directly.

## Discovery failures

CSS 0.2.1 searches `Content/Paks` recursively, including `~mods`. Keep the recommended one-folder layout above. Invalid packages are rejected individually, with filenames and reasons in `CSS.log` and `runtime/status.json`; a bad neighbor no longer aborts the entire catalog. See [discovery and diagnostics](../package-discovery.md) for validation behavior and regression commands.
