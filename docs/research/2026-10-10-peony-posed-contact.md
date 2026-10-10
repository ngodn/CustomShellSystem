# Peony target transfer and posed contact

Private authoring work on October 10, 2026. No result in this note is an
installed repair or a release candidate. Eve remains first in the full
three-outfit goal. No game launch or user test is needed for these checks.

Evidence paths below are relative to
`CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/work`.

## Body transfer and inpainted targets

`peony-surface-transfer1` compares the existing proxy skin weights with a fresh
barycentric transfer from the final body. The existing garment already uses
body-surface transfer. Repeating that operation is not a missing repair.
Median weight L1 difference is 0.000672; the 95th percentile is 0.015946.
Both fields fail the target-edge screen in all 145 sprint frames. Worst edge
extension is 10.645 cm with existing weights and 10.699 cm after fresh transfer.

The problematic loose points are several centimeters from the body. At frame
88, edge 597/598 measures 13.867 cm with existing weights, 13.852 cm after fresh
transfer and 3.657 cm at rest. Its matched body-surface points are 12.191 cm
apart. Those points are not necessarily neighbors on the body topology, so
this is not proof that the body mesh or skeleton is broken.

Epic describes the limitations of nearest-surface transfer for loose clothing
and using matched regions to fill unmatched weights in its
[Panel Cloth transfer tutorial](https://dev.epicgames.com/community/learning/tutorials/Dl20/unreal-engine-panel-cloth-transfer-skin-weights-node).
The private `prepare_peony_inpaint_targets.py` trial uses a positive harmonic
graph solve, not UE's biharmonic implementation. It changes 344 loose vertices,
preserves all 32 native pins and uses nearby fitted vertices as fixed boundaries.
The target-only sprint screen improves from 145 to 79 failing frames, with
worst extension reduced to 5.926 cm. That does not establish simulated fit.

The actual cloth12 native simulations still fail:

| Clip | Failing frames / 145 | Maximum edge ratio |
| --- | ---: | ---: |
| Walk | 57 | 5.780 |
| Jog | 108 | 6.742 |
| Sprint | 140 | 10.926 |

All 32 native anchors reconstruct within 0.000025 cm. Fresh inspection preserves
render geometry, section morphs and mesh/skeleton/physics/secondary references.
The inspector does not independently read back free-particle skin influences.
Cloth12 is rejected as a repair. Its evidence and 109 editor assets are hashed
in `peony-cloth12/study-receipt.json`.

## Travel and drive controls

`peony-distance-trace1` uses the exact cloth8 assets. The additive native
`peony-travel-scale-probe.patch` changes only an offline preview component.
Its default remains 1, matching UE 5.6.1's component constructor. The patch
replays exactly, builds successfully and reads back the selected scale.
No asset or installed runtime setting is changed by this diagnostic.

| Sprint case | Failing frames / 145 | Maximum edge ratio |
| --- | ---: | ---: |
| Travel scale 1 | 106 | 8.318 |
| Travel scale 4 | 106 | 8.336 |
| Scale 4, no backstop | 102 | 8.294 |
| Scale 4, no backstop or animation drive | 109 | 7.131 |

All four native runs complete successfully and all 32 anchor checks pass.
Body collision and self-collision remain enabled. Allowing up to 120 cm instead
of 30 cm travel does not remove the defect. Neither does removing animation
drive after the travel/backstop controls.

The scale-1 repeat has the same bone poses and identical cloth positions in
frames 0 and 1 as the older capture. Subsequent positions diverge, and its
failing-frame count is 106 instead of 109. Do not claim bitwise deterministic
simulation or interpret a small failing-frame reduction as an improvement.
Both captures reproduce the same large maximum stretch.

## Corrected collision-center classification

The original three-ray parity interpretation was wrong for overlapping body
surfaces. An even number of crossings can mean the point is inside two regions,
not outside the body. The earlier claim of 414 outside-center observations and
its conclusion that the original axes must move are withdrawn.

`peony-connected-fit1/original-outside-winding.json` samples 11 of those
observations. Every sample has generalized winding approximately -2, consistent
with two overlapping inward-oriented regions. The corrected audit evaluates
winding for all ray votes except unanimous inside votes, including 000.
This follows the solid-angle method described by
[Jacobson, Kavan and Sorkine-Hornung](https://igl.ethz.ch/projects/winding-number/).
It is a sampled containment screen, not proof of continuous garment clearance.

`original-regional-all-frames.json` now checks the unmodified cloth8 axes in
436 poses: reference plus all 145 frames of each gait. It finds zero rejected
axis/center observations. No radius candidate is emitted. The conservative
radius constraints remain infeasible; nearest surface distance in an overlap
is not necessarily distance to the boundary of the union.

`bound-surface.json` also shows that earlier whole-body nearest-distance bounds
for the left thigh were limited by ring-finger faces, and left-calf bounds by
the opposite thigh. Those bounds are unsuitable as isolated leg-envelope fits.
Regional surfaces avoid these unrelated faces but do not establish a complete
contact envelope at joints. The moved, split and heavily shrunken candidates
from this fitting sequence are rejected and were never imported or installed.

The native cloth stretch remains real. Its matched no-body-contact control is
still useful evidence. Next isolate hip and leg contact in the actual solver,
retaining all other settings, before changing collider placement or radii.

The cloth12 and distance-trace receipts were verified before removing only
their stopped private editor caches: 75,104,591 logical bytes. Immediate free
space did not increase on this filesystem. Sources, editor assets, reports
and installed packages were retained.

## Reference-pose lead-in

The motion harness resets cloth directly into sprint's first bent-leg pose.
That differs from entering sprint through a transition. The private startup
probe creates a transient, one-second sequence from the mesh reference pose
to the measured sprint pose. It retains all lead-in particle positions and
does not reset cloth again when the actual sprint begins. This tests
initialization sensitivity, not the game's actual locomotion blend.

The first two attempts stop at the pose-validation guard. Native animation
compression changes pelvis translation by about 0.003 cm, beyond the requested
0.001 transform tolerance. The third uses UE's raw-data debug option only for
the transient sequence and restores the prior setting before normal playback.
Every raw bone is checked against the authored blend on all 60 lead-in steps.
The final module builds and the native run exits successfully.

`peony-startup-trace3` still fails 103 of 145 sprint frames, with maximum edge
ratio 8.124 and worst extension 13.537 cm. All 32 sprint anchors reconstruct
within 0.000022 cm. The lead-in itself fails the strain screen in 51 of 60
frames, starting at frame 6, with maximum ratio 6.037. Those lead-in rows have
particle output and native local-pose checks, not independent anchor checks.
The abrupt initial pose therefore does not account for the whole defect.

## Native contact isolation

`peony-cloth13` retains the exact cloth8 mesh, maps and settings. Two fresh
private Physics Assets contain the original hip spheres or original leg
capsules. Native readback verifies all eight spheres and ten tapered capsules.
The offline motion helper temporarily selects each asset and restores the
original reference on scope exit. No gameplay Physics Asset is changed.

| Sprint contact | Failing frames / 145 | Maximum edge ratio |
| --- | ---: | ---: |
| Full | 103 | 8.318 |
| Hip spheres only | 1 | 2.344 |
| Leg capsules only | 103 | 8.318 |
| No body contact | 1 | 2.344 |

All 32 anchors pass within 0.000022 cm. Matching worst stretch and failure
counts do not mean the complete simulations are identical: full versus legs
positions differ by up to 9.528 cm; hips versus none by up to 2.800 cm.
The major reproduced strain is associated with leg contact.

`peony-cloth15` uses a transient duplicated Physics Asset to isolate selected
bones, keeping the source asset and gameplay collision untouched. The first
attempt (`peony-cloth14`) used UE string parsing that stopped at a comma.
Those multi-bone cases are invalid as requested group tests. The corrected
helper passes `bShouldStopOnSeparator=false` and verifies every requested bone
list from the native report. Both probe modules build successfully; the final
patch replays exactly. No runtime CSS binary is changed.

| Sprint contact | Failing frames / 145 | Maximum edge ratio |
| --- | ---: | ---: |
| Full repeat | 110 | 8.318 |
| Left leg | 88 | 7.619 |
| Right leg | 24 | 4.205 |
| Both thighs | 107 | 8.318 |
| Both calves | 8 | 7.763 |
| Left thigh only | 87 | 7.619 |
| Right thigh only | 18 | 4.218 |

All seven runs complete with the intended bone filters and all 32 anchors
pass. Thigh contact, especially the left thigh, reproduces most of the large
stretch. Calf contact also has a separate failure and cannot be declared sound.
These exclusions are diagnostic controls, not permission to ship missing
colliders. Offline timings do not establish in-game performance.

The stopped cloth13/14 caches were removed after receipt and lock checks:
40,197,090 logical bytes, zero immediate free-space change. Sources, source
assets, native reports and recipe/module identities were retained.

## Regional leg-radius trial

The corrected 436-pose regional bounds allow a private leg-only radius fit
while preserving all collider centers, connections and eight hip spheres.
`peony-cloth15/regional-radii2.json` is the tested recipe. Its native readback
matches all eight hip spheres and ten leg capsules. An earlier preparatory
recipe reduced the unused neighboring endpoint even at t=0 or t=1. It was
rejected before motion testing; the corrected recipe changes only the active
endpoint for those endpoint constraints.

The candidate still fails the native strain screen:

| Clip | Failing frames / 145 | Maximum edge ratio | Maximum extension (cm) |
| --- | ---: | ---: | ---: |
| Walk | 9 | 3.136 | 4.291 |
| Jog | 66 | 4.189 | 6.059 |
| Sprint | 53 | 9.346 | 8.727 |

All 32 anchors pass within 0.000025 cm. Fewer failed frames do not establish a
usable repair. The sprint maximum ratio is worse than the control, while its
worst extension is smaller and occurs on a different edge: frame 6, edge 27/30,
3.918 cm at rest versus 12.645 cm simulated. Jog still has the original 31/309
witness. The trial is not accepted or installed. No material or body morph
validation is implied by this contact test.

`regional-sprint6-review` contains front, back, side and quarter views reconstructed
from the saved UE render mapping and captured simulation. Front and quarter
views were inspected. These use diagnostic materials and hidden hair, and do
not establish continuous motion, final material or gameplay acceptance.

`peony-cloth15/study-receipt.json` freezes 74 evidence/recipe files and 111 editor
assets. Every original staged file retains its cloth8 hash. Its stopped private
caches were removed after checks: 20,297,584 logical bytes, zero immediate
free-space change. No installed asset or saved user setting changed.

## Next work

Inspect the remaining thigh-contact conflict at the measured panel points,
including the skin target, pin transition, collision surface and actual rendered
panel. Do not repeat radius shrinking as a substitute for visible coverage.
The diagnostics identify a contact-sensitive region; they do not yet establish
whether fitting the collision surface, changing the constraint compatibility or
revising the loose-panel skin target is the correct repair.

Preserve full cloth simulation, body contact, accepted outfit shape, body
controls and materials. Final material/Windows cook, CINE, doubles, gameplay,
performance and release checks remain required by the parent goal.
