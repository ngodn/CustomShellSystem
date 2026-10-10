# Peony wardrobe controls and material bindings

The wardrobe fragment is now merged into a private seven-variant Eve catalog
under `work/peony-catalog4`. This is not an installed outfit or a complete
release package. Fit, final cooking, facial adapter registration, Astral support
and gameplay checks remain. The hip-accessory contact repair is still open.

## Complete control merge

`tools/eve-fit/assemble_peony_catalog.py` retains all six existing variants and
adds Peony with 40 controls: the 22 wardrobe controls below, six body shapes,
six skin/hair colors, five existing rig controls and hair visibility. Hiding
hair also hides its sword accessory, avoiding an accessory floating without
the ponytail. The new variant retains the donor's shared animation references.

The builder compares Peony's inherited body/hair with the accepted Skin Suit
interchange before reusing its dye masks and controls: 93,355 points, 115,429
ordered triangles, 379 bones, weights, UVs, corner normals/colors and all 22
authored morphs agree exactly. Deliberately changed body positions, UVs,
weights and morph deltas are rejected. This proves preservation of these
inputs, not new gameplay physics or material rendering.

Nine dye surfaces use 13 existing resources. The full seven-variant catalog
retains all 106 dye resources and the existing thumbnail, with verified hashes.
The covered-foot alias uses its original foot dye surface; boot lining uses
the matching leg material's skin surface. No garment receives an anatomical
dye surface. Existing six-variant definitions remain unchanged; no shipping
version or container identities are changed.

The previously implemented CSS `ground_offset_toggle` rule is now connected to
Peony's `toggle_boots`, with shown height 9.769743204116821 cm and hidden height
0. This reuses commit `c95fe26`, rather than adding another runtime height path.
Manual Ground Height keeps priority. Earlier paragraphs about missing automatic
placement describe the initial fragment; gameplay contact and doubles placement
still need verification.

The current native catalog loader reads the new catalog successfully.
`tools/eve-fit/check_peony_catalog.cpp` tests all 2,048 combinations of the ten
garment toggles plus hair, including correct boot coverage and authored height.
All 10,240 palette/profile combinations preserve visibility, height and the 17
inherited body/hair color, shape and rig controls. Moving between booted and
barefoot placement restores the original component height without accumulation.
This is a host check of the actual CSS control/catalog functions, not a live
Unreal component test.

Evidence is `assembly-receipt.json`, `native-checks.json`, `build-command.json`
and `cleanup.json` under `work/peony-catalog4`. Intermediate candidates 1-3
retain their small receipts and catalog definitions; 226 duplicate images were
removed only after matching the complete candidate's hashes, reclaiming
124,749,846 bytes.

Before cooking, add the existing shared cloth-morph correction to Peony where
required. The Genessa helper currently restricts inputs to its W3 meshes and
the `UG_Skirt` slot, so it cannot simply be run on Peony unchanged. The shared
graph and runtime contract already exist; preserve those assets while adapting
the outfit-specific attribute authoring for Peony's two cloth sections. Saved
morph deltas alone do not prove the cloth render path applies them.

## Inputs and ownership

`tools/eve-fit/build_peony_customize.py` consumes the retained fit11 interchange,
hipnative2 material bindings, Eve's five authored Peony palettes and the existing
exact-face footwear coverage recipe. It writes under Eve's
`work/peony-customize1`; it does not modify those inputs, an installed package,
or a release manifest. Its receipt pins every input and the builder.

The fragment has 22 controls:

- Ten independent garment switches: jacket, corset, choker, gloves, arm ornaments,
  hip accessories, underwear, leg panels, stockings and boots.
- Five colors: fabric, trim, metal, stockings and trim glow.
- Seven material controls: glow strength, glow coverage, flow speed, stocking
  opacity, fabric roughness, trim roughness and stocking roughness.

Both cloth sections (34/38) belong to Leg panels. Their former Arm/Lower_1
sections (27/30) are empty after trim consolidation and receive no controls.
Boot sections 26/28/33/36 hide covered-foot sections 18/19/37 while shown.
The builder verifies all 5,328 covered face identities, rather than treating
every skin or stocking section as covered. Removing boots restores those skin
sections. Automatic boot-dependent ground placement is still pending; the
fragment does not pretend the existing 9.769743 cm sole difference is resolved.

All five palettes set the five outfit colors only. They preserve garment
visibility, opacity, glow settings, body controls and placement. Colors remain
sRGB in the catalog; CSS performs its normal conversion before vector material
writes. Palette names are Monochrome, Crimson Silk, Midnight, Jade and Obsidian Gold.

## Validation

The normal Python recipe validator and convention lint pass. Deliberately
changed slot names, a boot rule that hides the corset, a missing covered face,
an unknown panel material family and a missing fifth palette are rejected.

`tools/eve-fit/check_peony_customize.cpp` links the freshly rebuilt current
`css_data` library. It exercises actual control commands, hidden-section
resolution, palette selection and saved customization restore:

| Check | Result |
| --- | --- |
| All ten-switch combinations | 1,024 pass |
| Each combination with each palette | 5,120 pass |
| Palette/settings save and restore | 5 pass |
| Default boot coverage | Only sections 18/19/37 hidden |

The palette preservation check adds a representative body-shape control to the
fragment. This checks merging behavior, not the final Eve body-control catalog.

`tools/eve-fit/check_peony_material_bindings.py` reads the saved material instances
through UE 5.6.1. It enumerates the actual scalar/vector parameters and compares
the native values with the catalog defaults for each palette. This uses Epic's
[MaterialEditingLibrary parameter API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MaterialEditingLibrary?application_version=5.6).
Thirty material instances and 170 distinct parameter checks pass. All 52 staged
material/texture/function assets retain their hashes. No asset is saved by the
checker. The commandlet exits successfully; the sandbox's refused global editor
settings writes remain warnings, not writes to the user's configuration.

Evidence: `receipt.json`, `control-checks.json`, `material-readback.json`,
`material-readback.exit.json`, and `validation-receipt.json` under
`work/peony-customize1`. These checks establish slot ownership, control behavior
and saved parameter defaults. They do not establish final shader pixels,
cloth fit, game performance or gameplay acceptance. After the commandlet exited,
2,107,689 bytes of regenerable private caches were removed; the checked assets
and receipts remain pinned.
