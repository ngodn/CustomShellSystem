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

## Stocking study recovered after the disk-full interruption

The 04:52 render completed before the session stopped. The retained study is
`/mnt/eins0fxE/CSS-Development/peony-v2-stockings1`: `stockings.blend`, its receipt,
and neutral front/back/side renders are present. A fresh Blender read-only check
compares the source and recovered blend: the accepted body and all 18 bound
garments retain identical geometry, topology, skin weights, morphs and UVs.
Four additional stocking layers retain the corresponding body triangles,
weights and morph deltas with the intended 0.2 mm rest-surface offset.
`recovery-verified.json` records this check; all three rendered image hashes
match their original report.

`recovery-receipt.json` and six frozen scripts in `recipes` preserve the state
that was not recorded in the interrupted conversation. The old render report
omitted the stocking flag and helper hash, so the recovery receipt records the
known launch options separately without rewriting that earlier evidence.
`source-regions-aligned.json` is the useful source-region measurement; the
earlier `source-regions.json` used raw Basis positions and is superseded.

These neutral views still use diagnostic skin and an approximate stocking
shader. The sheer stocking appearance is not yet established by these images.
Native material integration, posed fit and gameplay remain pending; no V2
trial is installed. The glowing, flowing trim remains required.

The CSS workspace relocation, disposable-cache removal list and disk readings
are recorded in [the October 12 cleanup log](../development/cleanup-2026-10-12.md).
Referenced backups, original archives, accepted body inputs, current V2 source
and cook/readback evidence were retained.

## Stocking defaults, posed correspondence and native material

The recovered blend had nonzero default values on the new stocking shape keys.
`stockings-ready.blend` resets those defaults to zero. The body, 18 garments and
four stocking layers retain their original vertices, topology, UVs, weights and
morph deltas. The preparation recipe now initializes these values explicitly.
Earlier review renders already reset shape values before applying their chosen
settings, so this finding does not establish a cause for their appearance.

`fit-verified.json` passes 11 cases: neutral, six individual maximum body controls,
all controls at half and maximum, and retained walking/sprinting poses at maximum.
Maximum stocking-to-corresponding-body distance is 0.200177 mm, consistent with
the intended 0.2 mm offset. This checks the stocking layer, not every possible
garment collision or animation frame.

Opacity-zero and opacity-one comparisons confirm that the layer renders. The
initial 0.45 opacity was too faint in the diagnostic preview. The new material
uses 0.7 opacity, source stocking tint and weave textures, tiling 10, metallic
0.25, roughness 0.42 and normal strength 0.3. Legs default on; body, arms and hands
remain separate optional regions. This translucent overlay preserves the accepted
Eve skin beneath it. It is an adaptation, not an exact reconstruction of the
source game's opaque layered skin shader.

Seven native packages, comprising two textures, a parent and four palette
instances, pass fresh editor reload. Windows cooking completes with zero errors
and warnings, preserving source assets. `cooked-readback1/verified.json` verifies
the IoStore payloads and 23 SM6 plus 17 SM5 shader entries, including required
skin/cloth vertex factories. Nothing was installed.

The parent uses per-pixel surface translucency before depth of field, keeps depth
testing enabled and writes translucent velocity. Settings follow Epic's UE 5.6
[Material API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/Material?application_version=5.6),
[lighting mode API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TranslucencyLightingMode?application_version=5.6)
and [translucency pass API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MaterialTranslucencyPass?application_version=5.6).
Runtime sorting, performance and visual behavior still need native testing.

The new neutral and maximum-shape walking views completed in
`review-sheer-neutral` and `review-sheer-walk-max`. These use diagnostic skin and
an approximate shader. Visible faceting and these previews do not establish
native material quality. Character integration, cloth, CSS controls, saved
profiles and gameplay checks remain open, alongside the glowing, flowing trim.
`stocking-material1-receipt.json` freezes this work's recipes and evidence.
The completed cook's temporary prefix and redundant intermediates were removed
after recording the removal list in the October 12 cleanup log.

## Stockings integrated into the native character

`/mnt/eins0fxE/CSS-Development/peony-v2-character2` appends the four stocking
regions to the verified first V2 character. The original body, hair and garment
vertices, faces, corner attributes, skin weights and morph deltas remain unchanged.
The export adds 10,397 points and 20,274 triangles. Five material slots cover
legs, boot-covered legs, body, arms and hands. The 4,944 boot-covered stocking
triangles match the existing body coverage exactly. Leg stockings should default
on, other regions off; the boot-covered section requires both stockings enabled
and boots hidden. These rules still need CSS control integration.

