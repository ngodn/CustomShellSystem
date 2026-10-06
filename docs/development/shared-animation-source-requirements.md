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
