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
attributes match exactly; built facial deltas match the interchange within
0.000096 cm. Material slots, bone bindings, skeletons, physics assets and
post-process graphs remain equal to each original. Verification hashes both
source and candidate files before/after and saves nothing.

Evidence under `work/cine-animation-port/`:

- `facial-export-v1/report.json`: immutable inputs, mapping and export checks.
- `facial-export-v1/facial-shapes.json`: shapes and signed expression weights.
- `facial-export-v1/ue-verification.json`: six saved meshes and readback checks.
- `facial-review-v1/`: 22 Blender head renders, including Reset and all 21 poses.
- `facial-authoring-build.log`, `facial-verify-build.log`: successful UE builds.

The initial all-six import process was terminated after saving five candidates;
the sixth completed in a separate process. The subsequent fresh-process
verification succeeded for all six. Import progress is now persisted per mesh.

Reset, Angry, Kiss and Wink renders were inspected and show distinct expected
deformations. The render is a solid-material geometry review, not game lighting
or cooked playback acceptance. Other poses, teeth/eyelid contacts, cooked data,
runtime ownership/restoration, CINE controls and performance remain to check.
No expression menu or released outfit update is installed yet.

Source Neutral has seven nonzero weights. Keep it distinct from Reset, which
must restore the preceding face state while leaving body/hand morphs alone.

The implementation follows the pinned local UE 5.6.1
`SkeletalMeshAttributes.h` morph attributes and mesh-description build path.
See also Epic's [morph target API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UMorphTarget?application_version=5.6)
and Blender's [shape-key reference](https://docs.blender.org/api/4.5/bpy.types.ShapeKey.html).