`assembly-verified.json` independently checks appended weights and morphs against
the accepted body, source UVs, winding, region coverage and byte-quantized alpha.
Its rest offset remains 0.02 cm within float conversion tolerance. Original
character streams are exact prefixes, so the stocking operation does not refit
or deform the accepted body or the fitted garments.

Native import and expression addition pass. Fresh reload verifies the 46 material
bindings, 379 mesh bind bones, nine virtual bones, shared Skeleton, Eve Physics
Asset and secondary animation graph. The facial variant retains the accepted
115 expressions alongside 22 body/hand morphs. Source dependencies remain pinned.
Only required editor dependencies were staged, not an entire previous project.

Windows cooking completes with zero errors and warnings and unchanged sources.
The 121-package IoStore candidate passes exact payload roundtrip, dependency
resolution and all six material-parent shader checks. It continues to depend on
the accepted cooked Eve/shared packs, excluding recompiled editor stand-ins.
Both variants read back as 151,979 render vertices, 203,465 triangles, three UV
channels and 46 material bindings. Their base streams match. Separate body and
facial checks pass all 22/137 morphs, skin-weight quantization, source normals and
vertex colors, including stocking coverage alpha.

The existing 430 zero-primary-UV-area tangent fallbacks remain. Their count did
not increase, but their visual impact is still unresolved. Neither the cook nor
the data checks establish material appearance, sorting, cloth behavior, motion
quality or gameplay acceptance. No new trial is installed. Native visual review,
CSS wardrobe/palette/glow controls and profiles, cloth and the broader regression
matrix remain. The glowing/flowing fabric, panel and strap parents are present
in this combined cook.

`stocking-integration-receipt.json` preserves commands, recipes and evidence.
Temporary Wine state, shader intermediates, duplicate staging inputs and unbound
mesh copies are listed before removal in the October 12 cleanup record.


## CSS material palette support

The user authorized CSS core improvements for the new outfits. The palette model
now accepts optional global base-material parameters: scalar, raw linear vector
and texture. Existing palettes retain their control-value format. Each palette
is limited to 512 unique slot/parameter destinations. Editable controls must have
explicit palette values when their bindings overlap the authored parameters.
The full format is in `docs/control-convention.md`.

The runtime applies the material settings once after rebuilding owned MIDs, then
applies the control overrides. A palette identity change invalidates the material
cache even when its control values match the previous palette. Dropping an
explicit control restores the selected palette baseline. Original restores the
material parents. The cache now retains its transition fingerprint when no
customization values changed. Material-only palettes can supply a scalar/vector
fingerprint; the existing one-parameter check is not an exhaustive material audit.

`tools/eve-fit/build_peony_v2_palettes.py` maps the verified source recipes to the
18 garment slots in `peony-v2-character2/binding-plan.json`. All four source
palettes have 225 settings. `check_peony_v2_palettes.py` independently compares
them with decoded cooked material instances, including global parameter addresses
and packaged texture types. All 900 settings match within float serialization
tolerance, and all 34 referenced textures exist in the cooked readback. No body,
stocking, geometry or material-parent changes were needed for this step.

Evidence: `work/three-outfit-rig-physics/peony-v2-customize1`, containing the
palette fragment, source hashes, cooked comparison, test logs and private core
DLL. DLL SHA-256:
`9bdba0323924018ef87dc2b176ccca803ce92a4678e01b8cb9bb755807194ec3`.
The C++23 Windows build passed with one existing unused-variable warning in
`extension_data.cpp`. Portable controls, profiles and data tests passed, as did
20 Python control tests and four package tests. Package tests install only into
temporary fixtures. Root and theark free space remained 129G and 90G respectively
at `df -h` precision. Existing build directories were reused.

No installation or runtime acceptance occurred. The fragment intentionally has
no editable controls yet. Complete its visibility, body, pattern, stocking and
optional glowing/flowing trim controls before packaging; fill overlapping palette
control values, preserve black-and-white floral as the authored default, then
check palette changes, manual overrides, reset, profile reload, preview/world
rendering, transitions and supported doubles in the game.


## Complete V2 customization metadata

`build_peony_v2_customize.py` now produces `customize.json`, `variant.json` and
`texture-dependencies.json` under `work/three-outfit-rig-physics/peony-v2-customize1`.
The 78 controls retain the accepted skin-suit body, hair, dye and physics settings;
remap body dye surfaces through the assembled source-slot map; and add 13 garment
and sheer-layer switches, color controls, the source finish/pattern selectors,
stocking opacity and optional flowing trim. The recipe validates and passes the
control convention lint. All 13 referenced dye PNGs match the donor manifest.
The variant retains the measured boot height of 9.738332312554121 cm, with zero
lift when boots are hidden. This is metadata, not an installed catalog.

