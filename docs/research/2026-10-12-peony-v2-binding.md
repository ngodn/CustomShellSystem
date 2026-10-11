# Peony V2 body controls and target binding

Status: private authoring study, not installed or accepted for release. Continue
from `binding-footwear3` for body-control and motion repairs. The preceding neutral
baseline remains `neckline-contact3`; the additional glove edits still have
unresolved whole-surface contact. All supplied palette templates and the
black-and-white floral Hydrangea default remain required.

Work root: `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/work/peony-v2-source1`,
relative to `/home/eins0fx/development/mods/msII`.

## Visible glove witness

The source reviewer now records pixel rays, original body-face identities and
camera transforms. Before/after comparisons reuse the same recorded cameras.
The review's existing heel-coverage deletion changes body polygon numbering,
so `CSS_review_body_face` records the original reference index before that
render-only deletion. Do not treat an evaluated polygon number as an import
face identity without this mapping.

In the left side view of `glove-fit5`, pixels `(300,463)` and `(297,466)` first
hit accepted-body faces 13867 and 13877. Skin sits in front of the glove shell
and knuckle plate. The shell witnesses include slot-3 faces 1973 and 2054,
source triangles 26952 and 27033; the plate is component 48. This isolates a
visible knuckle breakthrough rather than merely counting all internal contacts.

`glove-local-contact2` adds body-vertex constraints at reverse body-edge/glove
intersections to the local-contact recipe. It keeps the same 0.5-mm step limit,
6-mm total bound, welded source seams and triangle guards. Maximum movement
is 4.237 mm left and 5.268 mm right. In the identical camera, both witness
pixels now hit glove geometry first, and the inspected side view no longer
shows that patch. Front/rear views were also inspected.

This is a limited repair. Main-shell garment/body edge crossings remain
135/168 left and 136/163 right. Thumbs remain at zero; wrist closures still
intersect. The 98-object preservation check passes. The glove copy is retained
as an input to binding, not declared fully accepted.

## Binding implementation

`bind_peony_v2_study2.py` uses the established Eve binding helpers on separate
copies of the V2 garment objects. It preserves all original objects and the
accepted body. The copy contains 18 retained garment sections, the existing
unaccepted heeled lining and a bound accepted-body reference. Source skin,
removed pockets/robot ornaments, alternate shoes and optional nails are not
included as bound garment sections.

The rig uses the measured 379-bone body import. Maximum Blender/import bind
matrix difference is 0.000021652; joint-head difference is zero. This does not
replace or prove compatibility with a released shared skeleton.

Primary fabric uses matching body-surface weights and the six existing body
controls. Glove queries follow the corresponding digit/hand region. Separate
trim follows its primary fabric; glove glow follows the gloves and heel glow
follows the heels. Retained heel geometry and lower ankle accessories keep
their mapped foot/leg drivers. The restored ankle accessories do not yet have
transferred body morphs; their continuity still needs review. Used garment
vertices have at most seven influences before export.

Body donors exclude internal anatomy, retaining the 333 exterior detail faces
already identified during hip fitting. The first collar query exposed a donor
selection omission: jacket vertex 828 is 0.574 cm from accepted-body face 50443,
which uses neck/head bones but belongs to material `Eve Face`. The restricted
tree had placed its nearest donor over 8 cm away. The corrected selection
includes 1,086 neck-weighted faces from that material, rather than broadening
to every facial surface or raising the distance bound.

The first saved binding also reproduced the known Blender shape-key default
problem: new controls were saved at one. The rest checker rejected it. A
separate correspondence check caught single-precision barycentric sums up to
1.39e-7 from one. `binding2` explicitly saves all new controls at zero and
normalizes barycentric values in double precision. Both checks now pass.

## Verification and remaining defects

- Fresh reload compares 20 baked surfaces with their source evaluation and
  verifies neutral armature evaluation does not change them.
- A separate before/after check preserves all 98 original mesh objects,
  including positions, topology, UVs, weights and existing shape coordinates.
  It checks the saved six-control correspondence and normalized target weights.
- Neutral, all-six-maximum, walking frame 36 and sprint frame 12 were rendered
  from front, rear and side. All twelve views were inspected.
- Walking skinning agrees with an independent calculation from the retained
  game's pose and original import matrices within 0.000432 cm. Sprint agrees
  within 0.000333 cm. These are pose-replay checks, not live secondary physics.

The binding is not ready for installation. Walking exposes a skin gap across
the rear collar/jacket join. Footwear coverage remains unfinished, with visible
toes and lower-leg openings in these diagnostic views. Material/stocking
reconstruction is still pending, so these are not final palette previews.
The all-maximum view also needs closer checks at the breast transition and
waist/hip joins. Per-face area extremes remain substantial, including jacket
ratios up to 40.12 in walking and underwear ratios up to 40.01 in sprint.
Separate thin/internal faces from visible defects before attributing a cause.
Do not equate correct linear skinning with acceptable deformation.

