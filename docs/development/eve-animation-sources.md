# Eve animation sources and gameplay boundaries

2026-09-21. Four corrected offline candidates and compressed component previews
are prepared. [Native animation metadata and profiles](animation-options.md)
now support their separate slots; runtime/UI integration remains open.
No new animation is installed.
The accepted V44 rig, original proportions, hair 200/24, body dynamics and
-3 cm grounding remain unchanged. CSSX remains disabled.

## Requested result

CSS needs modder-provided Idle, Walk, Jog, Sprint and Beacon Teleport options.
Idle remains one visible control, with optional per-weapon selection internally
or a common unarmed idle. An unarmed idle must restore weapon visibility before
combat, aim, cancellation, outfit removal and other state changes.

For Eve, the user wants feminine, sensual movement like Stellar Blade, including
walk, jog and sprint. Use actual Eve clips as candidates, then review the full
skinned body after retargeting. Her optional beacon animation should be a
graceful, subtly sensual kneel with soft torso movement and a controlled hip
settle, followed by a matching rise. This direction is specific to Eve's set.

## Fresh local evidence

All readback and export evidence is under `work/anim1/`. The base game containers
were read without mounting mod directories or changing either installed game.
Tools use .NET SDK 10.0.401, the repository CUE4Parse checkout, Mortal Shell
5.6.1 mappings and the existing Stellar Blade retail mapping. Source discovery
used the existing container indexes; the selected packages were freshly decoded
from the installed containers, so candidate names are not the sole evidence.

`game.json` decodes five Mortal Shell packages, including `ABP_Player`, fast
travel and three kneeling abilities. `montages.json` follows their actual montage
references. `sb/` contains independent metadata decodes of the four basic Eve
clips. The eight named clip directories contain complete decoded source keys,
the source Skeleton hierarchy and source asset metadata.

| Candidate | Frames | Duration (seconds) | Mapped tracks |
| --- | ---: | ---: | ---: |
| Proto_Idle | 129 | 4.266667 | 139 |
| Proto_Walk | 37 | 1.2 | 138 |
| Proto_Run | 21 | 0.6666667 | 138 |
| Proto_Sprint | 17 | 0.53333336 | 313 |
| P_Eve_Peaceful_Idle01 | 211 | 7 | 138 |
| P_Eve_Peaceful_Idle02 | 241 | 8 | 138 |
| P_Eve_Peaceful_Idle03 | 241 | 8 | 138 |
| P_Eve_Peaceful_Idle04 | 151 | 5 | 138 |

These packages are under
`/Game/Art/Character/PC/CH_P_EVE_01/Animation/`. All reference
`CH_P_EVE_01_Skeleton`, with 3267 merged reference entries. That count is not
the number of animated bones or a target CSS rig requirement. The source and
CSS skeletons are different; do not import by matching bone indices.

The previously decoded Eve body AnimBP has direct references to `Proto_Idle`,
but no direct text references to the other three Proto clips. That does not
prove which movement assets Stellar Blade selects at runtime. Keep all eight
as candidates until graph selection and skinned motion are reviewed.

## Mortal Shell integration findings

`ABP_Player` derives from native `SpartaAnimInstance`. Its cooked
`GetCurrentBlendSpace` returns `ActiveBlendSpace` and `UseActiveBlendspace`.
There are separate linked locomotion, aiming, motion-matching and hand-correction
stages, plus DefaultSlot, FullBody and UpperBody montage slots. The existing CSS
walk override writes the two instance fields and changes the requested walk
speed from approximately 184 to 85 cm/s for the borrowed Cultist clip. It also
uses the same blend space at idle. This is not yet an independent custom idle,
jog or sprint system. Do not simply expose the old hidden run experiment as the
requested Eve feature.

`GA_Traversal_FastTravel` references `A_Shared_Actions_KneelDown_Montage`.
The montage uses the 4.266667-second `A_Shared_Actions_Kneel` sequence at
**play rate -1**, in DefaultSlot, with auto blend-out disabled. Its notify
subobjects identify these events:

| Time | Event |
| --- | --- |
| Entire montage | InputBlock notify state |
| 1.2 s | PreActivate |
| 1.5 s | FadeToBlack |
| 3.0 s | Activate |

