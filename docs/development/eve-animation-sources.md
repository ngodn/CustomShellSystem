# Eve animation sources and gameplay boundaries

2026-09-21. Source keys and the first offline walk/idle retarget candidates are
prepared. No new animation is installed.
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
The solid-material renderer does not reproduce body/foot opacity masks, so
bare-foot surfaces appear through the shoes. Hair is rigid because secondary
motion is not evaluated. Neither artifact proves a defect in the installed mod.
Do not accept heels, hair, finger contacts or game integration from these views.
The protected production mesh and shared Skeleton remain byte-identical.

Next: bring the preview's visibility and secondary motion into agreement with
the game, inspect hand/heel contact and full motion from more angles, extend the
candidate set to jog/sprint, then validate compressed/cooked playback. Author
the beacon pair and connect modder metadata, saved choices, contextual UI and
runtime cancellation/weapon restoration. No game restart or runtime write was
needed for this checkpoint. Full release acceptance remains open.
