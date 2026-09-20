# Arm twist binding experiment, 2026-09-21

V44A is an isolated, rejected experiment. V43 remains the installed baseline.
Restoring separate twist weights and fitting their pivots, while retaining the
existing bind orientations, curves the forearms unnaturally. It does not repair
the left wrist or fingers. Do not cook or deploy this blend as a finished V44.

## What is established

The original Eve rig separates bending from twisting. A read-only Blender test
sets FK mode, disables stretch and applies plus/minus 60 degrees of local Y
rotation independently to the upper-arm, forearm and hand controls. Forearm
twist rotates the distal deform bone by 60 degrees while its bend bone remains
unchanged. Hand twist affects the hand alone with the source's forearm-follow
option disabled. The original saved rig uses IK; this is an explicit control
test, not a claim about its saved pose.

V43's unweighted forearm helpers retain canonical offsets rather than fitted
Eve offsets. Their perpendicular distances from Eve's elbow-to-wrist axis are
about 6.17 and 12.35 cm per side. The upper-arm helper distances are about 1.23
and 2.46 cm. Reusing these pivots unchanged for new skin weights is unsuitable.

The V44A builder fits all eight helper pivots along Eve's existing limb segments
at the canonical fractional distances. It splits each affected main-arm weight
using the source bend/twist ratio and assigns the distal portion to the distal
game helper. This is a mapping hypothesis, not exact equivalence between the two
rigs. The body changes 1,510 vertex weight rows; the Black Pearl suit changes
2,322. Other affected source outfits are recorded in the report. Existing palm,
finger, body-physics and unrelated weight values remain unchanged.

All mesh coordinates, every stored morph coordinate/value, topology, UVs and
materials remain unchanged. Bone names/order, rotations and scales remain
unchanged. The maximum fitted Blender/JSON head discrepancy is 0.000108 cm.
The source blend, V43 blend and shared reference hashes remain unchanged.

## Evaluation and rejection

Unreal imports a diagnostic triangle carrying the candidate's complete 379-bone
bind reference. It independently reads that reference back for the renderer.
This tests the engine's pose calculation, not a cooked production skin or LOD.
Blender evaluates the candidate's actual skin weights on those engine poses.

The corrected 1,275-bone source fixture, authored retarget source, animated
virtual tracks, two CopyBone nodes and left TwoBoneIK node are preserved.
Baseline/candidate on the existing CSS and game-rotation references produce
80 cases across five H2 samples, raw/compressed paths and IK off/on. The engine
process exits zero and every enabled IK target error is below 0.001 cm.

The game-rotation baseline reproduces the previous foundation/CSS-mode poses.
Forty paired checks verify that all non-helper local poses remain within 0.001
of baseline. Unreal bone names are compared case-insensitively: two virtual
names differ only in capitalization. This candidate therefore cannot repair
finger rotations or the main arm IK pose.

Reviewed front/rear game-rotation renders show an unnatural curved forearm
contour and the unresolved left grip. The current-CSS-reference render also
retains the larger weapon/pose mismatch. A zero IK target error is not visual
acceptance. Both render processes exit zero; neither candidate is accepted.

Evidence directory: `work/grip-grounding-v1/arm-twist-binding-v1/`. Read
`candidate.json`, `twist-response.json`, `pose-validation.json`,
`evaluation.json`, `engine-candidate-bind.json`, `visual-review.json` and the
command/result manifests. Render folders under SeduXtress `work/nextgen-audit/`
are `arm-twist-v44a-game_rotations-s1-v1` and `arm-twist-v44a-css-s1-v1`.
The source blend is `work/CSS_SeduXtress_ArmTwistV44A.blend` under SeduXtress.

Reproducible scripts are archived in
[arm-twist probes](../../tools/authoring-probes/arm-twist/README.md).

## Next action and preserved scope

Test the full arm/hand rest-pose alignment described by the
[non-CSS guide](non-css-guide-findings.md), using measured source controls and
the working game reference. Preserve anatomical segment lengths and Eve's body
proportions. If reposing changes stored coordinates, transform every affected
morph and outfit consistently and verify the inverse transformation; do not
delete morphs or rescale Eve to Genessa. Original authoring files stay intact.

Do not repeat the weights-and-pivots-only candidate or declare a shared-skeleton
replacement sufficient. A V44 release still requires convincing arm/wrist/hand
deformation, the requested weapon/animation checks and reviewed live footage.
Accepted body dynamics, hair 200/24, modular assets and disabled CSSX remain
preserved. This hand milestone does not replace the full Next-Gen runtime, UI,
physics, authoring, performance and distribution objective.
