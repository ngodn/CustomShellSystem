# Slipstream binding and body-shape study

The private Slipstream candidate now binds nine fitted garment meshes to Eve's
379-bone measured rig and carries all six existing body shapes. No Slipstream
package is installed. Peony refit2 remains the current in-game trial, with its
live fit acceptance pending.

The authoring files are in
`CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/work`.
`slipstream-binding-study1/receipt.json` freezes the candidate files, current
checker results, diagnostic renders and copies of the authoring scripts.

## Binding

`bind_slipstream_study.py` starts from `slipstream-bootfit2.blend`. Primary
surfaces transfer body weights and shape deltas through body triangles selected
by anatomical region. Trim follows its garment layer. The lower boots retain
rigid foot weights, blended into leg weights above the ankle. The loose skirt
blends original mapped weights with body transfer according to its clearance.
That skirt weighting is provisional until simulation and moving fit are checked.

`slipstream-bind1-finalcheck.json` passes after a fresh reload:

- The body and nine garment Basis surfaces match the retained fitted geometry.
- Triangle order and garment/source ownership are preserved.
- Every vertex has finite, normalized weights on known target bones.
- All six shape keys are present and saved inactive.
- Neutral rig deformation stays within the 0.00001 m tolerance.
- A deliberately moved fitted point is rejected.

The saved binding reports at most six garment influences per vertex. The check
does not establish final export influence packing or cloth behavior.

The retained walk at frame 36 agrees with independently calculated linear
skinning to a maximum 0.000432 cm component error. This verifies the transform
and skinning calculation, not the fit. Its local contact screen still identifies
83 inward skirt samples, 128 sleeve/yoke samples and 610 lower-bodysuit samples.
Boots have no inward samples against visible skin in this pose. Moving fit is
not accepted and this candidate must not be cooked as finished clothing.

## Chest body-shape correction

Direct nearest-surface morph transfer buckles the chest fabric at maximum
breast size. The isolated breast-size screen reports 379 inward bodysuit
samples; body tone alone reports none. With all six shapes at one, it reports
294 inward bodysuit samples. These are oriented contact samples, not counts of
separate visible defects.

`slipstream-chestshape1` smooths the displacement field across the bodysuit and
its trim. Smoothing alone exposes skin through the lower chest and is rejected
as a final correction. It remains an intermediate input for the constrained fit.

`slipstream-chestshape2` follows the neutral torso fit's continuous-displacement
approach. It fits a 2 cm field with 618 centers against 26,904 sampled contact
constraints on the deformed chest. The initial deepest residual is 0.6235 cm;
after correction no constraint exceeds the 0.01 cm tolerance. The maximum
additional garment correction is 0.724 cm. The fitted Basis and body are not
expanded or otherwise edited.

Independent full primary-surface checks now report zero inward samples both
for breast size alone and with all six body shapes at one. Front, side and back
diagnostic renders are retained. This does not establish every intermediate
slider value, moving fit, self-contact or final visual/material acceptance.

`slipstream-chestshape2-check.json` compares a fresh reload against bind1.
Body, Basis, topology, weights, UVs and the five other shapes are identical.
Only the breast-size key on four bodysuit/trim meshes differs. The neutral
binding checks and deliberately changed-point rejection also pass.

## Contact-screen limitation and next work

The first diagnostic excluded covered lower-leg faces before building its
nearest-surface tree. That made boot points query the distant cut boundary and
produced misleading inward distances. Current reports retain the full body
tree and explicitly count queries whose nearest skin is covered. The reports
ending in `-rest-contact` and `-walk36-contact` supersede the initial contact
numbers embedded in the first neutral/walk render reports. Screens still use
local oriented distances and cannot prove global separation.

Next: correct moving sleeve and lower-bodysuit binding, then validate the skirt
with its intended physics. Sweep body shapes and gait frames before export.
Materials, five palettes, removable-part runtime behavior, facial/Astral
integration, cooking and gameplay checks remain required. The whole outfit
and coordinated release goal remain unfinished.

## Walking contact diagnosis and native preparation