The garment switch groups cover all 18 source parts. The boot switch also hides
skin aliases 38/39/40 and stocking section 42. Stockings own sections 41/42;
optional body, arm and hand sheer layers own 43, 44 and 45 independently. The
first generator pass mistakenly treated a null optional boot slot as present
on those extra layers. The independent ownership check caught this before any
installation; the corrected output passes the exhaustive native test.

Source colors that differ between parts remain independently adjustable.
The checker confirms 122 bindings and 610 values across Original plus four
palettes against the decoded cooked materials. Color defaults convert from
source linear values to CSS sRGB, then back for the comparison. SourceGlow's
unused alpha differs between source palettes; the opaque graph consumes RGB
only, so its editable color keeps a fixed protected alpha while preserving HDR
RGB. Other color vectors retain their authored alpha. Palette-invariant glow
strength, coverage and flow speed are omitted from the raw palette settings;
the original materials already supply their defaults, and manual choices now
survive palette selection. Stocking opacity also survives palette selection.

The source selectors expose 88 named texture choices across their controls.
Seven distinct finish textures are not yet native assets. Their source paths,
content hashes and sampler types are recorded in `texture-dependencies.json`.
All seven source hashes pass verification. Import and cook these before building
an installable package; an unresolved texture path is not accepted as working.
Body tattoo selectors are not copied onto the accepted Eve body texture layout.

CSS's control count and saved-control-value limit increased from 64 to 128.
The capacity tests exercise 128 controls and saved values and reject overflow.
The Windows C++23 build and portable controls/profiles/data tests pass, alongside
20 Python control tests. `check_peony_v2_customize.cpp` passes all 8,192 garment
combinations, 32,768 palette/visibility combinations, texture selection/reset,
Original restoration and saved-profile preservation of shape, opacity, flowing
glow, placement and expression. These checks do not prove runtime material
switching, visibility propagation to doubles, shader appearance or cloth motion.

`controls-core-verification.json` records the updated build at
`build/cine-win/css_core.dll`, SHA-256
`f2f0c72c6da85babb691c871347da7adb2a7772c3672a48328c8eeceda9cbe89`.
The earlier `peony-v2-customize1/css_core.dll` is the retained palette-only build,
not this updated candidate. No game files changed. Existing build directories
were reused; no full project copy or cook was made. `df -h` remains 129G free on
root, 90G on theark and 50G on the development drive.


## Source finish textures cooked

The seven missing finish textures now have native assets in
`/mnt/eins0fxE/CSS-Development/peony-v2-options1`. The immutable selection points
to the earlier dependency plan; its `pending_import` entries describe the input
set, not the current completion state. `completion-receipt.json` records the
current evidence and frozen recipes. The existing materials and character cooks
were not rebuilt or copied.

`author_peony_v2_options.py` imports the exact hashed source PNGs using the same
sRGB/default compression settings as the V2 fabric textures. A fresh editor
process verifies all seven saved Texture2D assets. Windows UE 5.6.1 cooking
passes with zero errors and warnings. The private seven-texture IoStore trio
passes verification, dependency/import resolution and payload roundtrip checks.
`check_peony_v2_option_cook.py` confirms source dimensions, top mip retention,
complete positive-sized mip chains and sRGB settings. These are asset checks,
not a claim of visual equivalence after block compression.

The existing cook/readback helper now accepts the isolated `peony-v2-options1`
workspace and requires exactly seven texture packages with no material masters.
Its frozen copy is under this workspace's `recipes`; the earlier character and
stocking checks are unchanged. Linux editor logs include the existing read-only
preset-preference warning at shutdown; both editor processes exit successfully.
Windows cook diagnostics are clean.

`check_peony_v2_control_materials.py --require-complete --additional-decoded ...`
merges the character and option readbacks in memory, rejects package collisions,
and requires every declared choice to resolve to a Texture2D. All 88 texture
choices and 122 bindings pass; 610 Original/palette values match and the unresolved
texture count is zero. Omitting the options readback produces the expected hard
failure. See `peony-v2-customize1/control-materials-complete.json` and
`peony-v2-options1/missing-texture-rejected.json`. Body and garment meshes,
materials, fitting, glow settings and the 78-control recipe did not change.

Five recorded temporary groups were removed after verification, reclaiming
1.35 GiB allocated. Original editor assets, cooked CSS payloads, the container
trio, useful DDC and all evidence remain. Their hashes still match. The private
Docker daemon exited after confirming no containers remained. Free space before
and after this turn stays 129G on root, 90G on theark and 50G on the development
drive at `df -h` precision. The complete outfit still needs final package/catalog
assembly and in-game visual and behavior checks; nothing was installed.