`GA_Player_RiseFromKneel` references
`A_Shared_Actions_Kneel_Montage_NoCustomCamera`, playing the same sequence forward
at rate 1, with 1.5-second blend-in/out. This is a candidate arrival hook, not
proof that every beacon arrival uses it. `GA_Player_KneelAndReach` instead points
to a Lazlo memory montage, so its tempting name is not a valid reason to replace
it globally. `GA_Player_Kneel_Loop` has a distinct looping montage and ability
blocking tags.

The custom kneel must keep the game's event schedule, input ownership,
interruption behavior and root-motion contract. Animation style can change
within that schedule. Preserve the correct endpoints for the kneel loop and
rise. Trace the live arrival/cancel paths before selecting the integration hook;
do not mutate shared gameplay assets based only on the decoded defaults.

Epic documents montage sections, animation segments and notifies as separate
parts of playback, and notifies can drive gameplay events. These are the relevant
engine concepts; the exact timings above come from the installed game, not a
generic example. [Montages](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-montage-in-unreal-engine?application_version=5.6),
[Notifies](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-notifies-in-unreal-engine?application_version=5.6).

## Export and review checks

`MeshExport` now has a separate per-track source-key mode. It preserves time
arrays and endpoints instead of resampling through the legacy conversion
library's frame-count/duration rate. All eight clips pass the hierarchy,
mapping, finite-transform, quaternion and key-time guards. Repeated decodes
match exactly apart from the explicitly added interpolation metadata, and
frame counts/durations match their source asset properties. See
`source-verification.json` and [usage](../../tools/MeshExport/ANIMATION.md).

The existing default Mortal Shell ACL export of `A_Shared_Idle_H2` matches every
JSON field of the prior fixture, including virtual tracks, after adding the
new source mode. Evidence: `regression-result.json`. The .NET build passes;
upstream conversion-library warnings remain.

`review_source_tracks.py` builds an eight-second source-joint comparison and
stills at 0, 1, 3 and 5 seconds. These show distinct standing, walking and running
poses; Peaceful Idle02 includes a broad arm-opening gesture. The joint preview
cannot establish how Eve's skin, outfit, heels, fingers or secondary motion will
look. It is not gameplay or animation acceptance.

## First fitted-mesh candidates

Historical checkpoint: the uncorrected `anim2`/`anim3` candidates are superseded
by the facing-axis correction below. Do not deploy them.

Evidence is under `work/anim2/`. `Proto_Walk` and `P_Eve_Peaceful_Idle01` now
retarget through Unreal 5.6.1 onto `SK_BlackPearl2`, using its accepted mesh
bind pose. The source rig contains 54 body/finger bones with their original
parent hierarchy and local reference transforms. It omits mirrored accessory
helpers, not body proportions. An initial 314-bone selection was rejected by
the importer's positive-scale guard; do not weaken that guard. CSS retains its
own secondary-motion rig.

Twenty named chains map spine, head, clavicles, arms, legs, toes and fingers.
Target reference alignment uses the engine's automatic alignment. Outputs
`/Game/CSS/AnimLab/RT_Walk` and `RT_Idle` explicitly set their retarget source to
the fitted target mesh. Do not substitute the shared Skeleton's reference
rotations, which would reopen the earlier hand-binding problem.

The editor's `IKRetargetBatchOperation.duplicate_and_retarget` is unsuitable
for this headless commandlet: its completion path accesses Slate and crashed
with `CurrentApplication.IsValid()`. This was an offline editor crash, not a
game crash. `CSSAnimationLibrary` uses the pinned engine's pose processor
directly and never invokes the UI completion path. It accepts bone-only,
non-additive clips at rate 1, rejects curves, attributes, notifies and speed
planting, refuses existing output packages, and never saves assets itself.
The local 5.6.1 implementation is authoritative; newer API documentation has
different signatures. Epic describes the necessary external source-scale
step in [ScaleSourcePose](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/IKRig/FIKRetargetProcessor/ScaleSourcePose?application_version=5.6).

Tool files under `tools/authoring-probes/animations/`:

- `prepare_eve.py` recreates the 54-bone import payload and original-time samples.
  Its outputs match the initial prepared JSON exactly (`preparation-result.json`).
- `CSSAnimationLibrary.h/.cpp` are copied into the editor project's
  `Source/CSSAuthoring/`. Apply `tools/authoring-patches/headless-retarget-build.patch`
  there to add IKRig and AnimationBlueprintLibrary dependencies. The existing
  editor project uses C++20. The final build exits 0.
