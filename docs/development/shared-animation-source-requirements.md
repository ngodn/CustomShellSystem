# Missing source definitions

The local DumbDumb and Iris Out sequences decode, but seven source definitions
remain unidentified. The complete authoring rig is not reconstructed. Their
resolved body chains can now be auditioned independently, as recorded below.

Both sequences reference
`/Game/Art/Character/PC/CH_P_EVE_01/CH_P_EVE_01_Skeleton`, with recorded SkeletonGuid
`64ACCBA8-42A4F5D4-E692FAA9-08A5F607`. The installed retail skeleton and supplied
authoring skeleton do not establish the seven missing definitions below.
A matching GUID alone would not prove compatible bone order or reference poses.

| Source bone index | Animation track index |
| ---: | ---: |
| 3269 | 149 |
| 3270 | 150 |
| 3271 | 156 |
| 3272 | 157 |
| 3273 | 151 |
| 3274 | 111 |
| 3275 | 112 |

DumbDumb is `/Game/Mods/HD-ATOOL_P/anim/971`, 3,943 samples over 65.7 seconds.
Iris Out is `/Game/Mods/HD-ATOOL_P/anim/970`, 1,001 samples over 16.666666 seconds.
Every missing DumbDumb track contains multiple rotation and translation keys.
Iris Out has multiple keys on indices 3271 and 3272; its other five missing
tracks contain constant transforms. Constant tracks still need their binding.

The useful next input is the matching authoring Skeleton asset, with its
dependencies, or an authoritative export of its ordered bone names, parents
and local reference transforms, including virtual-bone definitions if present.
An FBX with unrelated bone ordering or a same-named retail Skeleton does not
establish these animation index bindings.

Facial completion also needs the original morph shapes or driver definitions
for the names listed in [the facial audit](performance-facial-curves.md).
The eight inspected retail Eve face meshes contain none of those exact names.
No aliases, dropped tracks or replacement expressions have been introduced.

The retained evidence is `work/cine-animation-port/unresolved-source-requirements.json`,
which identifies both source audits, their hashes and every unresolved track's
key counts. The user confirmed source permission on October 6; see
[the source permission record](shared-animation-source-permissions.md).

The user has been asked whether matching source files or an author-provided
link exist elsewhere. No author has been contacted on the user's behalf.

## Resolved body audition, October 6

The earlier all-or-nothing export rejection prevented evaluation of body tracks
whose identities and ancestors were already available. The established CSS
retargeter uses a closed set of 54 body/finger bones. Both dances supply all 54,
and none requires an unidentified parent in the resolved reference skeleton.
Retail and supplied authoring skeletons agree on all 54 names, indices and
parents. Maximum reference-position difference is 0.000005001 cm; quaternion
angle and scale differences are zero. This does not identify the extra bones.

`project_resolved_body.py` creates an explicitly marked body audition. It checks
metadata, track bindings, complete ancestry, key counts/times and finite valid
transforms. It retains the other 104 tracks, including all seven unidentified
tracks, in `preserved-tracks.json`. Every decoded key in all 158 tracks is
accounted for without modification. The strict general source exporter remains
unchanged. The prepared batch carries its audition status and preservation hashes.

Evidence under `work/cine-animation-port/body-projection-v1` includes input
hashes, two preservation receipts, `verification.json` and
`source-prefix-check.json`. The implementation passes nine rejection/preservation
tests; the existing four hierarchy and eleven sampling tests also pass.

UE 5.6.1 generated `/Game/CSS/AnimLab/RT_L5P_DumbDumb` and
`/Game/CSS/AnimLab/RT_L5P_IrisOut`. The former retains 3,943 keys over 65.7 seconds;
the latter retains 1,001 keys over 16.666666 seconds. Both remain 60 Hz.
`retarget-l5p/readback-result.json` compares all frames, each with 388 evaluated
bones, against saved assets and a repeated conversion in a fresh process.
Position and quaternion differences are zero. Sampling checks pass for both
source and target assets. Protected outfit mesh and Skeleton hashes are unchanged.

