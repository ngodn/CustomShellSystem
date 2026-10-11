# Peony V2 body controls and target binding

Status: private authoring study, not installed or accepted for release. Continue
from `binding-collar1` for body-control and motion repairs. The preceding neutral
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