`slipstream-skin-contact36.json` records vertex/triangle-center witnesses,
body triangles and both skin-weight sets. The deeper jacket witnesses lie at
the sleeve/yoke junction, where a restricted arm donor surface gave some points
too much upper-arm weight. Lower skirt witnesses include the opposite thigh
crossing the skinned skirt during the stride. They require a cloth test, not
an assumption that rigid skinning can keep a loose skirt clear of both legs.

`slipstream-body-contact36.json` also checks the corresponding body anchors
against the other anatomical region. Some witnesses coincide with contact
already present in the target body pose: 17 of 53 jacket witnesses, 131 of
248 cross-region bodysuit witnesses and 12 of 34 skirt witnesses. This local
oriented-distance evidence does not explain every defect or prove closed-volume
self-intersection. Do not enlarge or modify the accepted body to make these
counts zero.

`slipstream-jacketbind1` transfers sleeve/yoke weights through the complete
body surface, preserving garment geometry and the corrected shapes. It changes
167 primary jacket points and transfers the jacket trim from that layer.
The frame-36 jacket contact count drops from 128 to 86. A complete 145-frame
walk comparison is mixed: total inward samples drop from 30,531 to 28,137 and
the worst per-frame count from 603 to 579, but 18 frames gain up to six inward
samples. The deepest witness remains about 1.985 cm. This is a private test
candidate, not a verified regression-free repair. Further visual/contact and
body-shape checks remain required.

### Skirt simulation surface

The raw skirt has branching waist geometry after seam welding, so directly
using its render mesh as a cloth proxy is rejected. Proxy generation preserves
all render geometry. It removes 331 inward-facing simulation triangles and 45
small branch faces in the upper waist band, then reduces only the simulation
surface. Exact removed branch coordinates/areas are recorded in the report.

`slipstream-cloth-proxy2` contains 624 particles and 976 triangles, one connected
surface with no edge shared by more than two faces. Its 102 pinned particles
follow the sloping waist, with anchors in all twelve 30-degree sectors. The
free simulation targets blend toward the pelvis, rather than pulling the hem
between opposing thighs. All six body-shape delta arrays are retained.

Independent readback checks 10,108 render-surface samples against this proxy:
maximum distance 0.23146 cm, p95 0.08600 cm, none above 0.3 cm. The source mesh
is unchanged. The initial proxy1 used a flat-height waist threshold and is
superseded by proxy2's contoured anchors. Starting stiffness, motion limits,
backstops and self-collision are provisional until native motion/contact tests.
Topology and close mapping alone do not establish cloth stability.

The existing native `CSSBindClothV3 -CheckTopology` command also passes and
exits successfully. It constructs Chaos's triangle topology and exercises the
five-ring adjacency used for self-collision exclusions for every particle.
This still does not execute cloth simulation.

### Garment import

`slipstream-export1` exports all 31,625 garment triangles as 13 material sections
covering six independently removable groups. The source has 25,436 points,
379 measured bones and all six body shapes. Independent interchange comparison
passes for positions, winding, UVs, weights, morph deltas and ownership.

UE 5.6.1 imports and freshly reloads the private asset successfully. Native
readback reports 31,625 triangles, 25,463 built vertices, 13 ordered sections,
379 bones and six nonempty morph targets. `native-verified.json` records the
asset and command hashes. This checks counts, ownership and shape presence;
it is not a full native vertex/delta comparison or a Windows cook. Materials
remain placeholders and the garment asset is not an installable character.

Next native steps are cloth binding, collision/animation staging and the actual
gait simulation. Complete-body/facial assembly, reversible boot coverage,
materials, palettes and all release checks remain pending. No game package or
runtime DLL changed during this work.

`work/slipstream-native-study1/receipt.json` freezes 40 files covering the
contact diagnosis, current scripts, export/readback, cloth proxy and native
topology result. Native logs are copied into immutable snapshots after their
processes exit, so later trace-server output cannot invalidate those snapshots.

### First native cloth simulation

`slipstream-cloth-native1` stages 53 immutable Eve dependencies and the private
garment import. A private copy of the UE 5.6.1 editor helper now accepts the exact
canonical Slipstream mesh and collision paths. The Peony partition-specific
guard remains unchanged. The module builds successfully; its project and binary
hashes are pinned in `editor-module.json`. Shared authoring source and installed
game files are unchanged.