Five sparse skinned views per clip were generated using the existing Eve review
renderer. The inspected DumbDumb frames 984/2952 and Iris Out frame 500 show
recognizable whole-body poses without gross limb collapse. These are sparse
offline views, not continuous-motion, contact, facial or gameplay acceptance.
The first render launcher failed because `xvfb-run` was absent; direct background
Blender succeeded, without switching the desktop or interacting with the game.

The next work is full-motion/contact review, cooked readback and the actual
Eve, Genessa and Commander White adapter checks for these two candidates.
Facial mappings and unknown source definitions remain separately unfinished.
No new candidate has been added to the public catalog or installed in the game.

## Leg contact investigation, October 6

The full L5P diagnostic renders completed for both clips (1,972 DumbDumb and
501 Iris Out samples). These are review artifacts, not visual acceptance.
At DumbDumb frame 3414, the shoe mesh reaches -14.306 cm. Raw UE readback
confirms the source feet are near their ordinary ground height while the target
feet separate vertically. Cooking and Blender are not the origin of the error.

The actual source thighs are children of `Bip001-Spine`; the target thighs are
children of `pelvis`. UE 5.6.1's `FChainEncoderFK::TransformCurrentChainTransforms`
and `FChainDecoderFK::DecodePose` rebuild each chain relative to its mapped
target parent. A source parent's animated contribution can therefore be lost
when these parents represent different body joints. The default rig has no
foot goals. Epic's [IK retargeting documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?application_version=5.6)
describes FK chain transfer and optional IK contact handling; the exact local
5.6.1 implementation was checked before testing changes.

`retarget-l5p/leg-modes.json` records isolated One-to-One and Match Scaled Chain
trials. Neither fixes the ground penetration. These are retained failed
candidates, not replacements for the installed library.

`reparent_source_legs.py` derives an isolated source rig with the thighs under
the pelvis, recalculating only their local reference transforms and animation
keys. It retains all sampled world positions, rotations and scales numerically.
Both complete source clips pass that check, with maximum position component
error below 1.2e-13 cm. Six tests cover animated parent motion, nonuniform scale,
unexpected hierarchy, missing keys, invalid scale and unknown/duplicate tracks.
Original files stay unchanged; receipts are in `retarget-l5q`.

L5Q imports and retargets successfully. The inspected frame-3414 skinned view
reduces shoe penetration to -7.170 cm, so this is still not a finished repair.
A fresh UE source comparison also rejects the initial 0.001 cm key threshold:
the time-based evaluation measured 0.00659 cm at nominal keys and 0.392 cm at
half-frames. The follow-up verifier uses the frame API at keys and records the
worst bone and frame before reporting failure. The frame API gives the same
DumbDumb error at right toe frame 1052; Iris Out passes the key check. No
acceptance threshold has been relaxed, and this hierarchy conversion is not
selected for the final clip pipeline.

Explicit foot goals work with the original, unchanged L5P source. The first
attempt exposed an authoring API trap: `UIKRetargetIKChainsController::SetSettings`
calls `CopySettingsAtRuntime`, which copies only existing chain entries. Adding
goal definitions to a rig does not populate an old empty IK operation through
that setter. Rebuilding the isolated retargeter's default operation stack
populates the chains; settings readback confirms both foot goals are active.

Both L5P and L5Q `foot-ik-v3.json` reports now show the settings affect output.
The chosen audition uses source foot height, with a static offset derived from
the source versus fitted target bind heights, while retaining the ordinary
retargeted horizontal position. At 56.9 seconds, target foot heights become
7.775/7.711 cm and toe heights 2.192/0.802 cm. The unchanged source produces
the same corrected positions as the hierarchy experiment without introducing
another source interpolation stage. This is a sparse joint result, not yet a
whole-clip shoe-contact or pose-quality pass.

Full-length candidates `/Game/CSS/AnimLab/RT_L5R_DumbDumb` and
`RT_L5R_IrisOut` are being generated from that L5P foot-IK configuration. Their
export, hashes and subsequent skinned checks are under `retarget-l5r`.
