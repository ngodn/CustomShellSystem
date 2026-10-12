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

## Actual walking and component-space correction to the diagnosis

The user authorized independent overnight gameplay tests. The new DEV-only
`animation.walk_trial` holds the verified walk key C plus one WASD direction for
100 to 2,200 ms. It requires a stationary grounded player, focused game, no menu
or active performance, and no already-held movement key. Tick and unload cleanup
release both keys; loss of focus, expired pawn, changed movement mode, menu entry
or travel beyond five metres stops the trial. This is a diagnostic command,
not a production movement feature. `animation.walk_sample` drains animation
evaluation and captures Speed, direction, selected blend node, pose and location
in one callback.

`cw-walk-reference1` retains input-mapping discovery and terrain checks. The
mapping identifies C as the walk action. The current capsule is radius 35 cm,
half-height 96 cm, profile PlayerCapsule. Vertical traces across the short beacon
path and a full capsule sweep establish unobstructed geometry for the selected
route. They do not certify hazards in arbitrary scenes. Each comparison uses
two seconds walking backward relative to the camera, then two seconds returning.
No attack, interaction, save selection or explicit camera control was used.

The reference-flag comparison contains baseline, imported mesh reference and
restored phases. Each has seven speed-qualified samples, but only four are
straight: the first three contain turning. Use Speed 184 within 0.01 and absolute
LocalVelocityYawAngle below one degree. The four straight samples use the shared
Eve walk. Nearest nine-bone local-rotation fits give baseline 5.19 to 12.99 degrees,
reference 5.69 to 15.44 and restored 5.35 to 11.47. This does not support adopting
the mesh-reference flag as the walking repair. Full restoration passes.

`cw-walk-bypass1` stopped before gameplay input because the recorder assumed DP-1
while the game was on DP-2. `tools/css_capture.py` now captures the current game
monitor, requires its visible fullscreen window and rejects a monitor/workspace
change during recording. `cw-walk-bypass2` records three successful outgoing
passes with baseline, temporary zero rig multiplier and restored rig. The
character returns within 4.04 cm of the surveyed origin. All keys release and
the saved CSS state, Iris Out, facial playback and all four animation choices
restore. The three clips and their logs are retained.

The bypass changes the nearest nine-bone local fit from 5.20 to 12.72 degrees
to below 0.001 degrees RMS. Restoring the rig returns 5.54 to 11.68 degrees.
**This alone does not show that the rig causes the reported foot tilt.** Local
foot rotations compensate for changes in their parent knees. An initial progress
message overstated this result; the following component-space check supersedes
that interpretation.

`cw-rig-solver1/phase_compare.py` estimates phase from 13 upper-body and ball
local rotations, rather than fitting the corrected leg joints. It reconstructs
component-space feet with the audited CW mesh parents. For all 12 straight
samples, phase-fit RMS is below 0.0053 degrees. Baseline and restored foot
orientations differ from the imported animation by less than 0.0021 degrees,
while their positions differ by 2.8 to 12.4 cm. The bypass control matches both
orientation and position within 0.005 degrees and 0.0034 cm. Thus these sampled
native rig outputs move the feet while retaining their animated orientation.
Do not use the larger local-rotation numbers as an ankle correction or justify
disabling ground placement with them. Phase is estimated, not directly observed;
neutral-pose reconstruction excludes surface morphs and world floor contact.

The read-only cooked rig extraction in `cw-rig-solver1/cooked.json` includes
389 RigVM instructions. Its current bytecode references the named FootBone
literal operands for traces and foot controls, then adjusts targets before
PBIK. The legacy CDO maps those operands to foot_l/foot_r, but the current literal
payload still needs verification. Static `ik_foot_*` helpers in the imported
animation do not, by themselves, establish the cause. `disassemble.py` resolves
serialized operand descriptors;
the exact readback command is retained. CUE4Parse currently skips modern literal
property-bag payloads. Legacy memory CDO values must not be presented as verified
current literal values. No game rig package or literal was changed.

Next compare the CW surface's animation-to-reference alignment, with boots and
bare feet, at matched phases. Earlier four-bone AnimationRelative tests and the
source/CW reference comparisons remain relevant but are not accepted fixes.
Epic's [retargeting documentation](https://dev.epicgames.com/documentation/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)
describes retarget poses and differing bone orientations; use the pinned 5.6.1
implementation and actual source references for a scoped trial, not a guessed
constant angle. Preserve floor contact and the source heel design.

Installed development core:
`css_core-development-7a764e26757d2a6d-walk-reference1.dll`, SHA-256
`7a764e26757d2a6dd6744ff61eae49025b449c497b7352f5923c9ab11e39af1c`.
The Windows C++23 build and controlled activation passed before the captures.
Final `cw-rig-solver1/health.json` verifies that exact mapped module in Linux
process 3113971, inactive movement/rig trials, multiplier one, reference flag
false, all keys up and exact saved-state restoration. Reduced noise still owns
one light with no error. This closes the comparison, not the foot defect or
the separate reload-crash release gate.

## Follow-up ankle candidate

CW `work/foot-retarget2` now compares full-leg and four-bone reference retargeting
against the unchanged baseline. Full-leg worsens the average surface-bank measure
and is rejected. Ankle-only reduces the mean absolute bind-lateral angle from
3.783 to 2.427 degrees, with intact neutral cooked surfaces in the reviewed
boot/bare-foot renders. This measurement does not establish floor contact.