`bind_slipstream_cloth_trial.py` checks the measured bone names and parents,
retains the mesh bind pose and six body morphs, sets the existing Eve skeleton,
physics and secondary graph, and copies the existing cloth collider into the
private Slipstream namespace. All 53 protected dependency hashes still match.

The first complete cloth binding attempt fails before saving: the independent
`Slipstream_skirt_Hardware` section loses its shared cloth mapping during mesh
rebuild. The saved garment hash remains equal to the pre-cloth binding receipt.
Do not bypass this guard or ship a skirt with independently skinned trim.

The existing Peony combined-section method has been adapted in
`merge_slipstream_skirt_sections.py`. `slipstream-skirtmerge1` retains all 8,000
skirt triangles (2,082 fabric and 5,918 trim), using UV2 to distinguish their
material families in one section. Undoing only the declared material-index/UV2
edits reconstructs the original interchange exactly. Empty material slots are
retained, so other ownership indices do not move. This is prepared interchange,
not a native-imported or material-complete replacement yet.

For collision diagnosis only, the primary fabric section was then bound without
the separate trim. One invocation had an incorrect config path and exited
without saving; the corrected invocation succeeded. These are diagnostic assets,
not a complete outfit candidate. The saved-setting native captures contain
145 frames each at 60 Hz, with the same 624-particle proxy2 and original copied
collider. Independent skinning verifies every native kinematic particle using
the engine's below-0.1-cm distance threshold:

| Capture | Maximum anchor error (cm) | Failing strain frames / 145 |
| --- | ---: | ---: |
| Walk | 0.0000249 | 0 |
| Jog | 0.0000283 | 134 |
| Sprint | 0.0000257 | 136 |

The strain guard requires an edge to exceed both twice its rest length and an
additional 1 cm. Jog reaches 5.497 cm maximum extension, sprint 4.645 cm. The
candidate is rejected. Correct waist attachment does not establish cloth fit,
collision quality, body-shape behavior or acceptable motion. The retained
starting collider and proxy constraints need contact isolation next, using the
recorded failing frames rather than a blind settings sweep. Native merged
fabric/trim binding and combined materials remain mandatory before full-outfit
acceptance. Secondary motion was not enabled in this first diagnostic batch.

`work/slipstream-cloth-study1/receipt.json` freezes 154 evidence files, including
the private helper sources, copied final logs, retained dependency assets,
motion captures, independent checks and merged-section preparation. Both the
corrected primary binding and the motion batch exit successfully. Test failures
above are garment-quality failures despite successful commandlet execution.

### Combined skirt and convergence follow-up

The next controlled captures use the same mesh, proxy and collider. All retain
correct native waist anchors. The counts below apply the existing edge strain
guard, not a silhouette or collision acceptance criterion:

| Diagnostic, 145 frames | Jog failures | Sprint failures |
| --- | ---: | ---: |
| Original settings, one iteration/one substep | 134 | 136 |
| Body collision excluded | 68 | 105 |
| Backstops excluded | 14 | 74 |
| Self collision excluded | 134 | 136 |
| Animation drive excluded | 109 | 132 |
| Four iterations, all features retained | 42 | 92 |
| Eight iterations, all features retained | 26 | 42 |
| Eight iterations/two substeps, all features retained | 0 | 0 |

The exclusions diagnose constraint interaction; none is a shipping choice.
Increasing convergence while retaining the same constraints removes this
particular gross-strain failure in the single-node clips. The saved candidate
still needs coverage and visual review, as the following results demonstrate.

`slipstream-cloth-native2` imports the combined 8,000-triangle skirt, binds its
single fabric/trim section successfully, and saves eight iterations/two
substeps. It retains the original 624-particle proxy, all backstops, body
collision, self collision and animation drive. The trim-binding failure is
resolved at the native section level. Its final material is not authored yet.

