# Eve expression candidates

2026-10-06. Authoring candidates only. No game files were replaced.

`export_facial_shapes.py` extracts the 99 facial shapes needed by the 21 supplied
ATOOL photo expressions from `CSS_SeduXtress_HeelSupportsV45C.blend`. It uses the
existing exporter's fitting and vertex filtering, then checks the body point
order against the accepted heel-support export. Body positions match exactly.
Existing body and hand deltas differ by at most 0.000078 cm where the older
sparse export omitted tiny values. All original input hashes remain unchanged.
The two jaw curves are matched case-insensitively; exported names preserve the
source animation curve spelling. Authored negative weights are retained.

`CSSFacialLibrary` adds the shapes to detached mesh descriptions, validates all
input before modifying a candidate, then lets UE 5.6.1 build the morph data.
It accepts only commandlet-owned assets under `/Game/CSS/AnimLab/SK_Face_`.
It does not modify the general mesh importer's 64-shape limit or original assets.
Its source copies live beside the other probes and in the existing CSSAuthoring
module. The UE module uses its pinned C++20 configuration.

Six candidates are saved:

| Original | Candidate suffix | Morphs |
| --- | --- | --- |
| SK_BlackPearl2 | BlackPearl2 | 22 retained + 99 facial |
| SK_PFit14 | PFit14 | 22 retained + 99 facial |
| SK_SFit16 | SFit16 | 22 retained + 99 facial |
| SK_BFit1 | BFit1 | 22 retained + 99 facial |
| SK_KCloth4 | KCloth4 | 22 retained + 99 facial |
| SK_ACloth3 | ACloth3 | 22 retained + 99 facial |

Fresh-process readback checks all six. Base vertex positions and existing morph
attributes match exactly; retained built facial deltas match the interchange
within 0.000096 cm. The expanded check also covers omitted vertices: Unreal's
unchanged `MorphThresholdPosition` of 0.015 cm filters small deltas. The largest
omission measured on each candidate was 0.015002 cm, within that threshold
plus float reconstruction error. Do not describe the retained-delta error as
the error across all vertices. Material slots, bone bindings, skeletons, physics
assets and post-process graphs remain equal to each original. Verification hashes both
source and candidate files before/after and saves nothing.

Evidence under `work/cine-animation-port/`:

- `facial-export-v1/report.json`: immutable inputs, mapping and export checks.
- `facial-export-v1/facial-shapes.json`: shapes and signed expression weights.
- `facial-export-v1/ue-verification.json`: six saved meshes and readback checks.
- `facial-review-v1/`: 22 Blender head renders, including Reset and all 21 poses.
- `facial-authoring-build.log`, `facial-verify-build.log`: successful UE builds.
- `facial-verify-v3.log`: expanded coverage, including filtered vertices and
  rejection of original assets and duplicate expression additions.

The initial all-six import process was terminated after saving five candidates;
the sixth completed in a separate process. The subsequent fresh-process
verification succeeded for all six. Import progress is now persisted per mesh.

All 22 frontal renders were inspected, including Reset and all 21 expressions.
The poses show distinct brow, cheek, lip and eyelid changes without obvious
mesh tearing in these views. Tease-ya and Zzz close both lids; Wink and Kiss
close opposite lids. Smile and Happy remain distinct, with teeth showing in
Happy. These observations are a solid-material geometry review, not game
lighting or cooked playback acceptance. Side-view teeth/eyelid contacts,
cooked data, runtime ownership/restoration, CINE controls and performance
remain to check.
No expression menu or released outfit update is installed yet.

## Shared facial cook and catalog, 2026-10-06

Six copies now live under `/Game/CSS/Locomotion/Faces/SK_Face_*` in the
authoring project. `stage_facial_library.py` copied only the previously verified
candidates. A fresh process verified geometry, morphs and bindings before the
Windows cook. The original outfit assets and installed containers remain
unchanged. `run_facial_library.py` uses the existing restricted commandlet runner;
the readonly verifier accepts the shared namespace, while the mutation helper
still accepts only the isolated AnimLab namespace.

The cook contains about 311 MiB of skeletal-mesh payloads. The private
`CSS_FacialReadbackProbe_P` container is under the ignored workspace and is not
mounted in the game. Do not distribute or install this diagnostic container.

`tools/FacialReadback` independently decodes the cooked GPU morph buffers using
the pinned CUE4Parse checkout and .NET 10.0.401. All six comparisons passed:

- Reference skeleton, materials, skeleton/physics/post-process bindings and
  sockets match the installed Eve package.
- Render positions, normals, UVs, skin weights, indices, sections and bone lists
  match. Cloth index mappings match; the reader does not decode every cloth buffer.
- All 22 existing body/hand morph delta arrays match exactly after decoding.
- All 99 added facial morphs have nonempty, finite GPU deltas with unique valid
  render-vertex indices. Each mesh has 121 morphs in total.

The cooked position precision is approximately 0.004 cm. This value is not a
measured source-to-cooked error bound. The added facial delta validity check does
not independently compare every delta with the Blender interchange. Editor
source comparisons and cooked GPU checks cover different parts of the pipeline.

Evidence: `work/cine-animation-port/shared-faces-v1/` contains the create/verify
reports, successful cook receipt, private container receipt,
`cooked-readback-v1.json`, and `facial-catalog-evidence.json`.
The catalog generator produces `facial-rig.json`: 21 expressions, 99 morph names,
and six adapters tied to the exact installed Eve container revision.

CSS's animation-library parser now accepts an optional `faces` list. Each rig
declares stable expression IDs, signed weights and exact source-to-derived mesh
bindings. Parsing rejects ambiguous FName casing, non-finite or out-of-range
weights, unknown shapes and conflicting adapters. Library conflicts are rejected
regardless of discovery order. Resolving a different outfit/revision or removing
the pack produces no facial adapter. Old catalogs do not need a `faces` field.

The weight builder clears the rig's other facial shapes on expression changes;
it never adds body or hand controls to that facial list. Reset remains a separate
runtime ownership operation, not the source Neutral expression.

Validation: 22 CSS host suites pass; the real facial catalog passes 40 parser,
compatibility and discovery checks. The decoder builds with zero warnings.
Runtime mesh activation, saved expression selection, CSS/CINE controls and
ownership/restoration are still unfinished. The installed performance trial has
not been replaced by this offline work.

The next data-layer step is implemented: optional `Customization.expression`
stores a stable expression ID in ordinary settings and profile snapshots. Empty
is the original face and is omitted from serialized old/default settings.
Palette changes preserve expression selection. A missing library preserves the
saved identity while `Catalog::facial_match` returns no adapter; an unrecognized
variant cannot borrow another variant's facial mesh. The selection/profile
regressions bring the real-catalog facial checks to 56, with all 22 host suites
still passing. This stores intent only; runtime activation and UI remain next.
The Windows CSS core also builds successfully. Its existing unused `row` warning
in `extension_data.cpp` remains; the expression changes introduced no new warning.
This binary is not installed. The last process check still found the game closed.

Source Neutral has seven nonzero weights. Keep it distinct from Reset, which
must restore the preceding face state while leaving body/hand morphs alone.

The implementation follows the pinned local UE 5.6.1
`SkeletalMeshAttributes.h` morph attributes and mesh-description build path.
See also Epic's [morph target API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UMorphTarget?application_version=5.6)
and Blender's [shape-key reference](https://docs.blender.org/api/4.5/bpy.types.ShapeKey.html).
