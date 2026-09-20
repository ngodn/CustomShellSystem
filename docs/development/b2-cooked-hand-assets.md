# V44B2 hand asset import and cook

The full B2 mesh, an isolated diagnostic Skeleton and the complete native
hand graph now pass import, fresh readback, cooking and independent package
decoding. V43 remains the installed fallback. This candidate still has
placeholder materials and no physics or post-process binding. It is not a
release package and does not establish weapon grip or live performance.

## Assets and preservation

The source is `work/grip-grounding-v1/arm-rest-correctives-export-v1/candidate.mesh.json`,
SHA-256 `f49fd4e69a554f99bb2c309c78688192bd839285db178a9c83e013181b640d8a`.
It retains 133,066 points, 193,261 triangles, 379 authored bones, 30 material
slots, six public body morphs and 16 private hand correctives.

The three isolated packages are:

- `/Game/CSSAuthoring/DiagnosticReferences/SK_ArmRestV44B2Correctives_V1`
- `/Game/CSSAuthoring/DiagnosticReferences/SKEL_ArmRestV44B2Full_V1`
- `/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCombinedV1`

The diagnostic Skeleton is not the production shared Skeleton. It does not
add the game's virtual bones, sockets or animation bindings. No shared
Skeleton migration, V43 replacement, CSSX activation or game write occurred.
Saved editor assets and protected V43/shared Skeleton hashes are recorded and
unchanged. Original proportions and the accepted hair/body settings remain
requirements for subsequent integration.

## Fresh import readback

`arm-rest-b2-full-import-v1` records import exit zero and independent fresh
readback exit zero (`readback-v2.exit.json`). All bone names and parents,
22 morph names, 30 material slots and the isolated Skeleton assignment match.
Source translation error is zero and maximum orientation error is
0.000002415 degrees, below the existing 0.001-degree gate.

Two verifier failures are retained to prevent repeating them:

- Exact mesh/Skeleton quaternion equality was too strict. In UE 5.6.1,
  `USkeleton::CreateReferenceSkeletonFromMesh` calls
  `FReferenceSkeletonModifier::Add`, which calls `FReferenceSkeleton::Add`.
  The latter normalizes the copied rotation again. The measured maximum
  component difference is 4.440892098500626e-16 across 95 differing rows.
  The verifier permits at most four double-precision ULPs of 1.0, checks
  unit length, and still requires exact translation/scale equality.
- `GetMorphTargetsPtrConv` exists in C++ but is not exposed as the attempted
  Python method. The editor verifier uses the reflected `morph_targets`
  property. This is editor API access, not a live UE4SS raw-array read.

Relevant source is `Engine/Source/Runtime/Engine/Private/Animation/Skeleton.cpp`
(`CreateReferenceSkeletonFromMesh`) and `Engine/Source/Runtime/Engine/Public/ReferenceSkeleton.h`
(`Add`) in the retained UE 5.6.1 toolchain. The failed logs and diagnostic
mesh/Skeleton dumps remain alongside the passing readback.

## Cooked data

`arm-rest-b2-cook-v1` cooks only the three explicit packages for Windows and
exits zero. `arm-rest-b2-cooked-readback-v1` stages them under the game mount,
packs with retoc, verifies the container and decodes all three with CUE4Parse.
Every subprocess exit and tool/input hash is recorded. No deployment occurs.

The independent geometry check verifies all 193,261 oriented triangles and
their material assignments, zero position error, and exact float32 values
for all three UV channels across 579,783 triangle corners. The decoded mesh
has 557,396 render vertices, at most eight influences per vertex and maximum
weight-sum error 9.69e-8. This checks weight bounds, not exact source influence
distribution or animated contact.

All 379 decoded mesh and Skeleton transforms equal the saved editor bind at
the decoder's float32 representation. All 22 morphs have populated GPU
batches. Decoded deltas match the source within the cook's recorded precision
bound: the position grid is approximately 0.004 cm and the largest measured
delta error is 0.003422834 cm. The largest omitted source delta is
0.014997399 cm, consistent with the import's 0.015 cm component threshold.
Coincident points are matched by position and compatible delta. These bounds
do not prove that small quantization changes preserve every hand contact.

The cooked native Control Rig class retains 3,360 instructions, 62 function
entries, the Forwards Solve entry, 16 private curves and one diagnostic curve.
Its two serialized VM programs agree, function indices are valid, and the
Enabled and input-domain defaults remain false. This verifies serialized
structure, not live function resolution or cooked execution.

## Reproduction and next work

`verify_b2_import.py` loads the saved isolated mesh in a fresh editor process.
`read_b2_cook.py` requires a terminal successful explicit cook and creates a
fresh staging/decode folder. `verify_b2_cook.py` runs in Blender after the
existing `verify_nextgen_cooked_geometry.py --check-uv` passes. Each refuses
to overwrite its evidence. Command manifests retain the exact invocation.

Next verify cooked deformation and bind the complete hand sequence into an
isolated full post-process candidate with accepted hair/body dynamics. Choose
the raw-game or V43-compatible hand input convention from evaluated pose
evidence. Preserve game virtual bones, collision and weapon attachment
behavior when assigning a production Skeleton. Then check full-body weapon
poses, sidearm aiming, all weapon families, controls, lifecycle, performance
and visible gameplay before replacing V43. Modularity and the full Next-Gen
architecture remain in scope.