The collar comparison below addresses the traced rear opening. Next check
individual body controls, seam continuity, footwear coverage and further motion.
Keep the measured glove improvement but
its remaining contact work open. Full materials, all palettes, modular controls,
physics, export/cook readback, gameplay and coordinated releases remain pending.

## Reproduction and cleanup

`binding2-receipt.json` freezes recipes, checks, rendered reports and dependency
hashes. Frozen scripts require the authoring `tools` directory on Python's module
path. Authoring files are outside the CSS Git repository; the docs commit does
not contain those files.

| Artifact | SHA-256 |
| --- | --- |
| `binding2.blend` | `57c257b4924010173f6094efccda68af1a444acb244a542dd844785c1a6b6455` |
| `bind_peony_v2_study2.py` | `da80f3060f137bccb34603901eda278d304f36d5a4b5dabf0da4d200b0a00049` |
| `glove-local-contact2.blend` | `8023319b3cb1fb6105ee5f748083950c45eacafb246a8d7a0a77cc9c0b1c622d` |

The failed `binding1` recipe was reconstructed and matched to its recorded
SHA-256 before freezing. Its blend was copied to
`/run/media/eins0fx/theark/CSS/peony-v2-rejected-20261011/binding1.blend`, verified,
and removed locally, reclaiming 90,197,108 bytes. Its adjacent receipt, local
correspondence data, failure logs and corrected recipes are retained. No package,
game setting, save, camera or character input was changed.

## Rear collar refinement

The walking opening is skin crossing a continuous jacket triangle, not a missing
seam. Pixel rays in `review-binding2-collar-probe1` identify jacket faces 3257,
2742 and 3263, source triangles 70750, 70235 and 70756. All belong to slot 19,
component 0. Their coarse neck/spine weight interpolation differs from the
accepted body's interpolation beneath the triangle interiors.

`collar-walk36-correspondence1.json` maps each posed ray hit back to its neutral
triangle. Sampling body weights at that neutral interior point predicts this
change in signed distance to the posed body:

| Rear pixel | Original distance (cm) | Body-weighted prediction (cm) |
| --- | ---: | ---: |
| 430,192 | -0.007671 | 0.302046 |
| 448,192 | -0.495391 | 0.245648 |
| 416,190 | -0.157050 | 0.199992 |

Those are point predictions. `binding-collar1` then tests actual mesh refinement:
split shared edges longer than 6 mm around the rear neck transition, preserving
the original neutral triangle surfaces. The known inner collar island is not a
refinement driver. Adjacent triangles share edge splits. New points receive body
weights and all six body-control deltas. Original points and their weights remain
unchanged. Original source meshes are untouched; only the private bound jacket
changes. Other bound garments, including shoulder trim, are unchanged.

The jacket grows from 2,755 vertices / 4,477 triangles to 6,425 vertices / 11,696
triangles. This adds 3,670 vertices and 7,219 triangles; export influence limits
and runtime performance still require review. UVs and inherited source-face
identities remain intact. Appended points have source-vertex ID -1 and explicit
parent-face/barycentric provenance rather than invented original identities.

The same three walking pixel rays now hit jacket fabric before skin. Front,
rear and side views were inspected for walking frame 36, sprint frame 12 and
all six controls at maximum. The rear walking band is covered. These nine views
do not establish clearance through an entire animation or all control mixtures.
The full-jacket area extreme of 40.12 remains elsewhere; the refinement is not a
claim that every jacket triangle is repaired. Footwear/body coverage and other
previously recorded defects remain visible and unresolved.

`binding-collar1-preservation.json` independently reloads both files and checks:

- All 117 other mesh objects unchanged, including the original source meshes
  and accepted body.
- All 2,755 original jacket vertices, weights and shape coordinates unchanged.
- Child triangles cover their original parent surface without reversed winding;
  maximum parent area difference is 1.047e-9 square metres.
- Neutral interpolation differs by at most 0.000005961 cm after float storage;
  UV difference is zero.
- New body weights and six saved morph correspondences match. The largest
  morph difference is 0.000011901 cm, below two float32 coordinate steps.

The first recipe invocation stopped before saving because the attribute copier
handled integer source IDs but not source normal/tangent colors. The corrected
recipe also interpolates point colors and custom corner normals. The first
preservation invocation used an absolute morph tolerance smaller than the
separate Basis/key float32 rounding bound; the final check derives its bound
from two stored-coordinate steps and reports the actual residual.

