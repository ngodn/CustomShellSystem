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
