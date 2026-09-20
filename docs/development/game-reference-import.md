# Exact game-reference evaluation, 2026-09-20

The MoreBeaute comparison needs the game's actual human Skeleton reference.
The older editor fixture was built from a 258-bone mesh subset; it cannot stand
in for the game's 1199-bone Skeleton when testing compatible-Skeleton rotation
remapping. Production CSS still has 379 authored bones and nine virtual bones.

## Authoring tool changes

The game reference contains six mirrored rope/string bones with negative local
scales. `CSSImportMesh` previously rejected them. The new explicit
`-AllowMirroredReferenceScale` option accepts finite nonsingular signed scales
only when both asset paths are inside `/Game/CSSAuthoring/DiagnosticReferences/`.
Normal outfit imports retain their existing positive-scale requirement.

`CSSRetargetLibrary.BindDiagnosticMeshSkeleton` binds an isolated diagnostic
mesh to an isolated diagnostic Skeleton using the engine setter. It does not
merge bones or save assets. Both arguments must be in the diagnostic namespace.
The Python property and setter are unavailable in this engine's Python API;
failed attempts are retained separately from the passing run.

The authoring source lives outside this Git repository. Its exact changes are
versioned in [diagnostic-game-reference.patch](../../tools/authoring-patches/diagnostic-game-reference.patch).
Paths in the patch are relative to the msII workspace. The patch is already
applied; `git apply --reverse --check` passes against the current files.
Do not apply it again without checking the current source.

Epic documents signed-scale handling in [TTransform](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/TTransform?application_version=5.6).
The pinned local 5.6.1 `TransformNonVectorized.h` also implements that handling.
This reference import is not a claim that CSS body/hair dynamics supports
negative owner scale.

## Verification

Both editor builds pass. The full reference imports 1199 bones, including all
six negative scales. A fresh editor read matches all reference translations
and scales exactly; maximum quaternion difference is 5.55e-8. All original
translation modes and nine virtual bones are restored and read back.

Three actual importer rejection checks pass, with no output asset created:
negative scale without the option, zero scale with the option, and the option
used outside the diagnostic namespace. The Original MoreBeaute 258-bone
reference also imports through the unchanged normal path.

A frozen MoreBeaute Martyr's Blade/H2 pose is then evaluated with Unreal's raw
and compressed paths. The control reproduces ten arm/wrist/prop transforms
within 0.001 cm and 0.001 radians. The worst compressed arm position difference
is about 0.000425 cm. Other common-bone rotations agree within 0.000002 radians;
thigh translations differ by up to 0.240 cm because this animation-only control
does not reproduce all final graph effects. Do not call it whole-pose identity.

The same source input transfers to V43 through the actual compatible-Skeleton
code. Both evaluations output 388 target bones. Production V43 mesh and shared
Skeleton file hashes are unchanged. This test does not rerun the game IK on
Eve, run the full gameplay graph, or establish a release-ready repair.

## Pose findings

Reviewed V43 renders from the transferred pose show the off-hand separated
from the weapon. Median distal-finger distances on the left are 20.64-24.71 cm.
A separate counterfactual reverses only compatible-Skeleton rotation remapping,
keeping every target translation and scale unchanged. In that case the weapon
orientation improves and those medians decrease to 8.85-11.39 cm. The hand still
does not reach the grip. Right-hand grip and all weapon/pose states still need
validation; these nearest-surface distances do not establish penetration or
contact quality.

This isolates a contribution from the different Skeleton reference orientations
in the frozen transfer. Next test an isolated Skeleton-reference correction in
the engine and check the remaining arm reach/IK behavior. Do not deploy a
blanket inverse-rotation pass based on this single case. Preserve original Eve
geometry and proportions, accepted V43 grip, body motion, hair 200/24, all bones,
and disabled CSSX. A matching live V43 H2 capture remains requested.

## Evidence

Game-work root: `work/grip-grounding-v1/game-human-reference-editor-v1/`.

- `evaluation.json`, `evaluate-r3.py`, `evaluation-r3.command.json` and logs:
  passing raw/compressed control and target test, with unchanged production hashes.
- `GameHuman1199.mesh.json`, `MoreBeaute258.mesh.json`, successful import logs:
  isolated reference inputs and imports.
- `guard-results.json`: three expected rejections from the actual importer.
- `source-changes.json`: authoring source hashes before and after this change.
- Earlier failed runs are retained. Initial full import rejected negative scale;
  initial control import had no input file after the first loop aborted;
  evaluation v1/v2 stopped on unavailable Python setters. None is a passing run.

SeduXtress authoring `work/nextgen-audit/`:

- `v43-frozen-morebeaute-h2-v1/`: reviewed two-view target replay and distances.
- `full-pose-remap-identity-v1/`: counterfactual input and per-bone rotation changes.
- `full-pose-remap-identity-render-v1/`: reviewed view B and distances.

`tools/render_live_weapon_reference.py` now records the source fixture's scope,
so editor-generated poses remain distinguishable from actual live snapshots.