Fresh native readback and `check_slipstream_native_skirt.py` compare the saved
8,000 triangles and winding, 6,412 render vertices, weights and six source body
shapes. Source geometry matches; maximum bone-weight error is 0.00002242 and
retained morph-delta error is 0.00001336 cm. The initial verifier incorrectly
required tiny filtered deltas to remain present. The corrected check reads the
actual native build threshold, 0.015 cm, and separately bounds omitted deltas
and retained-delta error, following the established Eve facial verifier.
Maximum filtered magnitude is 0.01464994 cm. An altered native delta of 0.1 cm
is rejected by the same check. This is not a claim that all source deltas remain
unfiltered, or that cloth rendering already responds to the body sliders.

All six saved-setting gait captures pass independent native kinematic-anchor
checks. Walk, jog and sprint without secondary physics pass the strain screen;
walk and jog with secondary physics also pass. Secondary sprint fails at frames
90, 118 and 119 on a few short edges. Thus five of six screens pass, and the
candidate remains unaccepted. Editor cloth-step means are approximately
1.26..1.53 ms in this batch, excluding animation refresh and rendering. These
are not game frame-time measurements or an isolated performance comparison.

Native render mapping is independently replayed at sprint frame 132. The saved
mapping's reference reconstruction has a 1.570 cm maximum error, with 401 of
6,412 vertices above 0.1 cm. Diagnostic front/side/rear images visibly show rear
body clipping. Keep this failure even though most strain screens pass. The
surface contact screen is only an oriented-nearest-surface measurement, not
proof of closed-volume containment.

The saved proxy normals differ from triangle-averaged normals by design: the
existing preparation uses body-facing backstop normals. Exact UE 5.6.1 source
also confirms cloth render binding constructs a separate averaged-normal
surface (`ClothingAsset.cpp`, `ClothingMeshUtils.cpp`). That difference alone
does not identify a bug and no normal rewrite has been made. The projection
code has a triangle-normal fallback; its involvement in the recorded mapping
error is a hypothesis requiring a targeted check.

A separate secondary-sprint contact capture records frame 119. All 32 native
collider transforms match the recorded bone transforms, with maximum actual
matrix error 0.00004197. Native particle coordinates match readback within
0.00000106 cm. No particle penetrates the collision shapes in this frame:
minimum native signed distance is 0.30000035 cm. However, 102 of 624 particles
have an inward nearest-body-surface sample below -0.2 cm while remaining outside
the collision shapes. The worst witnesses are near the right thigh and glute
weights. Combined with the visible clipping, this directs the next work toward
coverage of the posed body and the render mapping, rather than another arbitrary
solver increase or a bone-transform rewrite. The repeat is a separate simulation
capture and is not assumed bit-identical to the earlier secondary sprint.

Next: repair demonstrated collision coverage and mapping failures in the
complete skirt; recheck rest fit, all body shapes, posed fit, secondary physics,
cloth stability and cost. The source body and garment designs remain preserved.
No installed package changed. The stopped private `native1/editor/Intermediate`
cache was removed after locking and verifying pinned binaries: 391,289,005
logical bytes, with zero immediate available-space change. Required source,
assets, binaries and diagnostic evidence remain available.

`work/slipstream-cloth-study2/receipt.json` freezes 202 evidence files, final
log snapshots and authoring recipes, with all 53 protected dependencies unchanged.
Its `precision-followup.json` records the verifier refinement that measures
position error against the original interchange coordinates after the float32
spatial lookup. `skirt-verified2.json` passes the same bounds. Native game
acceptance, materials, morph rendering and performance work remain open.

### Fitted thigh collision and mapping follow-up

`slipstream-collisionfit1` replaces the sparse thigh sphere cloud with six
tapered capsule connections fitted to the accepted body's cross-sections.
The other 21 spheres stay unchanged. Eight thigh endpoints bring the total to
29 spheres. Neither the body nor the garment is edited. Reconstructed baseline
collision distances match the native trace within 0.000383 cm. The candidate
contains no reference-pose proxy vertices or attachment points; its minimum
rest clearance is 0.488 cm. This is a useful rest check, not posed acceptance.

The existing collision commandlet creates the diagnostic asset at
`/Game/CSS/eins0fx/EveSB/Peony/Physics/PA_SlipstreamFitTrial1` inside the isolated
Slipstream project, using its existing guarded authoring path. The native motion
test selects it through a temporary collision override. This is not a published
path or a change to Peony. Any accepted final asset belongs under Slipstream's
own `Physics` namespace. The saved garment and all 53 protected dependencies
retain their previous hashes.