The four-mode skeleton candidate passes private save/reload, Windows cook,
independent comparison of all 84 decoded skeleton exports, full outfit payload
roundtrip and combined catalog/import checks. It is installed through transaction
`MortalShell2/Content/css-cw-ankle-trial-20261013`, keeping the same development
core. Process 3189742 loaded exactly four changes, all Skeleton to
AnimationRelative. The installed current source and reproducible tooling are in
the CW project; its `docs/walking-foot-report.md` records the complete chain.

Short booted and bare-foot walks complete with native IK active. Candidate
phase-matched global-foot orientation differs from the editor candidate by less
than 0.026 degrees. Both boot toggles, Iris Out, facial playback, all input keys
and saved state restore. Final readback confirms rig scale one and mesh-reference
flag false. Reduced noise remains active. Evidence and final health are in CW
`work/foot-retarget2/live-ankle1`. This advances the foot repair to an installed
trial; it does not close broader gait/slope acceptance or the reload-crash gate.


## Short jog and sprint checks (October 13, 05:56)

The development-only movement probe now distinguishes walk (C), jog (no
modifier), and sprint (Left Shift), bounded to 2,200, 750 and 550 milliseconds
respectively. Unknown gaits and durations outside the bounds are rejected before
input. It retains a weak controller and verifies that it still owns the tracked
pawn. Existing focus, grounded-state, menu and 500 cm travel guards remain.
The synchronized pose sample also includes the mesh-to-world transform; this
provides the missing coordinate conversion for later surface/contact analysis.

Windows C++23 compilation succeeds. Both controlled core activations preserve
Linux game process 3189742 and the complete saved CSS state. Final loaded core:
`css_core-development-ccf2940b2158a109-gaits2.dll`, SHA-256
`ccf2940b2158a109c31e20a6247207152db718cc2b631eed7811130807a10321`.
The installed four-bone CW asset trial remains unchanged.

Evidence is `work/three-outfit-rig-physics/cw-gait1`:

- `walking`: walk reaches 184 cm/s with seven active walk samples, including four
  straight samples. Jog reaches 540 cm/s with three active jog samples. This
  jog moves backward relative to facing, so it is not a forward-gait comparison.
  The initial sprint request only jogs. The driver correctly fails its sprint
  assertion, releases input and restores settings.
- `sprint-input`: the movement key is down while both Shift keys read false.
  This reproduces a diagnostic input defect. It does not demonstrate a broken
  outfit sprint animation.
- `sprint-scancode`: specifying the Left Shift scan code for both key-down and
  key-up makes the game read LeftShift=true and select BS_S1_Sprint at 645 cm/s.
  The native sprint camera changes yaw by 4.77 degrees. The return-path guard
  refuses that changed direction. `return.py` verifies the reflected
  SetControlRotation signature, restores the captured orientation while
  stationary, then walks back on the surveyed lane. The return finishes within
  2.18 cm of this pass's start, with all keys up and exact settings restoration.
- `forward-sprint`: a walk positions the character farther along the same lane;
  a brief forward step precedes a sprint toward the original position. Sprint
  activates at 643 cm/s, but the sampled movement angle remains 8.08 degrees
  during acceleration and 5.44 during deceleration. The stricter straight-sample
  assertion fails. This is an observed sprint transition, not a completed
  straight sprint cycle. Walk corrections return within 11.43 cm of the survey
  origin. Complete saved state, Iris Out/facial playback and Reduced noise
  restore; no rig override remains.


- `settled-sprint`: increasing the forward preparation walk from 100 to 600 ms
  reduces the observed sprint angle to 3.01 degrees at 640 cm/s and 2.07 while
  decelerating. It still fails the two-degree straight-sample condition. Do not
  loosen that condition after seeing the result or count this as a completed
  straight cycle. The actor returns within 9.23 cm of the survey origin and all
  settings restore. Further repeated short passes are not justified without a
  route/preparation plan that permits a settled capture.

The pinned UE 5.6.1 WindowsApplication.cpp (key-down line 2858 and key-up line
2936) resolves Shift handedness from the message scan code. Microsoft's
[KEYBDINPUT contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-keybdinput)
documents KEYEVENTF_SCANCODE and wScan. The paired live captures establish the
local fix; neither document alone proves Wine's behavior.

Do not report the failed aggregate scripts as passed or infer that these short
captures close foot-contact, long-stride, slope, CINE or doubles acceptance.
Reviewed walk/jog frames show rendered boot surfaces but do not establish
improvement against a phase-matched baseline. Preserve native IK and the
remaining release/crash gates. The follow-up should obtain a settled forward
sample within a surveyed route, then compare actual boot surfaces and contact.

Final read-only health in `cw-gait1/final/health.json` verifies the mapped core,
inactive rig/movement overrides, both Shift keys and WASDC released, native rig
scale one, mesh-reference flag false, all four selected animations available,
Iris Out facial playback, both boots visible, and full saved-state equality.
Reduced noise owns one light with no error. No large cook or package copy was
created in this continuation; `df -h` remains 119G free on root and 37G on theark.


## World-space boot contact follow-up

CW `work/foot-contact1` reconstructs the four steady straight-walk snapshots
using the synchronized world transform. Player-profile simple and complex
traces agree at all heel/forefoot landmarks. A fixed-parent counterfactual
undoes only the constant ankle rotations verified across 241 editor frames.
The candidate lowers mean absolute bind-lateral bank from 2.978 to 1.422 degrees
(seven of eight foot samples improve); lowest sampled forefoot clearance is
-2.244 cm versus -2.136 cm in the counterfactual. This supports retaining the
trial, not a blanket height adjustment or a whole-cycle contact verdict.
The original grounding solver was not rerun for the counterfactual. No assets,
settings, camera or position changed. See CW `docs/foot-contact-readback.md`
for the source, trace and morph-exclusion limits.
