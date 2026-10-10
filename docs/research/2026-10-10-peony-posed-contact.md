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

## Backstop, drive, friction and target-weight controls

The `peony-cloth15` follow-up keeps the regional collision asset and original
cloth8 targets. Removing backstop gives 9/70/58 failing walk/jog/sprint frames
out of 145 each; also removing animation drive gives 21/73/103. Neither repairs
the cloth. All 32 anchor checks pass. The original study receipt is preserved;
`followup-receipt.json` records these additional captures.

The private authoring helper now accepts `Friction=0..1` for Peony, records the
actual coefficient, and restores it on scope exit. It does not modify CSS
runtime or save a changed asset. The frozen before/after source and patch replay
in `peony-cloth16` agree, and the authoring build passed. With regional collision:

| Friction | Clip | Failing frames / 145 | Maximum edge ratio |
| --- | --- | ---: | ---: |
| 0 | Walk | 9 | 2.830 |
| 0 | Jog | 63 | 4.441 |
| 0 | Sprint | 36 | 6.192 |
| 0.1 | Walk | 6 | 3.174 |
| 0.1 | Jog | 63 | 4.154 |
| 0.1 | Sprint | 37 | 7.806 |
| 0.3 control | Sprint | 57 | 9.689 |

These are single editor runs, not statistically established improvements or
game timings. Every case still fails. All 32 anchors pass in each capture.

### Body-surface witnesses with corrected containment

`peony-cloth16/audit_contact_winding.py` uses the corrected winding classifier.
Its `regional-contact-witness.json` samples frames 0, 2, 6 and 38. Upper panel
targets 25/27/28/30/31/309 begin inside the posed body, 1.45..2.43 cm at frame 0
and 1.47..5.35 cm at frame 2. At frame 6 the regional capsule's projected contact
positions for several upper points still lie about 2.25..3.07 cm inside the
body. These projected positions are an analytic contact candidate, not a
readback of a solver iteration. Radius reduction therefore does not establish
body coverage, irrespective of lower strain counts.

The cloth9/10/12 witness reports compare their own skinned targets. Anatomical
and harmonic targets clear these six upper points at frames 0 and 2, but four
remain about 0.35..0.45 cm inside at frame 6. This is a selected-point comparison,
not whole-surface fit. Their `no_body` rows are the shared cloth8 reference and
are **not matched no-contact controls** for these changed weights. See
`witness-comparison-scope.json`; do not infer causal simulation differences from
those rows.

The combined harmonic-target/regional-collider trial is also rejected:

| Cloth17 collision | Clip | Failing frames / 145 | Maximum edge ratio |
| --- | --- | ---: | ---: |
| Original fitted | Walk | 55 | 4.646 |
| Original fitted | Jog | 104 | 6.397 |
| Original fitted | Sprint | 142 | 14.765 |
| Regional | Walk | 39 | 4.132 |
| Regional | Jog | 114 | 9.054 |
| Regional | Sprint | 144 | 13.251 |

All six captures pass their 32-anchor checks, and their source motion hashes
match the verification records. Better initial target clearance alone is not
an accepted fix. No candidate in this section is installed.

### Motion-bound reach screen

`tools/eve-fit/audit_cloth_contact_reach.py` checks a sufficient condition for an
impossible individual contact constraint: the entire allowed movement sphere
lies inside a collider's contact margin. Tapered capsule phi is not an exact
distance, so the calculation divides penetration by its Lipschitz bound
`sqrt(1 + ((r1-r0)/length)^2)` before comparing with the movement radius.
Analytic capsule, contained/reachable movement-ball, degenerate-axis and 10,000
deterministic tapered-bound controls pass in `contact-reach-controls.json`.

`peony-cloth15/regional-sprint-contact-reach.json` finds zero such conflicts and
zero fixed-point overlaps in all 145 sprint frames. This uses authored radii
and bone-transformed centers. The largest local scale deviation is 0.0000266;
native radius scaling is not independently read back by this script. Absence
of this sufficient conflict does not prove joint constraint feasibility,
coverage of the body, continuous collision or acceptable simulation. Do not
increase movement limits on the claim that this check found them too small.

### Prior triangle-collision work applies to the next decision

The September 27 studies in `eve-outfit-goal.md` concern the older dress panel,
not this Peony mesh. They already tested native skinned-triangle colliders,
independent skin readback, lightweight proxies and temporal contact. The pinned
5.6.1 solver uses current-proximity queries for those triangles; ordinary CCD
does not cover them. A 16-substep diagnostic reduced penetration but remained
visually unacceptable and expensive. Do not repeat those trials as an untested
general solution or ship them based on editor support alone.

## Constraint replay and contact-side divergence

`peony-correction-trace1` replays the native const PBD constraints separately on
independent copies of each captured end-of-frame particle array. It never
applies those replays to live solver particles. X=P for these copies, so this
is not internal iteration instrumentation or a velocity/damping reconstruction.
The corrected helper builds, its frozen patch replays exactly, and the 39-frame
capture passes all 32 anchor checks within 0.000020 cm.