`binding-collar1-receipt.json` freezes the recipe, inspector, verifier, reviewer,
dependency hashes, saved provenance and rendered evidence. The blend SHA-256 is
`0a3df186276abe52b602b0f08d7d18568068d5b187e3e6c59aa67f2f8429a503`.
Authoring files remain outside CSS Git. The installed game files, saves and
settings are unchanged. No V2 trial is installed and no game test is requested.

## V2 closed-boot coverage and bare-foot restoration

The binding reviewer still included the rejected heeled lining. This caused the
exposed toes and lower-leg skin in its renders. The earlier closed-boot study
already established that the source-preserving heels should use reversible
covered-body visibility. The V2 binding now excludes that lining from the active
garment list. It remains in the private blend as an unchanged hidden diagnostic
object, not an export part.

`prepare_peony_v2_footwear3.py` retains the original heel, ankle-connection and
calf geometry. V2 source slots 4, 17 and 30 belong to the boot switch. The script
uses accepted-body import face identities to create covered material aliases on
the separate bound body, preserving the accepted source body itself. The bare
body, body controls, garment geometry, weights and UVs are unchanged.

The unmaterialed body study initially has no material slots. The first recipe
invocation stopped before saving when it tried to copy one. The working recipe
reconstructs body-face material ownership from the verified accepted import
before creating aliases. It does not reuse the retired garment's section IDs.

Two coverage candidates were rejected before retaining `binding-footwear3`:

- `binding-footwear1` reused the old 5,328-face, 42-cm partition. It removes
  skin visible through the V2 calf openings. In the front camera, pixels
  `(200,312)`, `(200,324)` and `(200,336)` hit the far side of the calf instead of
  body faces 5109, 5108 and 5097. Hidden-body rays identify those missing surfaces
  at heights 39.65, 38.80 and 37.94 cm.
- `binding-footwear2` lowers the partition to the 27-cm start of the previously
  fitted outer calf. The openings are filled, but hiding only triangles wholly
  below the cutoff leaves two crossing triangles exposed. Pixels `(244,520)`
  and `(655,520)` hit body faces 6257 and 3172 through the closed calf shell.
  Their vertices span 24.36 to 27.25 cm. The breakthrough occurs at 24.85 cm,
  below the fitted outer-calf region.

`binding-footwear3` includes triangles crossing that cutoff in the closed-boot
coverage. It hides 4,944 body faces, preserving 384 faces that the old partition
incorrectly hid. The three calf-opening witnesses now hit visible skin; the two
closed-calf witnesses hit garment faces 1536 and 2828. Both use the same recorded
neutral camera. This repairs the visibility boundary without expanding the shoe
or deforming the accepted body.

| Source body material | Covered faces | Private alias slot |
| --- | ---: | ---: |
| Eve Covered Feet | 218 | 24 |
| SkinCovered_1 | 3,890 | 25 |
| SkinCovered_6 | 836 | 26 |

These are private body-copy slots, not final catalog/export slots. With boots
shown, 56,870 body triangles remain visible. Hiding boots restores all 61,814
body triangles and removes the three boot objects. The rejected lining is absent
in both states. Measured boot-dependent placement is 9.738332 cm above the bare
body placement; wire this to the established CSS toggle-height rule during
catalog assembly, preserving the manual Ground Height override.

`binding-footwear3-preservation.json` independently reloads both copies. It checks
geometry, weights, UVs and shape coordinates across all 118 meshes, verifies
material-face ownership against the original import, and checks both visibility
sets and the sole-height calculation. Fifteen front/rear/side views were inspected:
neutral, all-six-maximum, walking frame 36, sprint frame 12 and boots hidden.
The closed heels retain their design, the calf openings retain skin, and the
bare legs/feet restore. This is still a limited offline check, not full-animation,
material, contact-with-terrain or gameplay acceptance.

Continue from `binding-footwear3.blend`, SHA-256
`844fca930c9e9c6b0f9bd509564d8441fe63a76b495d66619ca2f1f6f723130f`.
`binding-footwear3-receipt.json` freezes 45 recipes, reports, renders and
dependencies. The two rejected blends were copied to the external CSS archive,
hash-verified and removed locally, reclaiming 181,319,135 bytes. Their small
receipts, frozen recipes and diagnostic evidence remain local.

Next complete the individual body-control and seam/contact checks, then carry
the V2 face partition into the actual export and catalog. Remaining glove
contact, all supplied materials/palettes, physics, cooking and coordinated
release work remain open. No V2 trial is installed; game files, saves, settings,
camera and character input are unchanged.

## Individual body controls and jacket bridge

