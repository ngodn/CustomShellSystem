# Editable sources for Bikini, Knitwear and Alice

All 21 selected parts now reproduce the retained imports for these three
accepted outfits. Bikini and Knitwear pass directly from their saved libraries.
Alice needed two reconstructed source parts. No game assets, body settings,
fit, physics or installed materials changed.

| Outfit | Parts | Points | Triangles |
| --- | ---: | ---: | ---: |
| Vacation Bikini | 7 | 137,402 | 200,619 |
| Casual Knitwear | 7 | 146,464 | 216,613 |
| Midsummer Alice | 7 | 147,947 | 220,611 |

The [source manifest](../development/eve-source-parts-20261014.json) records each
library and comparison hash, accounts for every import part, and links the
existing import-to-cooked comparison. Current Eve container hashes and shared
binary container hashes still match those readbacks. CW's later path-only
update does not alter these Eve inputs.

## Two Alice source mismatches

`work/eve26/alice-cw4.blend` retains matching suit positions, weights and body
morphs, but 52 of its 43,166 evaluated triangles use different diagonals from
the retained import. The imported UV corner keys all exist in the library;
the resulting ordered triangle/corner comparison fails, and matched corner
normals differ by up to 1.93803 in vector length. This is a source reproduction
failure, not a demonstrated defect in the installed outfit.

The original ribbon in `CSS_SeduXtress_Variants_Fixed.blend` matches geometry,
UVs, normals, colors and morphs, but its weights differ from the accepted
import by up to 1.0. Both original libraries and failed reports remain intact.

Two import-derived libraries now preserve the accepted triangle connectivity,
weights, morphs and corner data. They are under Eve's
`work/eve-source-chain-20261014`:

- `alice-suit.blend`: 22,300 points and 43,166 triangles.
- `alice-ribbon.blend`: 624 points and 1,224 triangles.

[`save_import_part.py`](../../tools/eve-fit/save_import_part.py) reuses Skin
Suit's lining recovery method: explicit triangles, scene-linear color values,
all UV channels, neutral body shape keys and guarded float export normals.
Geometry or normal edits invalidate those guards and require regenerated
normals. Blender's preview still uses its packed normals. These files are
reconstructions from the accepted interchange, not original author files or
complete rig/material/cloth projects.

## Verification and reproduction

Fresh Blender 5.2.2 LTS processes run the existing part checker without resetting
saved body controls. All selected parts pass triangle order, named weights,
body and declared corrective morphs, three UV channels, corner normals and
vertex colors. Maximum differences across the selected parts are:

- Position: 0.000015296 cm.
- Named weight: 4.729e-8.
- Positional morph: 0.000077595 cm.
- Normal vector length: 2.022e-7.
- UV components and color bytes: exact.

Evidence is in `work/three-outfit-rig-physics/eve-source-chain-20261014`.
`plan.json` and `results` retain the initial 19 passes and two failures;
`repaired/plan.json` and its results retain the two replacement passes.
`alice-triangulation.json` isolates the suit connectivity difference.
Rebuild commands, exit codes, logs and fresh reload receipts are retained.
An initial driver failed because the generator script had not been created;
its failure log remains, followed by the successful `rebuild2` records.

[`run_eve_source_checks.py`](../../tools/diagnostics/run_eve_source_checks.py)
accepts an explicit plan and Blender binary. It refuses an existing result
directory, records every command and keeps failures. The manifest's pinned
`assemble_manifest.py` verifies each report's inputs, complete part coverage,
the shared import hash and the linked cooked readback before writing the
manifest. To repeat, use a new plan directory and output manifest location.

The evidence uses about 167 KB; the two new source libraries and receipts use
about 4.13 MB. No full projects or cooked packages were copied. `df -h` before
and after this source work reports root 95G available and theark 15G.

This closes editable-part reproduction for the listed fields. Source armatures,
complete assembly/material recipes, cooked normals/tangents and additional UVs,
cloth, posed fit, gameplay and performance retain their separate gates. The
linked cooked comparisons have narrower surface-field coverage than these
Blender comparisons. The remaining outfits are not covered by this report.