The fitted collider reduces the recorded secondary-sprint frame 119 inward
body-surface witnesses from 102 to 42, and the deepest from 8.019 to 2.619 cm.
Those are oriented nearest-surface samples, not a closed-volume proof. The
native render replay still shows rear clipping and distorted cloth. Both full
sprint captures fail badly, so reject this candidate:

| Diagnostic | Failing frames | Maximum additional edge length |
| --- | ---: | ---: |
| Fitted collider, sprint | 111/145 | 15.147 cm |
| Fitted collider, secondary sprint | 110/145 | 15.147 cm |
| Fitted collider, backstop excluded | 111/145 | 15.156 cm |
| Fitted collider, 60-frame reference-pose lead-in | 110/145 | 15.003 cm |

All four captures retain valid independent attachment checks. Removing backstop
or easing startup does not resolve this failure. Those exclusions are diagnostic
controls, not proposed shipping settings. The largest first-frame split is edge
149–151, which grows from 1.642 to 16.790 cm. Both proxy vertices currently follow
the pelvis entirely, with travel limits of 8.920 and 8.297 cm. Next inspect the
posed target/contact relationship at this witness and compare the original
fitted garment weights before changing more collision radii or solver settings.

The mapping audit now identifies the stored formula rather than speculating
about backstop normals. All 401 render vertices with rest reconstruction error
above 0.1 cm match the triangle-plane fallback coefficients within 0.0002.
The worst row is vertex 5966 on proxy triangle 474/476/506; its saved barycentric
weights are approximately 0.306, 2.253, -1.559, a substantial extrapolation beyond
the triangle. Exact UE source is `ClothingMeshUtils.cpp`, functions
`GetPointBaryAndDist` and `GetPointBaryAndDistWithNormals`.

Two position-only diagnostics search 48 nearby triangles and solve the normal
offset polynomial, retaining the existing simulation blend and source geometry.
Both correct 207 affected vertices after float32 coefficient storage. Allowing
mixed fixed/free triangles makes no additional improvement. The remaining 194
vertices include the worst 1.570 cm error, so neither is a finished mapping fix.
No native mapping was patched. A complete repair must also preserve normal and
tangent mapping, address the missing nearby support at the waist, and pass fresh
native readback and moving render checks.

`work/slipstream-cloth-study3/receipt.json` freezes 41 files, including both exact
mapping recipes, contact/strain evidence, rendered views and the private collider.
All three native commandlets exited successfully; the negative strain verdicts
remain recorded. No game package, installed core, save or profile changed. The
goal remains active. Peony refit2's live stationary-fit acceptance is still open;
the user's clarification that standing fit must work remains the requirement.

## Follow-up: original weights, fitted collision and shorter travel

The retained native3 comparison changes only proxy skin weights from proxy2's
free-panel pelvis blend to the original fitted garment weights. Geometry,
distance maps and six body-morph arrays remain unchanged. Original collision
then fails two sprint frames and one secondary-sprint frame out of 145; fitted
thigh collision fails 51 and 48. All waist-anchor checks pass. Removing backstop,
adding a gradual pose start or enabling CCD does not repair the fitted trial.
These are rejected cloth candidates, not installed outfit changes.

`slipstream-surfacecollision1` fits 32 dedicated skirt-region spheres against
the complete body in the reference pose and six captured poses. It preserves
0.15 cm sampled containment margin and 0.909 cm minimum reference-proxy
clearance. This is sampled neutral-body evidence, not full morph acceptance.
The native3 surface-collision comparison still fails eight frames in each
sprint capture. The diagnostic asset remains in the authoring helper's allowed
Peony namespace; it must not be published there as a Slipstream dependency.

`slipstream-cloth-proxy4` changes only maximum travel, retaining the waist taper
and limiting free movement to body clearance plus 0.5 cm, bounded to 0.75..3 cm.
The native5 trial reduces frame-119 inward nearest-surface witnesses to 17,
with a worst distance of 0.983 cm, but fails 121/145 sprint and 122/145 secondary
sprint strain frames. Reject it. The first-frame edge 583-584 stretches from
3.511 to 8.614 cm. Adjacent center-fabric vertices carry opposing thigh weights;
measure their skinned target separation before selecting a localized weight
correction. A closer travel limit does not establish a better garment fit.