The six controls were rendered separately from `binding-footwear3`. The lower
garment stays close in the inspected hip, glute and thigh views; this does not
clear its internal contacts or moving seams. The torso inspection found two
separate problems: maximum breast size intersects the main corset, and the
jacket's front bridge follows the wrong garment.

`body-controls1.json` tests the inspected 2,364-face corset UV island against the
complete accepted body in both edge directions. Maximum breast size produces
21 garment-edge/body crossings and seven body-edge/garment crossings. All six
controls at maximum produce 16 and six. The witnesses cluster along the center
front and one area below the left cup. Neutral, the other individual controls,
all controls at one half and the previously reported breast/glute values have
no crossings in this limited surface test.

`binding-breast1` changes only `PBMBreastsSize` on bound source slots 24 through
27. It uses the earlier guarded continuous contact field against the morphed
body, with a fixed lower waist. Neutral fit, topology, UVs, weights, skeleton,
body, the other five controls and footwear partition remain unchanged. The
largest correction is 1.63335 cm on the main corset and 0.42930 cm on its front
trim. Main-corset triangle areas remain between 0.72746 and 1.38830 of their
previous maximum-breast areas; no tested triangle reverses. The sampled margin
is at least 0.29629 cm on the selected main fabric.

`body-controls2.json` finds no crossings in either edge direction for 16 cases:
neutral, each individual maximum, all-half, all-maximum, the reported
breast/glute values, and breast values 0.25/0.5/0.75 with the other controls at
zero or maximum. `binding-breast1-preservation.json` independently reloads both
files and verifies all 118 meshes, permitting only the four specified shape
changes. This is an explicit garment-shape correction after body correspondence;
the corrected shape is no longer solely the original interpolated body delta.
The front cord already hangs below the cups at neutral size. Its compression
at large breast sizes still needs trim review, separate from the repaired body
crossings. The maximum-breast render is not full visual acceptance.

Continue cumulative fitting from `binding-breast1.blend`, SHA-256
`edaef0c389fb9083fe4ad89e69cab53154dda20d8e379b2f1fc924b0b690ded1`.

The bridge investigation identifies source slot 20 as the short front jacket
strap and its two buttons: three components, 294 vertices, 456 faces, heights
148.78 through 151.35 cm. Its original binding incorrectly used corset slot 24,
12.15 through 15.82 cm away for the main strap. The jacket is at most 2.563 cm
away. `bridge-donors1.json` records each component's bounds and distances.
The live binder now assigns slot 20 to jacket slot 19; the frozen old binder
remains available to reproduce the defect.

Two private bridge studies remain rejected for complete motion acceptance:

- `binding-bridge1` replaces every bridge vertex's weights and six controls
  with the nearest exterior jacket triangle's interpolation. It preserves
  neutral geometry and stops the breast slider pulling the strap upward, but
  creates uneven folds in the walking pose.
- `binding-bridge2` uses two button-center anchors on the jacket. Each button
  shares one binding; the strap blends laterally between them. This improves
  the walking outline. The sprint front view still shows intersections at the
  strap and attachment area, so it is a diagnostic candidate, not the next
  accepted cumulative baseline.

The identical-camera sprint rays make this rejection concrete. At `(547,399)`,
body face 6956 is in front of bridge face 344 by 0.04610 cm along the camera ray.
At `(449,393)`, jacket face 8736 is in front of bridge face 229 by 0.05076 cm.
The third probe, `(567,418)`, misses the bridge and cannot establish a bridge
intersection. `review-bridge2-sprint-probes/report.json` retains the hits and
their weights for the next attachment-clearance correction. Do not treat every
skin-colored pixel around the strap as a clipping witness.

Both saved-file bridge checks preserve the other 117 meshes and all neutral
geometry, UVs, materials, skeleton and footwear partition. They independently
reconstruct all 294 vertices' new weights and six shape offsets from the jacket
donors. These checks establish correspondence, not collision-free movement.
The diagnostic torso views also expose unresolved collar/shoulder irregularities
and sprint hip-trim separation. The previous collar repair only cleared its
three recorded witnesses; it did not clear the whole seam.

The frozen body-control and bridge recipes, checks and images are indexed in
`binding-body-controls1-receipt.json`. No V2 trial is installed. All supplied
palette templates and the black-and-white floral default remain required;
material matching, export, physics and release work are still open.

Rejected bridge snapshots remain local for diagnosis. The requested external
archive reports only 72 KB available during this checkpoint, so neither snapshot
was moved or deleted. Preserve the frozen recipes and evidence when archive
space becomes available; do not remove unrelated files to make room.

## Bridge weight and clearance correction

