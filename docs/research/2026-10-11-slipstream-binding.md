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
