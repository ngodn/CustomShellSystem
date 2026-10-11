# Peony V2 source materials and palette recipes

The private fitting input remains `binding-neck-donors1.blend`, SHA-256
`433d066c78e762fc7ed638bc39bf251f59c04c5741880aa443b4a571c8b0c610`.
No geometry, game package, saved selection or gameplay input changed here.
Work files are under
`CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/work/peony-v2-source1`.

## Minor clipping and current review

The user explicitly permits minor clipping that is unobtrusive in normal play.
The four whole-outfit sprint views in `review-neck-donors1-whole-sprint` and the
textured views in `review-floral2-sprint` supersede using the magnified underarm
crop as the immediate work priority. Fine underarm fitting is deferred. These
offline frames do not establish acceptance of the full animation or remaining
hip attachments. Keep conspicuous loose clothing, deformation and functional
defects in scope.

## Inherited source settings

The earlier exporter used `EMaterialDepth.TopLayerOnly`. It omitted inherited
material parameters even when retail parents were mounted. The existing
`SBMeshExport` now accepts `SBMESH_MATERIAL_DEPTH=inherited`, selecting
`AllLayersNoRef`; its unchanged default remains `top`. The .NET 10.0.401 build
completed with zero warnings and errors. This uses the local CUE4Parse
`ExportOptions.MaterialDepth` and `UMaterialInstanceConstant.GetParams` path.

`export_peony_v2_palette_sources.py` constructs separate retail-plus-mod mounts
for CNS, Hydrangea and Peony. Recolors can contain different pixels at identical
Unreal paths, so their files are never combined into one mount or resolved by
basename alone. The final `palette-resolved3` receipt records 115 CNS files,
57 Hydrangea files and 62 Peony files. It requests every CNS texture option,
including textures not selected by the original mesh's default material.

Hydrangea and CNS have no missing-import warnings in this export. Peony's
`MI_CH_P_EVE_33_Lower_2_Pink` contains a missing ORM import
`0x1D320B8AF30936DF`. Its exported effective ORM falls back to the inherited
`CH_P_EVE_33_Lower_2_ORM`. This is recorded, not silently declared equivalent
to the source game's runtime behavior. The requested black-and-white default
does not depend on this unresolved Peony override.

## Palettes and pattern identity

`palettes2.json` maps all 18 retained garment parts and preserves the complete
inherited texture, scalar and vector data. It includes four recipes:

- Black and white floral, the requested default.
- V2 CNS source default, including its configured component overrides.
- Hydrangea source recolor.
- Peony white and pink source recolor.

The seven arm colors remain independent component options, alongside 23 retained
texture selectors and their supplied pattern choices. They are not invented
whole-outfit schemes. Removed pocket/mechanical/nail selectors are excluded.
Body stocking controls are retained separately for adaptation to the accepted
body; none of the source skin replaces that body.

Visual inspection found that `AiK_FlowerA` contains skulls, snakes and roses.
The name alone did not identify the floral reference. The default recipe uses
`FlowerDecalPattern_A` on its fabric sections while retaining the Hydrangea
black-and-white base textures. The archive's actual pattern remains available
in its separate source recolor and the supplied component pattern choices.

`palettes2-validation.json` checks coverage of all 18 bound garment parts,
retention of seven arm options, 179 explicit scalar/vector overrides against
the raw exports, 114 material/texture hashes and the unchanged fitting blend.
These checks pass. They do not prove native shader parity or CSS installation.

## Textured previews

The binding reviewer now optionally preserves source UVs and uses palette
recipes through `peony_v2_preview_materials.py`. This is a Principled shader
approximation using source base maps, masks, normals, ORM, colors and pattern
scale. It is not a reconstruction of every layer in the original UE shader.
The source mask's fabric region replaces the underlying base there; the first
preview multiplied by a black lower base and lost the white side panels.
`review-floral1-neutral` is rejected for that preview error.

Reviewed output:

- `review-floral2-neutral`: front, back and side. White floral side panels and
  black edging are present; the accepted body and source mesh input are intact.
- `review-pink1-neutral`: front, back and side renders completed; side reviewed
  for the supplied white/pink material distribution.
- `review-floral2-sprint`: four views with all six controls at maximum; back
  and negative-X side reviewed at whole-outfit scale.

Skin remains a diagnostic material, with no final stocking, face or hair
material evaluation. Lighting and fabric response are not game-match evidence.
No V2 trial is installed.

## Flowing trim and next implementation

The user's October 12 reminder keeps optional glowing, flowing black edging
mandatory. `palettes2.json` records the shared `MF_TrimFlow` function and
`GlowColor`, `GlowStrength`, `GlowFill`, `FlowSpeed`, `FlowPhase` controls.
V2-specific masks and continuous rest-space coordinates still need authoring;
the old garment's masks are not proof of correct V2 coverage. Keep glossy black
trim with glow off and exclude floral fabric, skin and stockings from the
flowing trim mask. Verify this across palette selections and removable parts.

Next implement the V2 UE materials, stocking adaptation and flow masks using
these resolved sources, then validate the export, customization and motion.
Do not resume microscopic underarm clearance experiments before that work.

`palette-export-cleanup1.json` records removal of 75 duplicate PNGs from this
turn's superseded exports after checking identical retained hashes, reclaiming
166,422,634 logical bytes. Raw JSON, logs, receipts and current inherited
exports remain. No source archive or released artifact was removed.

`palette-materials1-receipt.json` freezes 30 recipe, report and render files,
the exporter DLL identity and the unchanged fitting input. `palettes2.json`
adds the glowing-trim requirement to the recipe; its palette values and source
controls are identical to `palettes1.json` used for the recorded renders.