The bridge's source weights primarily follow the neck and upper spine. Copying
the fitted jacket weights introduced shoulder influence into this short strap.
`bridge-clearance1` shows that moving the transferred binding forward by up to
6 mm still leaves sprint jacket crossings. Moving along the anchor normals
also fails (`bridge-clearance2`). These are rejected clearance-only controls.

Restoring the original V2 bridge weights through the existing semantic bone
mapping, while retaining its two-anchor body-control offsets, clears the tested
contacts with a 6-mm forward translation. The finer 4.5/5/5.5-mm comparison still
finds moving jacket crossings at 5.5 mm. Six millimeters is the smallest passing
tested offset, not a proven optimum. Translating Basis and all six shapes
together preserves their deltas and the bridge's shape.

The saved `binding-bridge3` passes `bridge-clearance6`: zero edge crossings in
both directions against the complete body and jacket in neutral, all controls
at maximum, walking frame 36 and sprint frame 12, with each moving pose tested
at zero and maximum controls. The saved-file check independently verifies all
294 restored vertex weights and the uniform translation, preserving the other
117 meshes, topology, UVs, skeleton, body and footwear partition. Maximum
weight error is 2.97e-8; translation error is below 9.84e-8 cm.

Matched-camera front and side renders retain the bridge through those motions.
The two earlier sprint pixel witnesses now hit the bridge first: `(547,399)`
places it before body face 6956, and `(449,393)` before jacket face 8736. This
accepts this local repair for continued fitting. It does not clear every
animation, coplanar contact, shoulder seam, front cord or hip trim.

A comparison restoring the whole jacket's source weights (`binding-bridge4`)
clears the bridge test with 4 mm but visibly exposes skin through the shoulder
in sprint and gives a poor walking underarm fold. It is rejected. Keep the
fitted jacket weights and restore source weights only on the bridge. The live
binder now preserves slot 20's source weights while using jacket correspondence
for its morphs; the later two-anchor and clearance recipes are still required
to reproduce the complete candidate.

Continue from `binding-bridge3.blend`, SHA-256
`6c1d60c03b9dc91b3b5021e2f668ea025834aa69f626473639d91ac3dccec6a9`.
`binding-bridge3-receipt.json` freezes 60 evidence/recipe/dependency files and
records three blend hashes. The rejected, regenerable `binding-bridge4.blend`
was removed after hash verification, reclaiming 90,633,321 logical bytes.
Its input, recipe, metadata and rejection renders remain local. It was not
archived because the requested external volume is full; cleanup is recorded
in `binding-bridge4-cleanup.json`.

The shoulder seam and compressed front cords remain visibly unresolved. No V2
trial is installed, and no game test is requested at this checkpoint.

## Shoulder trim diagnosis

The retained shoulder decorations still used their pre-refinement jacket
binding. A matched sprint ray at `(337,237)` in
`review-bridge3-shoulder-probes/report.json` hits jacket face 4589 before trim
slot 23 face 967. Their depth difference is 0.11222 cm. This is a separate
trim/jacket overlap, not the repaired front bridge or exposed body.

`binding-shoulder1` rebinds slots 22 and 23 to the refined jacket, excluding the
known 150-face inner fan. Neutral geometry stays fixed in the recipe. The old
pixel witness now hits trim first, and slot 22's body crossings disappear in
the six audited cases. Slot 23 still intersects the body and jacket during
motion. Rebinding alone is not sufficient.

`binding-shoulder2` adds 957 vertices to slot 23 by splitting edges longer than
6 mm, producing 2,113 vertices and 3,636 faces. New points use the refined
jacket's weights and morph offsets. This retains the neutral parent surfaces
in the construction, but does not by itself remove the moving contacts. Edge
counts across different topology are not a measure of improvement.

The direct nearest-jacket-normal offset is rejected. At 3 mm,
`binding-shoulder3` reverses six neutral triangles and seven body-tone triangles;
walking also retains body crossings. Its maximum neutral area ratio is 5.8268.
Do not promote that file or repeat a discontinuous normal offset.

The next diagnostic copy, `binding-shoulder4`, averages the jacket-normal field
within a 12-mm compact spatial kernel before applying a 4-mm field amplitude.
The field is not renormalized, so actual displacement is bounded by 4 mm.
The same displacement is applied to Basis and all six keys. No triangles
reverse in the construction check: area ratios across the seven shapes remain
within 0.84979 through 1.40886, and normal cosine is at least 0.96815.

`shoulder-contact-shoulder4.json` reloads the saved file. Both trim slots have
zero body crossings in both edge directions across neutral, all-maximum,
walk36 and sprint12 at zero and maximum controls. Slot 23 also clears the
jacket at neutral and all-maximum. Moving trim/jacket crossings remain:

| Pose | Slot 22 vs jacket | Slot 23 vs jacket |
| --- | --- | --- |
| Walk36 | 91 / 46 | 73 / 65 |
| Sprint12 | 145 / 36 | 77 / 73 |
| Walk36, maximum controls | 92 / 48 | 55 / 53 |
| Sprint12, maximum controls | 141 / 38 | 74 / 69 |

Each pair counts trim-edge/jacket then jacket-edge/trim hits. Complete meshes
include undersides and attachment details; classify these witnesses before
further displacement. Neutral/back and moving/front/side views improve the
plate coverage but do not establish complete seam acceptance. The two later
sprint probes `(368,194)` and `(374,274)` hit jacket faces 5089 and 9287 and miss
both trim slots. They locate visible jacket folds, not trim clipping proof.

Retain `binding-shoulder4.blend` for diagnosis, SHA-256
`ca7b295dbc57582be5cc7b59dc4115a7f1dfd25f0612b28bc7f7b568495195dc`.
Independent preservation checks for the three shoulder-edit stages and
classification of the remaining jacket contacts are still required. The
accepted cumulative baseline remains `binding-bridge3`, not this candidate.
The next correction must follow those witnesses rather than increasing a
global offset again.

`binding-shoulder4-receipt.json` freezes 60 files and records the five input/
candidate blend hashes. The rejected, regenerable shoulder3 blend was removed
after hash verification, reclaiming 90,644,521 logical bytes. Its recipe,
input, spacing study and metadata remain; no external copy was claimed.
See `binding-shoulder3-cleanup.json`. No game file was changed.

## Coupled shoulder attachments

Independent reload checks now pass for shoulder1's binding, shoulder2's
refinement and shoulder4's spacing. They verify the saved coordinates, weights,
six controls, topology and UVs against their respective inputs, with all
unrelated meshes, body, rig and footwear preserved. Shoulder2 starts from
1,808 plate faces and 1,156 vertices, adding 957 vertices. The refinement
preserves the parent surfaces and original vertices. These checks resolve the
preservation questions above; they do not resolve moving jacket contact.

The contact classifier tests each recorded edge witness from 24 directions
against the assembled opaque surfaces. A second comparison maps refined faces
back to the untouched source. Shoulder4's visible neutral cord contacts all
match original attachment overlaps, but many walking and sprint witnesses do
not. They cannot all be dismissed as hidden or inherited from the source.
See `shoulder-contact-visibility1.json` and `shoulder-source-contacts1.json`.

Restoring source weights on the plate alone reduces several moving contacts,
but `binding-shoulder5` visibly separates the cord ends in sprint. Across 379
cord vertices originally within 1 mm of the plate, its maximum sprint gap is
1.44802 cm. Restoring source weights on the cords themselves worsens jacket
contacts. Neither comparison is an acceptable standalone correction.

`binding-shoulder7` keeps the plate's source weights and couples the cord ends
to its fitted surface, weights and body controls. Original-source distance
defines the attachment region: full influence within 1 mm, smoothly falling
to zero at 8 mm. A 12-mm spatial kernel smooths the neutral displacement field
to retain the cord cross-sections. This affects 1,275 of 3,311 cord vertices.
The unsmoothed shoulder6 variant is rejected for 33 reversed faces.

The saved shoulder7 preservation check passes. The other 117 meshes, body,
rig, footwear, topology and UVs remain unchanged from shoulder5. Maximum
neutral displacement is 0.358273 cm. Across Basis and all six keys, the minimum
normal cosine is 0.926179 and area ratios stay within 0.864703 through
1.223512. No faces reverse. Body-control correspondence agrees with the
accepted body to at most 0.000021763 cm.

The same 379 attachment samples now have maximum gaps of 0.166997 cm in
neutral, 0.161753 cm in walk36 and 0.159928 cm in sprint12, all at maximum
controls. Sprint's median gap is 0.031504 cm. The reviewed neutral front/back
and maximum-control moving front/side views retain connected cords. These are
finite attachment samples and offline poses, not full animation acceptance.

Both trim slots remain clear of the body in both edge directions across all
six saved cases. Jacket contacts remain. The table gives visible witnesses
followed by those sharing an exact original-source contact face pair:

| Case | Cord slot 22, visible / source match | Plate slot 23, visible / source match |
| --- | --- | --- |
| Neutral | 4 / 4 | 0 / 0 |
| Maximum controls | 5 / 4 | 0 / 0 |
| Walk36 | 14 / 5 | 28 / 21 |
| Sprint12 | 4 / 0 | 40 / 9 |
| Walk36, maximum controls | 8 / 4 | 8 / 7 |
| Sprint12, maximum controls | 13 / 0 | 39 / 14 |