The separate native4 standard multi-influence mapping comparison also fails:
maximum rest reconstruction error grows from 1.570 to 1.608 cm. Reject it.
The position-only `slipstream-mappingfit3` diagnostic corrects all 401 rows above
0.1 cm by permitting no greater barycentric extrapolation than each source row.
Its remaining maximum error is 0.0946 cm. It has not been applied to a native
asset, and does not validate normal/tangent mappings or persistence through
mesh rebuilding and Windows cooking.

Evidence is frozen in Eve's `work/slipstream-cloth-study4/receipt.json`:
354 files are hashed and 142 small recipes/reports/logs are snapshotted.
Large captures and assets remain at their recorded source paths without extra
copies. All commandlets terminate successfully, all three private projects
retain their 53 protected dependencies, and pinned modules match. Candidate
rejection is based on the output, not the command exit status. Native4's stopped
build cache and two already-preserved duplicate mapping files were removed;
the cleanup receipt does not claim an equivalent increase in physical free space.
No game asset or CSS core changed during these comparisons.

## Center-weight conflict and remaining collision coverage

`fit_slipstream_center_weights.py` independently skins the captured source pose.
At frame zero, edge 583-584 has a 3.511 cm rest length but its two animated targets
are 14.426 cm apart. Even allowing each endpoint 3 cm of movement forces at least
8.426 cm separation. This proves a constraint conflict in the rejected short-
travel trial; raising solver iterations cannot make those limits compatible.

`slipstream-cloth-proxy5` changes 164 free-particle weight rows. It uses the
existing pelvis depth fade within 4 cm of the center and blends back to original
weights at 8 cm, preserving all 188 native waist anchors and every other proxy
field, including morph arrays. The five central edge witnesses retain their rest
target lengths across the capture. This establishes target consistency only.

The native6 comparison retains the original 12 cm travel and dedicated 32-sphere
collision. Anchors pass, but sprint and secondary sprint fail 18 and 19 strain
frames. The frame-119 body screen finds 78 inward witnesses, worst 2.551 cm.
The mapped rear review visibly clips, so reject it. Fresh mesh readback matches
native3 except the regenerated clothing binding GUID; the existing 1.570 cm
rest-mapping error remains unresolved.

Native7 combines those center weights with the previous close-travel map. It
fails all 145 frames in both captures, with maximum extension about 14.27 cm.
At frame 120, edge 322-536 stretches from 2.691 to 16.961 cm. Although only 13
inward nearest-body witnesses remain at frame 119, that does not justify the
distortion. Reject this combination and stop tightening the movement envelope.
All 251 native anchors still pass; a coordinate/order failure is not indicated.

`slipstream-surfacecollision2` separately evaluates a 96-sphere fit using the
same 3,405 candidates and seven body poses. Median/p95/worst sampled body gaps
improve from 1.331/2.977/4.525 cm to 0.869/2.320/4.184 cm. An analytic check at
native6 frame 119 reaches only 27 of its 78 previously uncovered clipping
witnesses, leaving 51 outside collision thickness. The definition has not been
imported, simulated or cooked. More spheres alone have not solved coverage.

The local UE 5.6.1 collider reads dynamic sphere/connection counts in
`ChaosClothingSimulationCollider.cpp:90`; the private collision-authoring
commandlet's 32/16 guards are not demonstrated engine limits. Do not confuse
this with CSS attachment limits or claim that raising a count fixes the outfit.
Complex skinned collision also needs solver compatibility checked before any
implementation: that same collider excludes complex shapes for its legacy
solver. No solver mode, helper count guard or game setting changed here.

Continue with targeted coverage of the remaining thigh/body witnesses and
compatible cloth deformation, preserving the rendered garment. Do not adopt
either center-weight candidate solely from its target-separation result.
Eve's `work/slipstream-cloth-study5/receipt.json` hashes 180 evidence files and
snapshots 58 small reports, recipes, logs and images. All six commandlets exit
zero and both private projects retain 53 protected dependencies. Large captures
and assets remain hash-referenced at their retained source paths. The installed
Peony repair, core, saves and settings remain unchanged.

