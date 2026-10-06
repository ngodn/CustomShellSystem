# Missing source definitions

The local DumbDumb and Iris Out sequences decode, but their source rig cannot
be reconstructed from the supplied files. This blocks a complete faithful port.

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
key counts. Public redistribution terms are a separate unresolved requirement;
see [the source permission audit](shared-animation-source-permissions.md).

The user has been asked whether matching source files or an author-provided
link exist elsewhere. No author has been contacted on the user's behalf.
