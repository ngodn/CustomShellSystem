# Native thumb-tip clearance

The thumb-tip correction now executes in a saved Control Rig using shipped
RigVM nodes. All 934 native cases pass the unchanged 0.001-degree numerical
gate and bone/curve preservation checks. V43 remains installed. This is an
isolated V44B2 stage, not a deployed or fully integrated hand repair.

## Input and correction

The 464 inputs contain calibration and original-Eve articulation, followed
by the measured native single-step finger outputs. Their manifest hashes the
upstream native report, original fixture manifest and every pose. Do not use
pre-articulation inputs or quietly substitute offline finger outputs.

The solver examines the distal thumb segment against all three segments of
each other finger. It changes only `thumb_01_l` through a three-component
rotation vector. Directional hulls retain the fitted B2 geometry. The other
four fingers remain fixed during this solve; their posed geometry is cached
once per execution. Thumb geometry is recomputed for derivative samples.

The offline model's settings remain unchanged: 0.01 cm margin, 0.05-degree
central-difference interval, at most 12 iterations, at most 2 degrees per
step and 12 degrees total. The tested reference inputs need zero iterations
in 355 cases, one in 87, two in four and three in 18. These counts do not
bound cost on arbitrary future poses.

Non-unit hand scale bypasses the correction. Each evaluation snapshots fresh
input; no correction accumulates from a previous pose. Translations, scales,
other bones and an unrelated curve are checked for preservation. Disabling,
re-enabling, repeated fresh input, a fresh instance and quaternion antipodes
are covered. Unsupported crossed centerlines also invalidate the solver,
but the current fixtures do not independently exercise that condition.

## Native evidence

Saved asset:
`/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftThumbTipClearanceV2_I12_E0p05`.

`work/grip-grounding-v1/hand-native-thumb-clearance-v1/` contains the report,
terminal exit 0, command, authoring-source archive and saved-asset hash.
The asset has 1,058 shipped nodes. Maximum computed error is
0.000768294 degrees; maximum applied-hierarchy error is 0.000088760 degrees.
The latter accounts for Unreal's existing small-transform-write tolerance,
as documented in [native articulation](native-hand-articulation.md).

Editor execution across the changing fixtures measures 0.363 ms median and
0.973 ms p95. This includes the Python invocation bridge and excludes pose
setup. It is neither a paired benchmark nor live FPS or combined hand/body
cost. The full graph still needs cost and motion checks before deployment.

The generic graph author now supports both finger splay and thumb rotation.
`hand-native-finger-clearance-refactor-v1` repeats all 934 finger cases:
applied rotations, calculated errors, angles, iteration counts and validity
match the previous native report exactly. `refactor-equivalence.json` hashes
both reports. The refactored finger asset is saved separately as
`CR_CSS_LeftFingerClearanceV2_I1_E1p0`; the earlier V1 asset is preserved.
Shared Skeleton and V43 mesh hashes remain unchanged in both runs.

## Skin and visual evidence

`hand-native-thumb-clearance-skin-v1` inserts measured native thumb outputs
into all 464 fixtures, which already contain measured native finger outputs.
Only web protection and corrective curves remain offline in this replay.
All 464 actual-skin checks have no additional triangle crossings beyond the
four neutral seam pairs, and no web correction saturates. Blender exits zero.
The report hashes its native input report and records the numerical pass.

Both opposite views of `synthetic-running_attack-1-alpha-1.0.json`, the
largest numerical-error case, were reviewed in
`native-thumb-tip-clearance-after-render-v1` under the SeduXtress audit
directory. Fingers remain individually visible with curled tips; the thumb
lies beside the index. There is no obvious crossing in these views. Image
hashes and the bounded conclusion are recorded in `visual-review.json`.

## Remaining acceptance

Neither numerical agreement nor isolated hand renders establish weapon
contact, whole-arm posture, sidearm aiming or animation quality.

Next combine calibration, articulation, finger clearance, thumb-tip clearance,
web protection and corrective curves in that order. Then verify fresh-input
transitions and total cost, complete B2 import/cook, and check all requested
weapon/gameplay cases with the accepted hair/body dynamics and native UI
lifecycle. Original proportions, disabled CSSX and the full modular Next-Gen
objective remain preserved.

The graph follows Unreal's documented
[Forwards Solve](https://dev.epicgames.com/documentation/en-us/unreal-engine/control-rig-forwards-solve-and-backwards-solve-in-unreal-engine?application_version=5.6)
model. Exact node names and math come from the installed UE 5.6.1 RigVM
headers and actual execution, not assumptions from a newer documentation
version.