## Collision connections and internal limiting surfaces

`slipstream-surfaceconnections1` tests connecting the retained spheres without
increasing the number of collision objects. Of 120 same-bone, noncontained
pairs, 36 pass sampled swept-ball containment and reference cloth clearance.
Sixteen selected connections produce 30 collision objects. Median/p95/worst
sampled body gaps improve only from 1.331/2.977/4.525 cm to
1.264/2.917/4.312 cm. This is insufficient coverage progress to adopt the
candidate. It has not been imported or simulated. The analytic swept-ball
distance matches independent one-dimensional minimization within 0.00000266 cm
on 200 points; that does not establish native tapered-capsule parity.

The preceding Peony regional-surface method suggests another cause of undersized
spheres: nearest surfaces can belong to an internal body overlap. The retained
32-sphere fit has ten limiting nearest surfaces whose outward samples remain
inside the body under the existing three-ray classification. Three clear
examples are sphere 3 on the left thigh limited by right-thigh face 5276,
sphere 13 on the right thigh limited by left-thigh face 1429, and sphere 29 on
the right thigh twist limited by left-thigh face 1379. This is evidence of an
internal-surface fitting problem, subject to the source topology/classifier
limits, not proof that it explains every clipping point.

`slipstream-regionalcollision1` keeps the same centers, bones and sphere count.
It bounds thigh radii against their own anatomical surface and checks 168
directions at seven poses against the whole body, including a 0.1 cm margin.
Five radii change, by approximately 0.589, 0.119, 0.007, 0.134 and 0.114 cm.
This is sampled containment, not a continuous sphere or full morph proof.

Native8 imports that asset into a private authoring path and tests the unchanged
original-weight, original-travel mesh. Captured native sphere definitions match
the authored data within 0.000000096 cm, and all 58 staged source assets remain
unchanged. Sprint and secondary sprint each fail seven strain frames; anchors
pass. Frame 119 still has 93 inward body witnesses, worst 2.495 cm. The candidate
remains unaccepted. Do not characterize the difference from the previous
eight-frame results as a proven performance or stability improvement.

A separate native8 diagnostic removes animation drive only, retaining gravity,
body collision, self-collision and cloth simulation. It fails ten frames in
both captures and retains 84 inward witnesses, worst 2.376 cm. Reject disabling
animation drive as the repair. Saved mesh/configuration data remain unchanged.

The next fitting work should address the uncovered body contour rather than
repeat a sphere-count, travel-limit or animation-drive sweep. The earlier Eve
regional hull study is available under `CustomShellSystem/work/eve26/region-hulls`;
its large regional hulls also overfilled parts of the body, so they are a
reference to inspect, not a ready replacement. A more local shape must pass
posed containment and cloth clearance before native testing. Rest render
mapping, other garment fit, body morphs, full character assembly and release
validation remain open.

Evidence is frozen in Eve's `work/slipstream-cloth-study6/receipt.json`.
Native creation and both motion batches exit zero; negative fit verdicts remain
recorded. No game package, core, save, profile or solver mode was changed.

## Rigid thigh level-set comparison

`slipstream-levelset1` samples the accepted full body's occupied volume in each
thigh's local space, at 0.5 cm spacing. It retains the intersection of the
reference pose and recorded sprint frames 0, 36, 72, 108, 119 and 132. Oriented
ray counts preserve overlapping body regions that odd/even parity would erase;
all three ray axes must agree. A single cube and overlapping cubes pass exact
occupancy controls. The body has some open rays and axis disagreements, so
this remains a sampled authoring method, not a watertight-body proof. Thirty-two
interior probes per shape and pose also agree with independent solid-angle
winding classification.