- `retarget_eve.py` creates private rigs, imports source sequences, converts
  and saves the two candidates, and samples all frames. Its optional
  `resume.json` permits only explicitly hashed assets left by an interrupted
  preparation. It refuses an already completed batch.
- `verify_eve.py` reloads both saved clips in a fresh editor process and repeats
  both conversions without saving the repeats. All 37 walk and 211 idle frames,
  including 388 evaluated bones, match the earlier raw poses exactly.
- `render_eve.py` replays those transforms on the V45C source blend, preserves
  authored fit shapes, and checks all 379 bones against the engine transforms.
  It never saves the blend. Maximum observed replay position error is below
  0.00064 cm; the reported quaternion-angle error is zero at Blender precision.

The helper initially checked the animation's cached sampled-key count inside
an open edit bracket. That cache updates later. The corrected guard reads the
data model's key count directly. Retarget and fresh readback commandlets exit 0;
the remaining warnings are blocked writes to the host Epic configuration,
which is intentionally read-only. Engine user data, logs and scratch remain in
the workspace.

Loop endpoint differences are below 0.000071 cm / 0.087 degrees for walk and
0.00223 cm / 0.007 degrees for idle, including virtual bones. These are endpoint
checks, not guarantees of smooth velocity or planted feet. Review videos retain
the original 1.2-second and 7-second durations at 10 fps; the repeated endpoint
image is excluded from encoding. See `walk-review.mp4`, `idle-review.mp4` and
`motion-review.json`.

Reviewed stills show a recognizable walking sway and the idle's relaxed turn
with a hand resting near the opposite arm. This is preliminary visual review.
The initial renderer omitted the game's covered-foot section selection and
the heeled stocking/lining parts, so bare feet appeared through the shoes.
This was a preview assembly defect, not evidence of an opacity-mask defect.
Hair is rigid because secondary
motion is not evaluated. Neither artifact proves a defect in the installed mod.
Do not accept heels, hair, finger contacts or game integration from these views.
The protected production mesh and shared Skeleton remain byte-identical.

Next: bring the preview's visibility and secondary motion into agreement with
the game, inspect hand/heel contact and full motion from more angles, extend the
candidate set to jog/sprint, then validate compressed/cooked playback. Author
the beacon pair and connect modder metadata, saved choices, contextual UI and
runtime cancellation/weapon restoration. No game restart or runtime write was
needed for this checkpoint. Full release acceptance remains open.

## Facing correction and component playback

Evidence: `work/anim4/`, with the rejected uncorrected jog/sprint controls in
`work/anim3/`. The upright walk and idle hid a source-facing mismatch. Faster
clips exposed it: the source's forward torso lean became sideways on CSS.
The source right-minus-left hip axis is +Y; the fitted CSS axis is -X.
Rotate the source coordinate basis +90 degrees around Z, in both its root
reference transform and every animated root key. Leave every non-root local
reference transform and the target mesh unchanged. Rotating only the animated
keys or editing the target binding would not be the same correction.

`prepare_eve.py --clips Walk Jog Sprint Idle --revision V2 --source-yaw 90`
creates separately named private assets under `/Game/CSS/AnimLab`. The checked
root-only conversion reproduces the rotated reference world matrices within
3.56e-14. At jog frame 6, the retargeted head-minus-pelvis vector changes from
approximately (26.80, -3.70, 40.72) cm to (4.07, 25.62, 40.56) cm. Sprint frame
4 changes from (41.30, -2.41, 26.30) to (2.79, 40.00, 26.42). Reviewed raw
renders now show forward running lean. Preserve the rejected controls so this
axis error is not rediscovered through repeated pose adjustments.

Fresh-process saved readback and repeated conversions match all 388 evaluated
bones exactly across 37 walk, 21 jog, 17 sprint and 211 idle frames. Production
mesh and Skeleton hashes remain unchanged. Editor build, conversion and
readback all exit 0.

`component_eve.py` invokes the C++ helper's `EvaluateClip` on an isolated
skeletal component. It requires valid compressed animation data, disabled
force-raw evaluation and matching virtual-bone GUIDs. A separate component
without the post-process supplies the upstream control. The real
`ABP_Secondary` evaluates hair, body and left-hand rigs, including active hand
corrective morph weights. The probe runs three walk/jog/sprint cycles and one
idle cycle at 60 fps, totaling 217/121/97/421 samples including endpoints.

