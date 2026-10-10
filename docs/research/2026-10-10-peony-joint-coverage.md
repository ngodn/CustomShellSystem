# Peony joint coverage and remaining attachment fit

October 10, 2026. Private `peony-panelcoverage1` trial under the Eve authoring
root. Nothing installed. Continues the [backstop study](2026-10-10-peony-panel-backstop.md).

## Change and native comparison

The previous mapped witnesses entered the posed body while staying outside
the existing capsules. The leg chains started below the hip/knee joint,
leaving gaps near those joints. `prepare.py` adds proximal thigh and calf
endpoints fitted to body cross-sections, connected to the existing first
endpoint on the same bone. Existing spheres, radii and centers are unchanged.
The trial adds four endpoints and four tapered capsules across both legs.
Thigh endpoint radii are 12.167/12.143 cm; calf radii are 5.377 cm. These are
private trial values, not final fit acceptance or generalized anatomy defaults.

Each panel retains ordinary collision with the opposite leg. Its own thigh
retains directed contact. The first left-panel trial improves the sampled
sprint body contact but fails 32 walking strain frames, maximum extension
9.688 cm. Adjacent vertices 413/414 separate to opposite sides near the calf:
a 1.014 cm rest edge becomes 10.701 cm in walk frame 16.

The next comparison changes only three own-calf direction flags. Geometry,
cloth maps, backstops, collision thickness and self-collision stay unchanged.
Left walking and sprinting both pass the strain screen. The same construction
is mirrored for the right panel; fresh native readback verifies both assets,
including capsule endpoint positions and direction flags.

| Selected panel / gait | Failed frames, out of 145 | Max extension, cm |
| --- | ---: | ---: |
| Left walk | 0 | 1.671 |
| Left jog | 0 | 1.814 |
| Left sprint | 0 | 1.643 |
| Right walk | 0 | 1.526 |
| Right jog | 0 | 1.414 |
| Right sprint | 1 | 2.026 |

All 16 anchors per panel pass independently, with maximum error below
0.000025 cm. The strain predicate remains ratio greater than two AND extension
greater than 1 cm; its two maxima need not belong to the same edge. No pass
threshold was relaxed.

The right sprint failure is frame 109, edge 82/83, rest 0.660 cm to 1.863 cm.
A targeted eight-iteration comparison passes all 145 frames, with maximum
extension 1.636 cm and actual iteration count verified as eight. Its
whole-component editor cloth-step mean is 2.233 ms, p95 2.774 ms, maximum
3.383 ms. This single control is not adopted as a final setting or a game
performance claim. Its body contact and the other gaits at eight iterations
have not been checked.

These are selected-panel collision overrides on the prior saved mesh. The
other panel retains its previous saved collider in each run. A saved combined
candidate and its full-component checks remain required before adoption.

## Mapped contact and visible shape

Neutral-body winding queries use the same method as the preceding study.
Counts are unique mapped render vertices more than 0.02 cm inside the body,
not visible pixels or triangle intersections.

| Panel / pose | Earlier count / depth, cm | Directed joint trial count / depth, cm |
| --- | ---: | ---: |
| Left walk 0 | 195 / 3.838 | 93 / 2.212 |
| Left sprint 78 | 29 / 2.575 | 0 / 0 |
| Right walk 0 | 42 / 1.437 | 40 / 1.579 |
| Right sprint 78 | 427 / 3.422 | 419 / 1.898 |

Left walking's residual points classify by nearest body face as 48 leg,
36 arm and 9 torso. Maximum leg depth is now 0.826 cm. The worst torso and
arm witnesses, 2.212 and 2.098 cm deep, have native mapping flag 65535:
they are fully skinned, not cloth-driven. Their rendered positions are
exactly unchanged from the previous candidate under the same body pose.
Across the residual set, 19 unique positions are fully skinned (16 arm,
3 torso); the remainder are simulated or blended. Do not claim all remaining
intersections are fixed attachments.

This separates two remaining problems. The leg contact envelope still leaves
gaps, especially on the right. The upper attachment's skinning/fit and contact
with the forearm also need review; changing cloth colliders cannot move its
fully skinned vertices. Enlarging every collider to solve those vertices
would target the wrong layer.

Front, back, side and quarter views were inspected for left walk 0 and sprint
78. They use neutral shading and replace only the measured left panel; the
other panel remains skinned. They are not a final two-panel outfit render,
material review, footwear acceptance or game capture. Hem bunching and full
motion quality still need evaluation.

## Preservation and next work

All 115 staged inputs are retained, including the corrected combined panel
material, saved mesh, body physics, post-process graph and animation assets.
The only new Unreal assets are the private collision candidates. The runner
now permits this specific private project and pins the helper with the exact
Chaos-factory guard. No CSS runtime, installed outfit or saved user settings
change.

The trial retains scripts, command exits, collider readbacks, independent
anchor/strain checks, mapped contact records and visual evidence. Freeze and
cleanup receipts account for regenerable caches without deleting source
assets or rejection evidence.

Next: address the measured right knee/thigh gaps and upper attachment fit,
then save and check a combined candidate. Preserve full body/self collision
and cloth simulation. Body morphs, modular materials/palettes, Windows cook,
facial replacement, CINE, doubles and gameplay/performance remain in the goal.
