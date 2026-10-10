# Peony wardrobe controls and material bindings

This is a validated wardrobe fragment, not an installed outfit or a complete
release catalog. Eve body/hair/physics controls, footwear-dependent placement,
final cooking and gameplay checks remain. The hip-accessory contact repair is
still open.

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