See `shoulder-contact-visibility2.json` and `shoulder-source-contacts2.json`
for exact edge/face witnesses. Matching source face pairs identifies existing
attachment overlap, not an automatic visual pass. Conversely, new face pairs
need inspection before displacement. The classifier uses opaque geometry,
24 view directions and a 0.02-cm depth tolerance; it does not test material
alpha, coplanar overlap or the full motion range.

Continue the shoulder investigation from `binding-shoulder7.blend`, SHA-256
`c3cc027f967a583b7dbb5d5022fd2bcbb095b366d129fb215dd44d401f4c2d45`.
The accepted local baseline remains bridge3 until the remaining moving trim
contacts are resolved. Main jacket folds, front cords, hips, gloves, source
materials, all supplied palettes and the remaining export/runtime work are
still open. No V2 trial is installed.

`binding-shoulder7-receipt.json` records 113 recipe/evidence files and four
blend hashes. The rejected shoulder6 blend was removed after hash and open-file
checks, reclaiming 90,635,362 logical bytes. Its input, frozen recipe and
metadata remain. No external archive is claimed because that volume is full;
see `binding-shoulder6-cleanup.json`.

## Lower-neck donor correction

The shoulder7 close-up identifies a separate jacket defect. In sprint12,
side pixel `(367,448)` hits jacket face 11002, whose vertex 2559 has 65.65%
neck influence while adjacent vertex 6022 has 18.81%. Pixel `(366,527)` hits
jacket face 11495. The jagged ridge is jacket geometry, not a shoulder cord.
See `review-shoulder7-close-probes/report.json` for the full rays and weights.

The original binder excluded the accepted body's `Eve Head` material, despite
that section containing part of the lower neck. It allowed `Eve Face` neck
faces instead. The collar refinement later used the complete body surface,
so new vertices could select the missing Head faces while original vertices
remained pinned to the more distant Face seam. At vertex 2559, the original
donor is body point 16841 at 1.990247 cm distance. Head face 7683 supplies a
closer donor at 0.455528 cm, spanning body points 2267, 1957 and 2268.

`binding-neck-donors1` adds the 228 Head-section faces with neck influence
above 0.05 to this jacket comparison. It changes only vertices for which
that section provides a closer donor: 185 jacket vertices' weights and
body-control offsets. Neutral coordinates remain fixed. The matched sprint
close-up removes the jagged ridge. Maximum-control sprint front and walking
back views were also reviewed. The live binder now includes those Head neck
faces alongside the existing Face neck faces; the frozen prior recipe remains
available. This does not imply a fresh complete bind has been accepted.

The independent saved-file check passes across all 118 meshes, preserving
neutral coordinates, topology, UVs, accepted body, rig, footwear and unrelated
meshes. Unchanged jacket vertices retain their weights and keys. Maximum
weight error against recorded body donors is 4.54e-8; maximum body-offset
error is 0.000005937 cm. No key triangles reverse. BodyTone's local area ratio
range is 0.631207 through 2.260643 relative to the prior incorrect mapping;
the other five controls are unchanged. These are correspondence and
preservation results, not a blanket distortion pass.

`neck-donors1-contact-comparison.json` maps the retained 2,513 original exterior
jacket faces through the refinement. In all six cases, the exterior faces
affected by the repair have zero remaining body crossings in both edge
directions. No new exterior contact edge/face pairs appear elsewhere.
Remaining exterior contacts outside the repaired region are still open:

| Case | Before, jacket/body edges | After, jacket/body edges |
| --- | --- | --- |
| Neutral | 0 / 0 | 0 / 0 |
| Maximum controls | 0 / 0 | 0 / 0 |
| Walk36 | 280 / 51 | 90 / 23 |
| Sprint12 | 325 / 55 | 110 / 27 |
| Walk36, maximum controls | 262 / 52 | 82 / 28 |
| Sprint12, maximum controls | 325 / 52 | 110 / 29 |

Complete jacket meshes also include unclassified internal contacts. This
comparison does not clear them or establish full-motion coverage. Both shoulder
trim slots remain clear of the body in these six cases, but plate/jacket
contacts increase in several moving cases because the dependent trim has not
yet been updated. The corrected jacket must drive that next comparison, not
another plate offset over the old jagged surface.

Continue fitting from `binding-neck-donors1.blend`, SHA-256
`433d066c78e762fc7ed638bc39bf251f59c04c5741880aa443b4a571c8b0c610`.
Retain shoulder7 as the before-state. `binding-neck-donors1-receipt.json` freezes
the recipes, reports, renders and input/output identities. No V2 package or
new runtime trial is installed. Other jacket contacts, trim, cords, hips,
gloves, all source palettes and the remaining goal work are not complete.

