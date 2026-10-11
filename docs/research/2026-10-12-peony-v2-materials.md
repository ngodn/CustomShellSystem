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
At that initial recipe stage, V2 masks and continuous rest-space coordinates
were still pending; the implementation below supersedes that state for upper
trim. The old garment's masks are not proof of correct V2 coverage. Keep glossy black
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

## First V2 flow implementation

`binding-flow1.blend` adds UV1 to all 18 retained garment parts using one shared
rest-space X/Z origin and scale. The input is still `binding-neck-donors1.blend`.
The saved-copy check protects 2,314 existing fields across the scene, including
positions, topology, material assignments, skin weights, original UVs, body
controls, CSS attributes and rig hierarchy/rest/pose. It passes after reloading.
Maximum coordinate reconstruction error is 0.0000051 cm. Geometry is unchanged.
The new blend SHA-256 is
`9ba5f98e2099faf1e8b0ba00ced1a4dfb04d8c3c1df807bf110b03d1f65afa7c`.

The upper source mask separates red fabric from blue trim and cyan details.
The narrow mask is `saturate(B - R - G)`, with coverage extending to
`saturate(B - R)` through `GlowFill`. All four upper palette recipes use the
red fabric channel. This pass covers slots 19 through 27 only; the lower mask
uses different regions and is not yet adapted. Gloves and heel emissions keep
their separate source material path.

`peony_v2_preview_materials.py` evaluates the shared pulse formula using UV1,
while every source texture explicitly uses UV0. The `review-flow2-off`,
`review-flow2-on` and `review-flow2-phase` views show the floral fabric excluded
from the upper trim emission and the pulse moving to a different position.
The cyan review color and strength 8 are diagnostic settings. Native defaults
are amber with strength zero, keeping flowing trim optional.

`review-flow2-baseline` renders the original fitting input without a flow graph.
Decoded front and side pixels exactly match glow off on the UV1 copy. The back
differs by at most one 8-bit channel level, with mean difference below 0.000016
per channel. `flow-off-comparison.json` records that the PNG files are not
byte-identical; `flow-off-pixels.json` records the actual pixel comparison.
Neither receipt is relabeled as exact equality. These comparisons cover the
Blender preview, not native engine shading.

Native assets are staged separately from the game at
`/mnt/eins0fxE/CSS-Development/peony-v2-materials1`, since the root filesystem is
nearly full. `author_peony_v2_materials.py` uses the existing graph/import helpers
and the shared `MF_TrimFlow`, with outfit assets under
`/Game/CSS/eins0fx/EveSB/PeonyV2`. It creates 109 packages, including 72 palette
instances and 36 upper-flow instances. `verified.json` passes 1,430 checks in
a fresh UE 5.6.1 editor, including saved parameters, texture identity/color
spaces, optional glow defaults and unchanged source/package hashes.