At sprint frame 38, edge 31/309 is 15.807 cm long, versus about 1.900 cm at rest.
Both points' nearest shape is the same left-thigh capsule. Their radial contact
normals separate from 16.84 degrees at frame 0 to 138.61 degrees at frame 38.
The native edge pass pulls their separation down to 7.705 cm, but leaves them
4.125 and 3.532 cm inside collision geometry. The area pass similarly reduces
the separation to 10.335 cm while putting them inside by 3.116 and 1.353 cm.
Tethers, maximum distance and backstop leave this edge's separation unchanged
in this replay. Animation drive changes it by only about 0.018 cm.

This establishes a concrete conflict at this witness: constraints pull two
nearby material points together after they have separated around the same
collider. It does not prove a complete cause for all stretched edges. Frozen
`correction-summary.json`, `contact-normals.json` and the full native report
retain the evidence. The initial non-batch CLI invocation failed argument
validation without producing a motion capture; `corrections-batch.exit.json`
records the successful invocation.

## Directed thigh contact is tested and rejected

Pinned UE 5.6.1 `PhysicsEngine/TaperedCapsuleElem.h` declares a cloth-only
`bOneSidedCollision` property. `ChaosClothingSimulationCollider.cpp:663..665`
passes the capsule's rotated +X direction to the cloth sphere connection, and
`PerParticlePBDCollisionConstraint.cpp` reflects wrong-side contact corrections.
This is exact local source evidence, not a claim about an installed game test.

The private `peony-one-sided1` trial uses that property on six thigh capsules,
directed outward from the body's center. All original centers, radii, endpoints,
hip spheres and ordinary calf capsules remain unchanged. The native authoring
commandlet accepts a validated optional `one_sided_normal` only for private
Peony recipes. Fresh asset inspection verifies all eight standalone spheres,
ten capsules, flags and contact directions. Deliberately reversed flags and
rotations fail the expanded verifier.

The full-cloth candidate fails all three gaits:

| Clip | Failing frames / 145 | Maximum edge ratio | Maximum extension (cm) |
| --- | ---: | ---: | ---: |
| Walk | 80 | 59.167 | 30.574 |
| Jog | 31 | 21.129 | 27.567 |
| Sprint | 26 | 19.647 | 19.259 |

All 32 anchors still pass within 0.000025 cm. Fewer failed sprint frames do not
justify this candidate: severe corrections move to other edges. Walk frame 75
stretches edge 18/19 from 0.617 to 31.192 cm. Reject global outward thigh
correction for this cloth asset. No source garment, installed outfit or
gameplay physics asset was replaced.

The frame-75 mapped render was inspected from front and quarter views. The
panels show distorted edges and a displaced fabric patch between the thighs.
These diagnostic colors and hidden hair are not final-material validation.
The retained render mapping has a 0.378 cm maximum rest reconstruction error;
do not describe it as exact surface reconstruction.

## Separate left and right collision ownership

`peony-one-sided1/region-contact.json` identifies two disconnected proxy
surfaces: 531 left particles and 529 right particles. In the failed global
outward trial, walk frame 75 particles 18/19 have pelvis-skinned targets
inside the opposite thigh's authored capsules. The independent shape query
predicts about 1.94/2.02 cm ordinary correction versus 14.75/14.72 cm after
reflection across that opposite thigh's direction plane. These are queries
against authored shapes under captured poses, not intermediate solver traces.

The follow-up `left-directed.json` retains every collider but directs only the
left thigh. Fresh native readback verifies the geometry and direction flags.
All 32 anchors pass in three 145-frame gait captures. Left-panel failures drop
to 10 walk, 27 jog and 6 sprint frames, while the right panel fails 145, 145 and
21 frames. This control is not a repair; it motivates separate collision
ownership for the two disconnected surfaces.

`peony-partition1` splits only the primary fabric material into `_L` and `_R`
slots. It preserves source points, face corners, skin influences, bones and
22 source morphs. Proxy recombination checks preserve every particle field
and triangle. Both slots retain the same original fabric material. The private
import initially refused occupied package paths; the two original private
assets were then moved to `pre-import` with recorded hashes before importing.
No source or installed package was overwritten.

The built `peony-partitioned-cloth-probe.patch` assigns an explicit physics
asset per primary panel and supports selecting the captured cloth asset by
index. It refuses inferred trim attachment ownership and multi-asset collision
tracing. Existing single-asset behavior remains the default. Patch replay
matches both compiled source files exactly. Fresh saved readback confirms two
cloth assets, 16 pins each, unchanged distance/backstop maps, self-collision,
four iterations, and retained body physics/post-process references. The left
physics asset matches its previously verified staged hash; fresh right-asset
inspection also passes the geometry/direction verifier.

Both cloth assets simulate in every capture. Only the selected asset's
positions are exported. All captures below contain 145 frames at 60 Hz, with
all 16 selected anchors passing within 0.000025 cm.