All three output filters are present: 29 hair, 7 body and 19 hand bones. Bones
outside those filters have zero measured local translation change and less
than 6.67e-8 radians rotation difference. Rig counters advance, hands report
valid, hair uses the accepted 200/24/0 and body uses 2 Hz / 0.7 damping / 1
motion / zero gravity. The helper does not save the mesh, Skeleton or AnimBP;
their hashes are checked before and after. Compressed upstream versus raw
local poses differ by at most 0.00381 cm and 0.161 degrees across the four
clips (`compression-comparison.json`); this is not a skin-contact tolerance.

The fitted-mesh renderer now hides the exact covered-foot body sections and
shows heeled stocking feet and footwear lining. It replays the component's
bone transforms and active hand morphs while preserving authored fit shapes.
It also supports front, back and side views. Source blends are never saved.
Component previews are in `work/anim4/component-{walk,jog,sprint,idle}`.
All four Blender runs exit 0. Maximum bone replay error is below 0.000947 cm,
with zero measured quaternion-angle error at Blender precision. The encoded
`*-component.mp4` files run at 15 fps for 3.6/2/1.6/7 seconds, excluding the
repeated final endpoint. Sampled stills show the corrected running lean and
visible heel supports. The left hand remains visibly spread in these poses,
and the long hair overlaps the lower-body silhouette in some running views;
inspect those contacts from additional angles before visual acceptance. The
clips have been generated, but only sampled stills were visually inspected
at this checkpoint.

Limits: the owner remains stationary, materials are diagnostic solids, and
the probe does not evaluate game locomotion blends, weapon overlays, foot IK,
collisions or gameplay. Compressed editor execution is not cooked/live
acceptance. No animation is installed. Next inspect contacts and transitions,
then cook and integrate optional animation metadata, saved choices, UI and
runtime cancellation/weapon restoration. Beacon authoring remains open.

## Locomotion blend sources and idle playback investigation

`work/anim6/` contains successful read-only decodes of Stellar Blade's
`IdleRun_BS_Peaceful2D`, `LockOn_IdleRun_BS` and `LockOn_Sprint_BS`. The
peaceful BlendSpace references the Proto clips directly, so they are not
merely animation-name guesses. This does not prove that the current Stellar
Blade runtime selects that graph.

Its first axis is Speed, maximum 800; the second is LeftRight, -1 to 1.
The central samples are Proto_Idle at 0, Proto_Walk at 150, Proto_Jog at
300, Proto_Run at 500 and Proto_Sprint at 700. Sprint repeats at 800 with
rate 1.14. The current CSS Jog candidate uses Proto_Run, not Proto_Jog;
the distinction matters when fitting cadence to Mortal Shell's movement.
Walk/jog/run also have left/right slide samples. The lock-on locomotion
BlendSpace uses normalized LeftRight/FrontBack axes and separate cardinal
and diagonal clips. Do not stretch the current forward clips into strafing
or copy Stellar Blade's input axes into Mortal Shell's direction/speed API.
Retargeted stride, transitions and gameplay selection remain unverified.

Pinned UE 5.6.1 `AnimInstance.cpp` lines 2286 and 2395-2398 establish that
`PlaySlotAnimationAsDynamicMontage` calls `Montage_Play` with its default
stop-other-montages behavior for the same group. It is therefore unsuitable
as an unconditional idle replacement alongside game-owned combat/travel.
The alternative under investigation is a constant BlendSpace containing the
same idle at four direction/speed corners. This is an idle wrapper only,
not a directional locomotion asset or a completed runtime integration.

The commandlet helper builds successfully. `idle_carrier.py` creates a
private `/Game/CSS/AnimLab/BS_V2_Idle` and compares compressed sequence and
BlendSpace execution on isolated components with the actual secondary rigs.
Single-node BlendSpace time is normalized, while sequence time is seconds
(pinned `AnimSingleNodeInstance.cpp`, `GetLength`). The first center-input
comparison did not pass its strict 0.001-radian full-pose equivalence gate:
maximum rotation difference was 0.0051216 radians, about 0.293 degrees.
Fresh-process readback at both corners and an interior input reports the
same maxima and localizes the largest rotation difference to
`CSS_Hair_Ponytail_05`; upstream animation differs by only 0.000001061 radians.
Do not describe this as exact secondary-motion equivalence, widen the gate
without justification or retune the user's accepted hair physics to hide it.
Saved assets and production mesh/Skeleton/AnimBP hashes are checked by the
probe. No animation is cooked or installed by this experiment.

