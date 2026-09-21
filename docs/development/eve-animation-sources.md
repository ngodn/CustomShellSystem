# Eve animation sources and gameplay boundaries

2026-09-21. This milestone prepares source animation keys and identifies the
gameplay boundaries for expanded locomotion. No new animation is installed.
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

Next: select and retarget source motion onto the accepted CSS bind pose without
changing proportions; evaluate actual skinned motion in the editor, author the
beacon pair, then connect modder metadata, saved choices, contextual UI and
runtime cancellation/weapon restoration. Full release acceptance remains open.