## Underarm contact isolation

`jacket-contact-locations1.json` places the remaining exterior jacket contacts
around the underarms and lower jacket, with neutral heights approximately
134.85 through 144.06 cm. They hit Eve Body and Eve Arms surfaces, not the
Head-section donor defect repaired above. At one failing triangle, adjacent
vertices 78 and 80 have upper-arm weights of approximately 0.05359 and 0.91554.
The union of the four moving contact sets contains 187 garment vertices.

Three isolated comparisons from neck-donors1 are rejected:

| Copy | Change | Walk36 exterior crossings | Sprint12 exterior crossings |
| --- | --- | --- | --- |
| neck-donors1 baseline | Existing fitted weights | 90 / 23 | 110 / 27 |
| underarm1 | Geodesic 12-mm weight smoothing | 89 / 21 | 101 / 29 |
| underarm2 | Source weights with the same regional falloff | 79 / 16 | 121 / 41 |
| underarm3 | Refine recorded contact faces to 6-mm edges | 237 / 24 | 263 / 40 |

Pairs count jacket-edge/body then body-edge/jacket intersections on the
retained exterior face set. Underarm3 changes topology, so its jacket-edge
counts cannot be used as a percentage improvement or regression. It still
fails the contact screen and the matched close-up. Both weight comparisons
affect 390 vertices including the falloff region and retain neutral geometry
and body controls in their recipes. Source weights are aggregated after bone
mapping; do not use a dictionary comprehension that overwrites source bones
mapping to the same target bone.

Underarm3 adds 867 vertices, giving 13,372 jacket faces. Its independent saved
preservation check passes: original vertices, weights and keys are preserved,
other meshes and rig are unchanged, child triangles cover their parent
surfaces, and UV error is zero. Maximum neutral error is 0.000005961 cm and
maximum new weight error is 4.86e-8. Those structural results do not make the
moving fit acceptable. Source-weight restoration also produces an unwanted
fold under the arm in the reviewed sprint side view.

The 24-direction assembled-geometry check confirms that these are not all
hidden crossings. `jacket-contact-visibility1.json` records 30 visible witnesses
in walk36, 49 in sprint12, 29 in maximum-control walk36 and 62 in
maximum-control sprint12. This uses opaque geometry and a 0.02-cm depth
tolerance, not final material rendering or a complete motion test.

The earlier positive-X side view misses the exposed negative-X underarm in
maximum-control sprint. The reviewer now supports an optional `--both-sides`
view. `underarm-contact-cameras1.json` centers a matched 14-cm close-up on
25 witnesses visible from negative X. The baseline and subdivision close-ups
both show skin breaking through a jagged garment edge. Do not advance either
on the basis of the opposite-side view alone.

In `review-underarm-contact-probes1/report.json`, negative-X pixel `(440,600)`
hits body face 10036 before jacket face 3286 by **0.238820 cm** along the view
ray. Pixel `(420,560)` hits jacket face 3232 only 0.012803 cm before the body.
Pixel `(320,650)` hits jacket face 878 before the body and a second jacket
surface. These rays distinguish exposed skin, a nearly contacting edge and
an overlapping garment fold. The 0.238820-cm difference is a view-ray depth,
not a measured rest-normal displacement or a universal correction amount.

Continue from neck-donors1. Neither smoothing, restoring source weights nor
adding triangles alone solves this region. The shoulder trim remains dependent
on the corrected jacket and is still open.

**Subsequent user direction, October 12:** minor clipping that is unobtrusive
during normal play may wait for later improvement. This supersedes the proposed
local-clearance experiment. The 14-cm crop and geometric witnesses establish
contact, not its severity at normal viewing scale. Review ordinary-scale
visibility before further fitting; defer inconspicuous cases and prioritize
obvious fit, silhouette and functional defects. Do not describe deferred
clipping as fixed, or use contact counts alone to reject the whole outfit.

`underarm-study1-receipt.json` freezes 60 recipe/evidence files and records the
input plus three rejected blend identities. The three regenerable rejected
blends were removed after hash and open-file checks, reclaiming 271,717,118
logical bytes. The common input, frozen recipes, metadata, refinement origins,
reports and renders remain. No external archive is claimed because that volume
is full. See `underarm-study1-cleanup.json`. No game file was changed.

Subsequent whole-outfit review and source-material work are recorded in
[Peony V2 materials](2026-10-12-peony-v2-materials.md). The fitting copy stays
at neck-donors1. The user's minor-clipping deferral moves material, palette,
stocking and flowing-trim implementation ahead of further underarm refinement.