Two 50 by 46 by 42 grids use cell-center signed distances, generated with SciPy
1.17.1. The initial 0.15 cm inset reaches 80/93 recorded native8 clipping
witnesses but fails boundary containment and is rejected. Increasing the inset
to 0.5 cm allows more room for the 0.5 cm voxel's spatial uncertainty. That candidate
reaches 74/93 witnesses and passes 512 boundary probes per shape across all
seven poses, 7,168 checks in total. Minimum reference cloth clearance is
0.754 cm left and 0.908 cm right, above the 0.3 cm contact thickness. Body
morphs, intervening gait frames and continuous containment remain unverified.

The exact local UE 5.6.1 source supports rigid level sets in its existing
legacy cloth solver. `ChaosClothingSimulationCollider.cpp` includes them among
simple colliders and wraps each grid in its bone-local transform. This does
not require changing `p.ChaosCloth.UseLegacyEvolution`. `ArrayND.h` uses Z as
the fastest index, matching the exported C-order values; `UniformGrid.h`
locates samples at cell centers. In this engine version,
`FKLevelSetElem::GetLevelSetData` returns X/Y dimensions swapped, while
`GridResolution()` returns the actual dimensions. Native readback must use
the latter and record both, rather than silently transpose the values.

`slipstream-cloth-native9` is a private copy of native8. It retains the 32
regional spheres and adds these two rigid thigh grids. It uses the unchanged
original-weight, original-travel garment and existing cloth settings. The
private helper adds explicit grid inspection and native distance samples.
Its first compile failed on the case-sensitive `Chaos/Levelset.h` include;
the corrected compile passes. All 59 copied source assets pass initial hash
comparison. This is an authoring trial, not a package installed in the game.

Fresh native9 load preserves both 96,600-cell grids within 0.000000474 cm.
The 1,522 native distance queries at cell centers agree within 0.000000361 cm.
The 32 original sphere definitions are preserved within 0.000000096 cm. The
motion capture confirms 32 spheres and two transformed level sets in the actual
cloth solver, not just in the saved physics asset.

Both 145-frame sprint captures retain all 188 anchors within 0.000026 cm, but
fail the strain screen in 30 and 31 frames. Worst extensions are 2.412 and
2.440 cm. Frame 119 has 34 uncovered inward witnesses, worst 1.724 cm, compared
with native8's 93 witnesses and 2.495 cm. The matched render replay and rear
review still show visible skirt/body intersection. Better coverage does not
make this an accepted garment. Rest render-mapping error is unchanged at
1.570 cm worst and 0.545 cm p99.

Native10 reuses the already checked center-weight mesh from native6 with the
same new collider. It changes neither travel limits nor cloth configuration.
Both anchor checks pass, but strain fails in 39 and 41 frames, with worst
extensions 3.303 and 3.184 cm. Frame 119 retains 43 inward witnesses, worst
2.000 cm. Reject this combination too. All 59 staged source assets in each
project remain unchanged. Creation, fresh readback and both motion batches
exit zero, which establishes execution only. Timing recorded during these
authoring runs is not controlled game performance evidence.

A follow-up checks 512 boundary points per thigh at eight newly failing
frames: 6, 22, 38, 55, 88, 102, 120 and 135. Frames 88 and 120 expose the
seven-pose fitting limit. Each has 35 outside left-thigh samples, up to
0.599 cm, and 24 outside right-thigh samples, up to 0.443 cm. The other six
sampled poses pass. Original fit-source and new native9 local bone snapshots
match exactly throughout all 145 frames, ruling out a mismatched pose input
for this comparison. These witnesses do not explain every remaining strain
failure, but they reject the current rigid surface's full-stride containment.

Next: bound these local volumes over the full recorded stride before further
native trials, rather than enlarge them or repeat the center-weight change.
Keep the six initially passing failure poses as separate contact/constraint
witnesses. Full morph coverage, other gaits, the residual render mapping and
the remaining outfit work still need validation. The current rigid-level-set
candidate is rejected; the method is not yet established as a shipping fix.

Evidence is frozen in Eve's `work/slipstream-cloth-study7/receipt.json`.
It hashes 161 files and retains 132 small snapshots. After all editor jobs
finished, the native9 rebuildable Intermediate cache was removed, reclaiming
373.9 MiB. Its source, successful module, assets and evidence remain retained;
the pinned module hashes pass again after cleanup.
Installed packages, CSS runtime, saves and profiles remain unchanged.