The live player probe timed out while the game process remained present.
That is not crash evidence. No gameplay input or restart was attempted, and
runtime animation ownership, weapon restoration and graph layering remain
pending live verification.

## Directional source batch D1

`work/anim9` records 19 clips resolved from the actual animation references in
the previously decoded lock-on BlendSpaces. Walk and run each supply forward,
backward, left, right and four diagonals. Sprint supplies forward, left and
right only. Its source BlendSpace repeats those clips across FrontBack; that
does not establish a backward sprint animation.

The short labels are `WF/WFR/WR/WBR/WB/WBL/WL/WFL`, the equivalent `J` labels,
and `SF/SR/SL`. `J` identifies a prospective CSS jog slot, but these are
Stellar Blade's `Proto_Lockon_Run_*` assets, not a claim about their correct
playback speed in Mortal Shell. Walk has 37 keys over 1.2 seconds, run has
25 over 0.8 seconds, and sprint has 17 over about 0.5333 seconds, all at 30 Hz.
`source-index.json`, `source-verification.json` and `clip-map.json` retain the
original paths, hashes, durations and mapping. All 19 extraction processes
returned zero. Their 54-bone source bind matches corrected V2 exactly.

The authoring scripts now accept a bounded short-label/source-name map rather
than four hard-coded clip names. Duplicate selections are rejected before
output creation. The old four-clip V2 preparation still produces the same
eight JSON files. All new assets remain private under `/Game/CSS/AnimLab`,
using short `D1` names; no source game files or production assets were changed.

Fresh-process readback and repeat conversion match all 388 evaluated bones
exactly in all 19 raw sequences. Compressed component execution covers 1,075
frames with the actual hair, body and hand post-process. Its defaults and
29/7/19-bone filters match the accepted V2 component baseline. Unaffected
bones have zero position error and at most 6.6641e-8 radians rotation error.
This batch evaluates one cycle per clip, not repeated-loop stability.

Fitted-mesh replay produced three sampled poses for each clip. All 57 renders
were inspected in `walk-review.jpg`, `jog-review.jpg` and `sprint-review.jpg`.
The replay position error is below 0.000842 cm. The samples show coherent
directional body poses, with the accepted wardrobe and heel supports present.
They also show limitations that prevent deployment acceptance:

- Lock-on walk uses raised combat-ready arms. Preserve the distinction from
  relaxed forward movement and validate game weapon layering before use.
- In sampled forward run and forward/right sprint poses, the ponytail falls
  in front of or overlaps the torso/legs. Left sprint differs. Compare upstream
  head/hair attachment transforms and initialization before changing anything;
  do not retune the accepted production hair settings to hide this result.
- These are stationary-owner, solid-material samples without weapons or a
  floor-contact check. They do not prove whole-motion quality, cadence,
  directional blending, combat contacts or gameplay transitions.

Evidence includes `batch/readback-result.json`, `batch/component-result.json`,
`validation.json`, `render-result.json` and each `render-*/report.json`.
Import, retarget, fresh readback, component evaluation and all 19 render
processes returned zero. No D1 animation or new core is cooked or installed.
Next resolve the sampled hair behavior, construct direction/speed blends with
appropriate phase/cadence, and complete custom idle, weapon restoration and
beacon integration before controlled live acceptance.

## D2 attachment propagation correction

D1's hair-root error occurs before secondary physics. The compressed upstream
forward sprint has about 27.91 degrees of unexpected local rotation at each
head-attached hair root; right sprint reaches about 41.02 degrees. Raw samples
already contain the error, so compression and the accepted spring settings
are not its origin.