| Panel and gait | Failing frames | Maximum edge ratio | Maximum extension (cm) |
| --- | ---: | ---: | ---: |
| Left walk | 13 | 3.269 | 3.385 |
| Right walk | 0 | 4.380 | 2.400 |
| Left jog | 0 | 5.084 | 1.677 |
| Right jog | 0 | 5.131 | 1.634 |
| Left sprint | 10 | 4.688 | 1.991 |
| Right sprint | 30 | 10.697 | 3.244 |

The screen requires an edge to exceed both twice its rest length and 1 cm
extension. The ratio and extension maxima can belong to different edges;
a zero failing-frame count is not final visual acceptance. Failures remain
beyond startup: left walk includes frames 25..30 and 61..66, and right sprint
includes frame 78 and late-cycle frames.

A control restores ordinary `PA_LegsInside` collision on the selected panel
only. Left sprint then fails 101 frames with 13.909 cm maximum extension;
right sprint fails 23 frames with 5.134 cm maximum extension. The other panel
retains its directed setup in each control, so these are selected-panel
controls, not a fully ordinary two-panel outfit. The left result supports
the directed-contact change. The right result has reduced severity but more
failed frames, so neither the whole candidate nor its performance is accepted.
Measured whole-component editor cloth-step means span approximately 3.3..4.5 ms
in the directed captures; these are not per-panel costs or game FPS results.

The left walk frame-0 and right sprint frame-78 mapped diagnostic views were
inspected. Each view replaces only its selected fabric panel; other parts are
skinned, including trim whose cloth ownership is still unimplemented. They
are not complete-outfit render or material validation. The right frame-78
rest mapping has a 0.378 cm maximum reconstruction error, and the left has
0.376 cm; neither replay is exact render-thread evidence. The right frame-78
failure is edge 85/519, growing from 0.335 to 3.579 cm; both endpoints have
pelvis-only targets with an unchanged skinned edge length. The left walk
frame-0 failure includes edge 401/403, growing from 1.849 to 5.235 cm, also
with unchanged skinned length. Continue from these contact witnesses before
changing weights or trying another broad parameter sweep.

`comparison.json`, saved binding checks, source-to-partition checks, native
readbacks and per-panel anchor/strain reports retain the results. The anchor
verifier now records its selected slot, and the strain audit rejects a
verification for another panel; the negative check passes. Primary fabric
partitioning is useful evidence, not an installed repair. Trim integration,
whole-outfit collision/morph checks, Windows cooking and gameplay acceptance
remain required.

`peony-one-sided1/followup-receipt.json` retains 182 files and
`peony-partition1/study-receipt.json` retains 212. Once all native and Blender
jobs terminated, exclusive editor locks and input/output hash checks preceded
cleanup of only their private `tmp`, `ddc` and `user` caches. The cleanup
receipts account for 75,723,886 logical bytes removed, with zero immediate
filesystem free-space change. Original authoring,
retained trial evidence and installed packages remain intact.

## Evidence retention and next work

Evidence is frozen in cloth15's `followup-receipt.json` and cloth16/17's
`study-receipt.json`. All staged inputs retain their recorded hashes. After
the native and Blender processes terminated, exclusive editor locks were
acquired and only each trial's `ddc`, `tmp` and `user` caches were removed.
The cleanup receipts record 59,850,800 logical bytes removed across the three
projects, with zero immediate filesystem free-space change. Source assets,
reports, recipes, native modules and installed packages remain intact.

The correction and one-sided studies each retain their own `study-receipt.json`,
frozen native patches and helper recipes. All 114 original staged files are
unchanged in both projects. The rejected candidate's authored collision asset
is retained for reproduction. Native build/run and Blender handles are terminal;
only their private caches were cleared after acquiring exclusive editor locks.

Inspect how the neighboring points first separate around the thigh, and which
colliders act on the new directed-trial failures. Do not repeat the same global
one-sided recipe as a fix. Distinguish initial target penetration, neighboring
panel regions and contact with the other leg before changing the garment's
attachment or collision partitioning. Inspect the remaining thigh-contact
conflict at the measured panel points,
including the skin target, pin transition, collision surface and actual rendered
panel. Do not repeat radius shrinking as a substitute for visible coverage.
The diagnostics identify a contact-sensitive region; they do not yet establish
whether fitting the collision surface, changing the constraint compatibility or
revising the loose-panel skin target is the correct repair. The additional
controls above reject backstop removal, drive removal, friction reduction and
the combined harmonic-target/regional-collider trial. The individual
motion-bound screen found no conflict. Inspect the actual contact sequence and
constraint corrections at a stretched edge, with the original body coverage
retained, before another parameter or collider-representation change.

Preserve full cloth simulation, body contact, accepted outfit shape, body
controls and materials. Final material/Windows cook, CINE, doubles, gameplay,
performance and release checks remain required by the parent goal.