The authoring API has a known setter return-value issue in this engine range;
values are checked by readback rather than relying on that Boolean alone.
See [Epic UE-291403](https://issues.unrealengine.com/issue/UE-291403).

The workspace Docker daemon was stopped. After restarting it, the first cook
attempt could not see the larger drive because the helper binds its state over
`/mnt` inside its private namespace. The helper now preserves the original
drive mounts under `work/docker1/host-mnt` before that bind, and the cook runner
translates external staging paths into that namespace. No host drive mounts,
game packages or settings were changed. `windows-cook1` retains the failed
mount receipt; `windows-cook2` is the compilation attempt after correcting access.
That Windows UE 5.6.1 cook exits zero with zero errors and warnings, compiling
the new material graphs for SM5 and SM6. Its receipt confirms authoring sources
were unchanged. This establishes compilation, not native appearance or runtime
control binding. `cook-validation.json` inventories the cooked Peony packages.
The failed first attempt's duplicate staging project was removed after checking
its material assets against the retained editor inputs; logs and receipts remain.
This reclaimed 41,919,642 bytes. `flow-materials1-receipt.json` freezes 14 tool
recipes and 23 reports/previews, with tool copies in the native staging project's
`recipes` directory. The temporary workspace daemon was stopped after cooking.

Remaining: finish lower trim coverage, inspect native rendered materials,
preserve UV1 through mesh export/cook, adapt stockings and source fabric detail,
connect CSS controls and templates, then test profiles, modular parts and doubles.
No V2 game trial is installed.

## Lower trim and V2 garment export

`prepare_peony_v2_flow_masks.py` reuses the earlier Peony mask method on the
correct V2 structural mask. It selects green within ten pixels of the red
panel region, excluding that region plus a one-pixel margin. The red output
channel contains the full border and green contains its narrow edge detail.
The mask has 41,373 full-coverage pixels, 27,588 detail pixels and no overlap
with the protected region. This is pixel-level evidence; filtered/mipmapped
appearance remains a rendering check.

The same structural mask is used across recolors, whose source material masks
assign different colors to the same physical regions. `flow-lower1/palettes.json`
adds border emission to source slot 8 and flowing emission to the separate hip
band/strap meshes in slots 9 and 11. Original palette values and source textures
remain intact. Gloves, boots and body are excluded from these additions.
`review-lower-flow1/side.png` and `review-lower-flow1-pink/back.png` were inspected:
the side-panel border and hip details emit while the broad fabric panels do not.
The offline shader remains an approximation of the source material.

`/mnt/eins0fxE/CSS-Development/peony-v2-materials2` contains the expanded native
materials, selected through a hashed `material-input.json`. A fresh editor
passes 1,572 checks over 112 packages and 72 palette instances, including 48
flowing-trim instances. The Windows material cook succeeds with zero errors
and warnings. Strength still defaults to zero.

The existing garment exporter now has an explicit V2 mode. Its output in
`/mnt/eins0fxE/CSS-Development/peony-v2-garments1` contains all 18 retained parts,
44,777 used points, 67,762 triangles, 379 measured bind bones and six body
morphs. The independent saved-Blender comparison passes for topology/winding,
skin weights, body deltas, section ownership and UV channels. UV0 retains the
texture convention; UV1 retains the common rest-space flow convention without
a V flip. The spare third channel is zero. These are interchange checks, not
proof of cooked UV retention or final shared-skeleton compatibility.

The private Unreal import then retains 67,762 triangles, 18 sections and all
six morph targets. Its 44,828 render vertices include normal/UV splits. All 18
slots are bound to the black-and-white V2 instances. The garment and dependencies
pass Windows UE 5.6.1 cooking with zero errors and warnings, followed by an
IoStore roundtrip and import-resolution check for 114 packages. All five material
masters contain SM5 and SM6 shader code, including normal skinning and cloth
vertex-factory variants. Compiled cloth variants do not establish cloth simulation.

`cooked-surface-verified.json` compares decoded cooked triangles against the
verified interchange. Positions, UVs and winding match in every section;
all 18 material bindings, 379 bind bones and six morph names survive. Maximum
flow-coordinate reconstruction error is 0.0000127 cm. This closes the UV1
export/cook question for the private garment. Cooked morph deltas, posed
deformation, full-character/shared-skeleton assembly, native materials/stockings,
CSS controls and gameplay remain separate work. Nothing was installed.

## V2 character assembly

The garment's `cooked-deformation-verified.json` now compares decoded skin
weights and all six morph delta streams with the verified interchange. No new
bone assignments appear. Weight differences and discarded small influences
remain within the cook's 8-bit quantization bounds. Retained morph components
differ by at most 0.002008 cm; omitted deltas remain within the 0.015 cm build
threshold plus the measured floating-point tolerance. This verifies cooked
deformation data, not posed fit or cloth behavior.

`/mnt/eins0fxE/CSS-Development/peony-v2-character1` combines the accepted Eve
body and three hair/ponytail parts with all 18 V2 garment parts. Its interchange
contains 138,132 points, 183,191 triangles, 379 bind bones, 41 material slots and
22 body/hand morphs. `assembly-verified.json` compares geometry, winding, corner
attributes, skin weights and morph deltas with both source inputs. The assembly
changes only part selection, offsets and the planned boot-coverage material
partition. It does not refit the accepted body or garments.

The V2 coverage receipt supplies exactly 4,944 body triangles in three dedicated
skin aliases. Boots use source parts 4, 17 and 30, with the measured 9.738332 cm
placement offset. Hiding boots must restore those skin aliases and the bare-foot
offset; connecting and testing that CSS behavior remains pending.

The private UE import succeeds. The facial variant adds the accepted 115 Eve
expressions to the existing 22 morphs. Fresh editor readback verifies 406,473
expression deltas, with maximum retained error 0.000095176 cm, and preserves the
base mesh description and render sections. Both meshes are saved with the shared
Skeleton, Eve body Physics Asset and secondary-animation graph. The assembly
still has no cloth asset. Editor dependency stand-ins must not be shipped over
the accepted cooked Eve materials or animation graph.

Fresh binding reload passes for both meshes: 41 material bindings, the original
379 mesh bind bones and all nine rebuilt virtual bones (388 component bones).
The shared Skeleton and staged dependencies remain unchanged. Windows UE
5.6.1 cooking finishes with zero errors and warnings. The private 114-package
IoStore candidate resolves against the hash-pinned accepted Eve and shared
containers, rather than shipping the recooked editor stand-ins.

Cooked readback retains all 183,191 triangles, three UV channels, 41 material
bindings and the expected rig links. Both variants contain 140,616 render
vertices and identical base skin/topology/surface streams. Separate deformation
checks pass for the 22 body morphs and all 137 facial-variant morphs, including
skin-weight quantization and source-normal preservation. This also preserves
the garment flow coordinates within the assembled character.

Tangent-frame checks explicitly do not pass universally: 430 render vertices
use the engine's unit-X fallback where primary-UV triangles have zero area.
`tangent-fallback-locations.json` locates 284 in existing body/face/hair-attachment
sections and 146 in V2 underwear, jacket, corset and trim. Their visual impact
has not been established. Inspect them during native material review; do not
call the cooked-data pass a shading or outfit-fit acceptance.

`character-assembly1-receipt.json` freezes the evidence and 14 tool revisions,
including the exact earlier garment-verifier revision. No game files were
changed. The workspace Docker daemon was stopped after its cook completed.
Remaining Peony work includes source fabric detail and stockings, garment cloth,
native visual review, all supplied palette/component choices, CSS controls and
saved profiles, followed by the required preview/gameplay checks.
