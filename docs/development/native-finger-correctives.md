# Captured hand transfer and native correctives, 2026-09-21

V44B2 retains the passing skin checks for all 35 previously corrected captured
hand poses. The original finger corrective-shape logic now also runs in a
saved native Control Rig graph, with 53 passing execution checks. V43 remains
installed. The pose-calibration and clearance stages are not yet native, and
the new graph is not attached to a production mesh.

## Captured geometry check

The Blender replay uses the aligned B2 mesh, fitted authoring shape values,
the original 16 corrective values and actual armature deformation. All 35
poses retain exactly the four neutral seam triangle pairs. The older input
before thumb clearance still produces 40 additional thumb/index crossings on
B2. This preserves a failing control for the same defect.

The two reviewed close renders of that corrected case show separation between
the thumb and index tips while retaining the finger curl. They are isolated
hand views. The capture contains the original game overlay and blends, but
montage/context reads were not atomic with each pose. Do not label every
sample an attack or use these sparse observations as continuity evidence.

These tests transfer the retained corrected local hand poses and their shape
values to B2. They do not recompute the clearance solver against B2 or replay
its complete new animation graph. Captured body rotations still belong to V43.
The diagnostic applies B2 bind translations to evaluate the hand; it proves
neither weapon contact nor whole-character V44 poses. The old synthetic
thumb/palm and folding failures remain open.

Evidence: `work/grip-grounding-v1/arm-rest-captured-hand-v1/`, including
`report.json`, the terminal process result, contacts, `visual-review.json`
and render command/result manifests. Reviewed images are SeduXtress
`work/nextgen-audit/arm-rest-captured-thumb-render-v1/hand-l-a.png` and
`hand-l-b.png`. The blend's hash is unchanged after the replay.

## Local corrective driver

The original full-world formula cancels its common animated parent transform.
Each corrective can therefore read its calibrated joint's local quaternion:

```text
source_rotation = normalize(left * calibrated_local_rotation * right)
desired = clamp(coefficient * source_rotation.to_euler(YZX)[axis], 0, 1)
runtime_curve = desired - baked_value
```

All 16 original drivers use YZX Euler order and have matching direct parents.
Their constants preserve the earlier calibration's target-local convention.
They must not consume raw game rotations or the uncorrected B2 neutral pose.
Original rig constraints and coefficients are read rather than inferred.
The 13 open-to-closed source controls and 35 captured fixtures compare 768
values against the retained full-world evaluation, with maximum error
0.000001151. The original source blend is unchanged.

`work/grip-grounding-v1/hand-local-corrective-driver-v1/` records extraction,
validation and terminal exit 0. Its parameter JSON is archived byte-for-byte
as `tools/authoring-probes/hand-transfer/left-finger-correctives-v1.json`.
These are diagnostic parameters, not a general hand-rig standard.

## Native graph execution

The saved asset is
`/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftFingerCorrectivesV1`.
It uses 193 shipped native RigVM/Control Rig nodes. Python authors the graph
and supplies test inputs; the graph computes the curves during execution.
No custom game DLL or per-frame Python is required by this graph.

The graph reads local bone rotations, applies the constants, converts to YZX
degrees with `RigVMFunction_MathQuaternionToEuler`, converts to radians,
evaluates the original coefficient/clamp and subtracts each recorded baked
value. `RigUnit_SetCurveValue` writes the 16 private curves. `Enabled` defaults
to false; disabled execution writes zero curve deltas, returning to the baked
mesh. The graph contains no bone-write nodes or temporal state.

UE5.6.1 execution checks cover all 48 fixtures plus disable, re-enable, repeated
evaluation and two new-instance states. All 379 local bone transforms remain
unchanged. Maximum curve-weight error is 0.000001176 against a 0.00001 gate.
This also tests Unreal's actual Euler conversion against the retained source
values, rather than assuming Blender and Unreal conventions match.

The passing evidence is
`work/grip-grounding-v1/hand-native-corrective-rig-v3/`. Earlier v1/v2 runs
stopped on editor Python enumeration calls before any numerical tests or asset
save. The final harness uses the keys returned by bone import. Preserve those
failed logs; do not interpret them as failed corrective equations.

The canonical Skeleton and V43 mesh asset hashes remain unchanged. The graph
is saved only after its checks pass. This is direct editor rig execution, not
AnimBP curve propagation, GPU morph execution, cooking, runtime cost or live
game acceptance.

## Remaining integration

Port the validated incoming-pose calibration and bounded clearance stages,
then place this corrective driver after them. Recompute and check geometry
constraints for B2, including unresolved thumb/palm and same-thumb cases.
Verify all requested weapons, heavy poses, sidearm aim/fire and continuous
motion. The tolerated Axe & Dagger/Axatana behavior must remain usable.

The B2 full mesh still needs the 16 corrective targets exported, imported,
cooked and verified along with the final pose graph. Combine that with the
accepted hair/body dynamics and independent controls, then verify actual
AnimBP output, disable/reset, profiles, travel/death, performance and gameplay.
The full Next-Gen goal, modularity and distribution work remain active.

Versioned implementation references: UE5.6.1
`RigVMFunction_MathQuaternion.cpp:124`, `AnimationCoreLibrary.cpp:263`,
`AnimationCoreLibrary.h:73` (handedness conversion defaults false), and
`RigUnit_SetCurveValue.h`. [Reproduction scripts](../../tools/authoring-probes/hand-transfer/README.md)
use the existing workspace sandbox command manifests.
