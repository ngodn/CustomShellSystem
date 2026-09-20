# Native hand articulation

The missing articulation stage now runs in a saved native Control Rig:
`/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandArticulationV1`.
It contains 435 shipped nodes and passes 899 native execution cases. This is
an isolated stage, not the full hand graph or a deployed V44 repair.

## Contract

Input is the calibrated local pose. Ten distal finger joints are projected
onto the original Eve source X hinge. Four non-thumb proximal joints retain
independent curl and use experimental swing gain 0.125. The thumb base,
metacarpals, translations, scales and unrelated bones remain unchanged.
The gain is retained from the earlier study, not a final user-approved setting.

All 14 locals are snapshotted before evaluation. If any X-twist projection has
norm at most 0.0001, the entire stage passes through without partial bone writes.
`Enabled` defaults false. Disabled evaluation also passes through, and the stage
does not change curves. Each evaluation requires a fresh upstream pose.

`articulation.py` reduces the original full-matrix source-axis calculation to
local quaternion constants already independently derived for the corrective
drivers. It matches 446 saved articulated poses within 0.000951 degrees and
checks exact preservation outside the intended rotations. A constructed singular
input verifies whole-stage bypass. These 447 fixtures are each executed normally
and with quaternion antipodes, plus five lifecycle cases, totaling 899.

## Engine small-change behavior

The first native run exited 255: comparing the final hierarchy directly with
the calculated pose produced up to 0.01248 degrees difference. The worst measured
output exactly equaled its input. This was not a failed hinge calculation.

UE5.6.1 `URigHierarchy::SetTransform` returns early for a clean transform when
`FRigComputedTransform::Equals` succeeds. Its default quaternion-component
tolerance is 0.0001, including equivalent quaternion signs. The shipped
`RigUnit_SetTransform` does not request a forced write.

The corrected verifier separately reads `ArticulationOutputs` and measures the
actual hierarchy. Calculated quaternion error is below 0.000104 degrees. Applied
hierarchy error against the engine's documented-in-source write rule is also
below 0.000104 degrees. The math tolerance remains 0.001 degrees; it was not
relaxed to hide skipped writes. Across the 446 primary poses, 333 contain at
least one tiny skipped change, totaling 1,546 joint writes. Their effect on
actual skin is checked using measured output, not assumed harmless.

The second run exits zero, saves the asset, and preserves all 379 translations
and scales, unrelated rotations and 17 seeded curves. Disable/re-enable,
repeated fresh input, a new disabled/enabled instance, singular input and
quaternion antipodes pass. Protected canonical Skeleton and V43 mesh hashes are
unchanged. As with the earlier native stages, the graph uses the diagnostic B
hierarchy; full B2 import and its attachment-leaf differences remain a gate.

## Actual skin and visual check

`hand-native-articulation-skin-v1` inserts the measured native rotations into
all 446 pre-articulation fixtures, then recomputes finger/tip/web clearance and
corrective values offline. All 446 pass actual B2 skin checks with no new
intersections. Thirteen original controls and five H2 controls also pass,
for 464 total. Blender exits zero. The report links the measured native report
by SHA-256. This is native articulation plus offline clearance, not a complete
native pipeline; calibration is still supplied by saved inputs.

The running-attack frame 88, alpha 0 case had the largest original mismatch.
Both fitted hand views of its measured-output replay were inspected in
`native-articulation-v1-after-render-v1` under the SeduXtress audit directory.
They show separate curled fingers and a thumb alongside the index finger,
without an obvious crossed finger. The isolated neutral-material renders omit
weapon contact, full-arm handling and temporal behavior. Those remain required.

## Evidence and remaining work

Evidence is under `work/grip-grounding-v1/`:

- `hand-articulation-fixtures-v1`: 447 fixtures and full-matrix comparison.
- `hand-native-articulation-rig-v1`: rejected direct-hierarchy comparison and
  archived verifier. No asset was saved by this run.
- `hand-native-articulation-rig-v2`: successful native report, process exit,
  asset hash and protected-asset checks.
- `hand-native-articulation-skin-v1`: measured-output skin replay, native report
  hash and the rendering command/result.

Implement native directional clearance and combine calibration, articulation,
finger/tip clearance, web protection and curves. Verify complete fresh-input
transitions, cost, full B2 import/cook, weapons/sidearm and visible gameplay.
V43 remains installed. Original proportions, accepted body/hair settings,
disabled CSSX and the full modular Next-Gen scope remain preserved.
