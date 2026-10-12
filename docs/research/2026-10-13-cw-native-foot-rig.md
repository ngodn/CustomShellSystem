# CW native foot-rig comparison

The native full-body rig affects CW's final leg pose. Disabling it has not been
accepted as a walking repair. A bounded reference-pose trial now passes complete
restoration, but does not yet establish a walking correction.

## Controlled bypass

`work/three-outfit-rig-physics/cw-rig-alpha1` records a rejected control: writing
the node's Alpha to zero does not persist because its graph input rewrites it.
The diagnostic recognized the different value and relinquished ownership.
It did not overwrite that later write. Saved settings were restored.

`cw-rig-scale1` instead changes only `AlphaScaleBias.Scale`, after a synchronous
`GetCurveValue` call on the same game-thread callback. UE 5.6.1's
`UAnimInstance::GetProxyOnAnyThread` drains existing parallel evaluation in that
context. `FAnimNode_ControlRig::Update_AnyThread` applies this multiplier after
the exposed input and clamp. The trial requires the verified ABP_Player class,
float alpha and zero bias. It does not copy the running AnimGraph struct.

Four stationary snapshots per phase confirm multiplier 1, then 0, then 1.
Explicit restoration and the separate one-second automatic timeout both pass.
The selected Iris Out idle and the complete saved CSS state are restored. No
movement or camera input was injected by this comparison.

Across the sixteen baseline/bypass pose pairs, right-calf local rotation differs
7.87 to 13.19 degrees and right-foot rotation 4.58 to 8.09 degrees. The restored
phase is much closer (right calf 0.30 to 2.72 degrees; right foot 0.27 to 3.98).
Idle phase varies, so these are ranges across sparse samples, not matched-frame
correction angles. Screenshots do not establish a walking improvement. Do not
ship a blanket full-body-rig bypass from this result.

The earlier reference comparison remains in `cw-foot-blend1`. Before trying
`bSetRefPoseFromSkeleton`, account for the engine's additional private
`bResetInitialTransformsBeforeConstruction` change. Restoring only the public
flag is insufficient. A fresh native animation instance could restore that
state. That lifecycle is now verified in the later recreation checks below.

## Reload regression found during the comparison

On core `563abafe6131e574`, initial shared walk/jog/sprint resolution succeeds,
then fails before the multiplier trial starts. Restoring Iris Out later reports
a missing facial shape. These failures precede the trial's rig mutation and
must not be attributed to its bypass without evidence.

A controlled reload to `baecd96329754a51` restores all three movement options,
the facial mesh and Iris Out performance. Another reload to
`d9b5ce2ba28e1c3f` reproduces an unavailable Iris Out. Its expanded diagnostic
reports `source=None.None`, while the target and declaration have their proper
CW and shared skeleton paths. A later reflected read of that animation finds
the correct shared skeleton. The CW face mesh also reads back 206 morph targets.

Evidence is in `cw-reload-diagnostics1`, including the log interval, failure
monitor and reflected object reads. This is a repeatable reload regression,
not a missing cooked asset established by the error text. The existing loader
activation receipt alone is not sufficient to pass a reload.

A rejected candidate used the reflected object-property accessor for UObject
reads and morph-target array entries instead of copying their storage as raw
pointers. UE 5.6.1 `PropertyObject.cpp` implements this accessor through the
wrapped pointer's `Get()`. See also Epic's
[object-pointer documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/object-pointers-in-unreal-engine?application_version=5.6).
The candidate is now rejected. `cw-object-reference1` passes fifteen follow-up
samples, but the identical DLL's second controlled reload in
`cw-object-reference2` reproduces a facial failure. The detailed message names
`CSSF_CW_Cinderella_000` and reports the candidate mesh path as `None.None`.
The later 206-entry reflected morph list contains that exact required shape.
Neither run records a difference between the raw object reference and its
reflected accessor. This does not establish pointer wrapping as the cause.
The accessor changes are removed from the retained source; its patch and test
receipts remain. Keep investigating load completion, cache validity and asset
lifetime, rather than changing the cooked face or weakening shape checks.

## Retained installed build

`cw-retained-diagnostics1` removes that experiment but still reproduces a gait
resolution failure. The next build keeps the narrow probe and extends failure
messages with object-array index, name index, load/post-load flags and destroy/
reachability flags. This records the object at the failing check, rather than
inferring its earlier state from a later successful reflected load.

Installed SHA-256:
`9b8cac120625639cc93db37c833774abb16fb2e76467c4db78411d54ee528331`.
Two controlled reloads, `cw-asset-evidence1` and `cw-asset-evidence2`, each pass
fifteen follow-up samples. Walk, jog, sprint and Iris Out are available; facial
performance is active with no error. Outfit, customization and the original idle
choice restore unchanged. The second uses the identical DLL under a fresh loader
filename. This is recovery evidence, not a fix established for the intermittent
failure. The extra diagnostic has not yet captured another occurrence.