The pinned UE 5.6.1 source identifies the mismatch. In
`IKRig/Private/Retargeter/IKRetargetProcessor.cpp`, lines 321-398,
`GetCachedEndOfBranchIndex` and `GetChildrenIndicesRecursive` stop when a
contiguous hierarchy branch ends. `RetargetOps/FKChainsOp.cpp` lines 661-750
uses that traversal to propagate mapped-parent motion to unmapped children.
CSS preserves game indices and appends its extensions. The head is index 9,
its cached branch ends at 13, and its ponytail starts at 258. Parent-link
traversal finds 47 head descendants outside that cached block. The same
audit finds 121 omitted pelvis descendants and 53 for spine_05. Exact source
hash and lists are in `work/anim10/branch-audit.json`.

`RetargetClip` now preserves retarget-pose local transforms for unmapped
terminal attachment branches. It gathers mapped bones from initialized,
enabled retarget operations and walks actual parent links. It skips mapped
bones, intermediate joints with mapped descendants, branches attached only
to the root, and virtual bones. This repairs the exported animation tracks;
the production skeleton's order, binding and body proportions stay intact.
An explicitly mapped accessory chain keeps its authored animation.

Direct `FTargetSkeleton` mask accessors are not exported by this installed
engine, as the first link attempt established. The helper uses exported
processor access plus each operation's virtual `CollectRetargetedBones`,
matching the engine's own mask construction. The corrected editor build
passes (`build3-exit.json`). Do not patch the engine or reorder the shared
skeleton to work around this authoring-only traversal limitation.

`attachments_eve.py` uses independent parent-link and chain calculations to
verify 23 private D2 candidates: all 19 D1 directions and the four V2 forward
movement/idle clips. It covers 297 unmapped attachment bones; the 24 appended
branch roots have meaningful rotation changes. With preservation disabled,
the forward-sprint control reproduces the old raw result exactly. Corrected
creation, fresh saved readback and repeat conversion all pass. Mapped raw
tracks and every track outside the attachment set match the old result
exactly. Corrected attachments match the independent reference within
4.87e-7 quaternion distance and zero translation error. All protected mesh,
skeleton, post-process and prior animation files retain their hashes.

Compressed playback passes 1,643 component frames, keeping the accepted
hair 200/24/0 and body/hand settings. Compression is regenerated, so do not
claim byte-identical or numerically identical final body output: the 19
directional comparisons outside hair/corrected roots differ by at most
0.005123 cm and 0.16428 degrees, with at most 0.001654 hand-morph difference.
Those measurements are in `body-comparison.json`; they do not replace a
visual contact check.

All 69 sampled fitted renders were inspected in `work/anim10`'s three review
sheets. The ponytail now attaches behind the head in the formerly bad sprint
samples. Some dynamic strand/body overlaps remain visible, including run and
idle samples. This fixes attachment propagation, not every hair collision or
whole-motion issue. Replay error is below 0.000835 cm. The candidates remain
offline, with no core or game asset deployment. Next build directional blends,
check phase/cadence and game weapon layering, then review actual moving-owner
hair/contact behavior with the new motion. Do not retune the accepted physics
merely to make stationary-owner sample images pass.

## Original author's pose and deformation references

The user supplied `reference/body-type-variant-EVE/3HVzUb9.gif` and
`SmutBase • [Stellar Blade] Eve.pdf` in the Eve authoring directory. Inspection
records and sampled GIF frames are in `work/anim3/author-ref/`.

The 23.29-second GIF demonstrates body-physics controls and deformation during
movement. It is useful for judging deformation, but does not establish a
correct weapon pose or supply the requested locomotion clips. PDF pages 14-17
describe corrective shape keys for extreme poses, tweak bones, a separate
Asset Library pose collection and physics rebinding after geometry/outfit
changes. Those are useful authoring references for shoulders, hips and the
planned idle/kneel; they do not establish compatibility with the game rig.

The local reference folder has no separate pose-library file. Read-only blend
library inspection found `WalkInPlace` (frames 1-37, 1229 curves) and
`EveWalkTweaks` (frames 36-216, 102 curves) in `eve_beta10.blend`. Their motion
has not yet been played back or accepted. Queue comparison against the game
clips after the current coordinate correction. Do not infer from their names
that they are the same animation or directly compatible with CSS.

Release provenance note: the supplied PDF lists CC BY-NC-ND 4.0 on page 3.
Record the author's applicable permissions before distributing derived source
assets or a starter kit. This inspection establishes the local document's
contents, not a legal conclusion about the user's separate author agreement.