The rig trial is inactive and sunlight Reduced noise remains selected. The
Windows development core builds and the host sunlight test passes. The clean
shortened menu label is visible in
`sunlight-misc-ui1/lighting-short-label.jpg`; real Default/Reduced clicks and
readback are recorded in that directory's `click-results.json`.

The second reload also passes 60 samples spanning 125.8 seconds in
`cw-asset-evidence2/soak-verification.json`. The trial remains inactive throughout
the final check. This observation does not establish that a garbage collection
occurred, or that the intermittent reload defect is fixed.

## Next reference-pose trial: establish full restoration first

The repeated-reload and collection checks are retained in the
[crash investigation](2026-10-13-cw-reload-crash.md). Their bounded passing result
allows a return to this investigation without declaring the crash fixed.

The pinned engine's `USkeletalMeshComponent::SetAnimInstanceClass` at
SkeletalMeshComponent.cpp:3492 does nothing when given the same class while
already in Blueprint mode. A null class clears the current instance, and
assigning the original class then calls `InitAnim(true)`.
`ClearAnimScriptInstance` at line 1374 drains existing parallel evaluation,
ends notify states, uninitializes the current instance and clears linked/cached
animation data. That supplies a candidate way to recreate the native ControlRig
and restore its private initial-transform state after a reference-pose trial.

Validate that lifecycle first, with the original idle, zero movement, no montage,
a retained original class, and both setter calls in one game-thread callback.
Read back a different AnimInstance and ControlRig identity, the same native class
and expected reference-pose flag, then verify selected locomotion, facial replay,
settings and rig initial transforms. These are proposed validation steps, not
completed tests. Do not merely flip `bSetRefPoseFromSkeleton` back to false and
claim restoration; ControlRig.h:665 declares the extra reset flag as a protected,
non-reflected field, and ControlRig.cpp:3564/3601 changes it during pose import.

## Native instance recreation and reference-pose trial

The DEV-only `animation.recreate_player_instance` request now performs the two
setter calls in one callback. It validates the current native player instance,
Blueprint mode, ready class/sparse data, no active performance, no menu, no
movement and no montage. It retains the original instance and class through
`AssetLoadRoots` until the new instance owns the class. An active Iris Out
request is rejected without replacing its instance.

Evidence is in `cw-rig-recreate1`, `cw-rig-recreate2` and `cw-rig-recreate3`.
The first attempt stopped before mutation because Speed was read as float;
live reflection establishes an eight-byte double and the diagnostic now uses
that type. The next attempt recreated the native instance and rig, with all 13
initial transforms exactly unchanged. Its strict post-process identity check
failed: `InitAnim(true)` also replaces the post-process instance. The replacement
has the same CW class. The third capture compares its CSS motion inputs instead.
Every input matches except JSON rounding in `CSSBodyOffsets`, with a maximum
absolute difference of 3.863e-11. `restoration-assessment.json` retains that
comparison at 1e-8 absolute tolerance. Original failed assertions are retained,
not relabeled as passes. Iris Out and facial playback recover, all four selected
animation choices remain available, and the saved CSS state matches exactly.

`cw-rig-reference1` uses that verified recreation path for explicit and automatic
trial cleanup. The existing synchronized scalar diagnostic now supports the
single reflected `bSetRefPoseFromSkeleton` flag, for at most 30 seconds. It does
not copy an AnimGraph node. Restoring the trial creates a new native instance
and rig, rather than merely clearing the public flag and leaving altered private
construction state behind. Both explicit reset and a separate one-second timeout
restore every checked initial transform exactly and clear the flag. The final
trial is inactive; no movement or camera input was sent.

With the flag on, all 13 checked reference transforms match the audited CW mesh
within 1.61e-6 cm and 1.71e-6 degrees. This includes the foot IK helpers that
previously differed by about 15.94 cm. That proves the engine imported the intended
mesh reference. It does not prove those offsets caused the user's walking defect.

Four stationary poses per phase do not show a clear improvement. Across the 16
unpaired baseline/trial pairs, left-foot local rotation differs 0.008 to 4.212
degrees and right-foot rotation 0.012 to 3.536 degrees. Restored-phase ranges
overlap those values, so idle phase is a confounder. The baseline and trial
screenshots show no obvious corrective change. Keep the reference adjustment
temporary and compare actual walking before considering a production policy.

The trial runs in the same Linux process 3113971 / Wine process 384. The installed
development core is `css_core-development-aec790e8409c73cd-rig-reference1.dll`,
SHA-256 `aec790e8409c73cd9c317881b34bcb318510a2a916e5b1197ba476fb22fb576d`.
Windows C++23 build and diff checks pass. The saved appearance, Iris Out choice
and facial playback restore; sunlight Reduced noise remains selected. The
reload-crash release gate remains open independently of this successful trial.
