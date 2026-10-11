# Corrected Peony/Hydrangea source

The user confirmed that the earlier Better Cleavage RAR was the wrong input
for the requested reference design, and explicitly retired it. The replacement
inputs are the V2 CNS ZIP and Recolors Texture ZIP named in the main goal.
This supersedes further fitting of the rejected Peony refit2 garment. It does
not establish that source choice caused every visible deformation.

## Verified source inventory

Evidence is in the Eve project's `work/peony-v2-source1`:

- `archives.json` records both original ZIP hashes. Extraction uses a new
  directory and rejects paths outside it. Original archives remain unchanged.
- Five package listings cover the V2 container and the four recolor containers.
- The CNS descriptor selects `CH33V2_NRS`, `CH33V2_RS51` or `CH33V2_RS101`.
  Each has 32 material slots and 556,401 source triangles. NRS has 307,221
  decoded vertices; RS51 and RS101 each have 307,603.
- All three skin audits match between an isolated mod/global-container mount
  and a mount with retail dependencies. `mesh-audit.json` records the results.
- `source.blend` contains 96 editable source parts across the three variants.
  `source-validation.json` passes triangle identity/winding, positions, UVs,
  raw normals/tangents, colors and named skin weights. Each source triangle is
  accounted for exactly once. These are source parts, not fitted CSS assets.
- `review-nrs2` contains front/back/side neutral geometry views. The inspected
  front/back views show the source's structured corset and leg-following fabric.
  The render hides optional nails and alternate shoes to avoid overlapping sets.
  It does not reproduce source alpha masks, textures or game shading. The first
  review attempt stopped because the empty source scene had no World; the
  corrected renderer creates one and completes all three views.

The optional Nails slot alone contains 442,783 triangles. Do not import every
source alternative indiscriminately or mistake the full container triangle count
for the intended CSS outfit cost. Inspect retained parts before assembly.

## Palette source and default

The user requests every distinct supplied scheme as a CSS template, replacing
the earlier fixed five-palette requirement for Peony. The default is the supplied
black-and-white floral Hydrangea reference. Keep independent manual tint and
optional flowing glow controls. Body reshape variants are not color templates.

`palette-source-inventory.json` retains the source material parameter overrides,
exported texture hashes, V2 CNS controls and reshape/non-reshape comparisons.
Hydrangea containers export seven material instances and 16 texture PNGs each;
Peony containers export eight material instances and 20 texture PNGs each.
All four export commands complete successfully. There is a missing package-import
warning for the old retail Body_02 mesh while reading the Hydrangea containers;
that mesh is not the selected V2 source. The V2 mesh was read separately and
passed the isolated/retail comparison. Do not equate successful texture export
with complete material reconstruction: inherited parameters, mask assignments,
pattern scale and the final visual match remain to be verified.

## Next implementation

Select the source variant using its fit to the accepted Eve body, preserving the
source's smooth structure and trim. Reuse the established fitting and morph
workflow but do not import the rejected garment's displaced vertices, old source
component numbers or cloth proxies into this different mesh. Re-identify the
pockets and mechanical ornaments before removal; preserve gloves, collar, heels
and ponytail attachment as requested. Verify the reference silhouette and
material patterns before cooking another trial, then check all body controls,
poses, facial playback and gameplay. No V2 game package is installed yet.

## V2 alignment and first fitting comparison

`aligned.blend` uses the existing joint-alignment tool and the accepted
`work/eve26/skin-f16-import/skin.mesh.json` body reference, without changing the
body. `fit-comparison.json` measures triangle-center distances weighted by area.
NRS is the closest torso/hip starting point of the three: the corset distances
are 1.494 cm for NRS, 1.679 cm for RS51 and 2.006 cm for RS101. These unsigned
distances do not prove exterior fit. The aligned NRS renders show clear body
intersections, including chest, shoulders and legs.

The source slot 8 thigh fabric is weighted primarily to thighs, pelvis, knees
and their helpers. Slot 4 follows calves and their helpers. Preserve that
body-following behavior rather than transplanting the retired source's free
hanging-panel configuration. This does not remove the body/garment secondary
motion requirements or authorize disabling existing outfit physics.

`fit_peony_v2_fabric.py` produces private fitting studies using one continuous
displacement field for each fabric group and its trim:

- `fit1`: broad nearest-surface averaging. Geometry checks pass, but the front,
  back and side renders show extensive intersections. Rejected.
- `fit2`: oriented normal rays before averaging. Geometry checks pass but the
  renders still fail. Rejected.
- `fit3`: fitting field coefficients rather than averaging requested offsets
  fails the jacket geometry gate (minimum area ratio 0.163, maximum edge ratio
  2.079). No blend is saved.
- `corset3`: isolate the corset with regularization 0.05. Geometry checks pass;
  visible chest intersections remain. `corset-ray-witness` repeats it with
  donor evidence. Several grazing rays request nearly 8 cm of movement toward
  remote body surfaces, including a sideways back-seam displacement. Rejected.
- `corset4`: regularization 0.005 without the donor correction fails the
  geometry gate. `corset5` excludes inward/horizontal corset faces from field
  samples but also fails. Do not repeat those parameter changes as a repair.
- `corset6`: restrict rays to similarly oriented, local body hits. Maximum
  binned fitting-offset residual decreases from 5.713 cm in corset5 to 1.640 cm.
  Geometry checks pass, with no reversed faces. The front render has smaller
  chest intersections, but still fails; the back lacing also needs its original
  structure preserved. This is diagnostic progress, not accepted fit.
- `corset7`: preserve aligned vertical coordinates and cast horizontal rays.
  This fails the geometry gate (minimum area ratio 0.0114). No blend is saved;
  a subsequently attempted review stops on its missing receipt. No review or
  acceptance is claimed for it.

Next isolate the remaining corset witnesses and distinguish visible fabric,
thin folded edges and internal closure faces before adding fitting constraints.
Do not relax the geometry gates or cook a candidate to hide the failures. Check
seam/lacing deformation alongside skin clearance. Jacket, arms, footwear, lower
garments, morph transfer and final material reconstruction remain unfinished.
All original inputs and installed game packages remain unchanged.

## Corset alignment and cross-section study

The original source back view has evenly spaced lower lacing. The joint-aligned
view already compresses those rows before surface fitting. The alignment uses
different segment transforms blended by source skin weights. Preserving that
aligned Z coordinate in a later fitter preserves the unwanted compression too.

`fit_peony_v2_corset_profile.py` now offers a single affine torso alignment from
the original Basis, using pelvis/thigh-root and neck landmarks. Its height scale
is 1.06278955. A radial control field then fits the torso cross-sections while
keeping these uniformly aligned heights unchanged. Main fabric and trim slots
24 through 27 receive the same field. Source Basis, source weights, UVs, topology
and the accepted body remain unchanged. This does not replace final target
skinning or body-morph transfer.

- `corset-profile1` retains the earlier joint alignment. Its raw profile field
  distorts lacing and fails geometry checks.
- `corset-profile2` starts from the original affine torso alignment but its
  unsmoothed field still fails geometry checks.
- `corset-profile3` smooths the control field over 1 cm, without smoothing mesh
  vertices. The trim checks improve, but 18 exterior face-center samples remain
  inside the body, at most 0.0662 cm deep. Rejected.
- `corset-profile4` raises radial clearance from 0.6 to 0.7 cm. Four face-center
  samples still penetrate, at most 0.0221 cm. Rejected.
- `corset-profile5` uses 0.8 cm radial clearance and expands contact sampling to
  vertices, edge midpoints and centers of exterior main-fabric faces. All 14,763
  samples are outside the body; minimum signed distance is 0.00364 cm. Four
  corset slots pass the local orientation, area and displacement checks. The
  front/back/side renders are saved in `review-corset-profile5`.

The orientation check needed correction: comparing a face normal directly with
its undeformed normal calls rotations over 90 degrees reversals. Six bottom hem
faces have that result, but all retain their winding relative to the normal
transported by the deformation Jacobian. The latest check requires a positive
Jacobian determinant and a positive transported-normal dot product, alongside
the existing area and displacement thresholds. Both raw and transported results
are retained. The lowest determinant across the four slots is 0.26045, with no
transported-normal reversals. This is a local geometry check, not a proof against
all self-intersections or all possible poses.

The neutral corset result is useful progress, not an accepted outfit. Its minimum
contact margin is small. The renders still show the untouched jacket, arms and
leg pieces intersecting the body, and retained hip parts overlap the lower back
corset. Refit/remove those parts as specified, review the full reference silhouette
and material match, then validate morphs, target skinning and motion before cook.
No V2 game trial has been installed.

The four rejected profile blend files are archived under
`/run/media/eins0fx/theark/CSS/peony-v2-rejected-20261011` after SHA-256 verification,
freeing 380,998,142 bytes locally. Their receipts, logs and review images remain
in the work directory. `rejected-fit-archive.json` records each destination.
The exact profile5 script is retained in `recipes/fit_peony_v2_corset_profile5.py`.

## V2 component removals and lower fit

The source component renderer now accepts a V2 identity and the source's camera
orientation. Separate textured reviews identify calf fabric (slot 4, components
6/7), thigh fabric (slot 8, components 3/4), glove parts, and hip slots 7/9/11.
Slot 9 contains decorative fabric bands; slot 11 contains long decorative straps.
Keep both. Slot 7 contains the small hardware pieces with separate glow in 16.
These source IDs are specific to V2 and were not copied from the retired RAR.

`prepare_peony_v2_wardrobe.py` produces `wardrobe1.blend` from corset-profile5:

- Remove both pockets, arm ornaments/stickers, leg-mounted hardware and its
  corresponding glow. Preserve the inspected glove components and glove glow.
- Preserve decorative hip fabric/straps, underwear, jacket/collar, corset and
  default heels/glow. Exclude the alternate heel set and optional nails, which
  were disabled in the source descriptor. The ponytail is an existing Eve part
  outside this source garment and has not been changed.
- Verify retained face identity, winding, UVs, source weights and every shape
  coordinate after removing faces. The accepted body stays unchanged.
- Remove 477,703 triangles from the selected variant, including the optional
  442,783-triangle nail set. Dead vertices remain temporarily to preserve key
  indices. Compact them during final export. Final CSS controls remain pending.

The initial removal run stopped on a NumPy integer assigned to a Blender custom
property. Converting the recorded IDs to Python integers fixes that diagnostic
property, and the rerun passes all preservation checks. A lower-fit invocation
made before that corrected run stopped because no wardrobe receipt existed;
it produced no candidate. The successful lower-fit run uses wardrobe1's hash.

`fit_peony_v2_leg_profiles.py` aligns the original leg fabric continuously using
ankle, knee, thigh and spine landmarks, then fits radial profiles. It retains
the original Basis and adds separate leg-alignment and fit keys. This reuses the
earlier leg-section fitting approach but is not yet an accepted lower garment:

- `legs-profile1` has no transported-normal reversals, but contact checks fail
  on 160 calf faces and 66 thigh faces per side. The calf has up to 12.04 times
  its initial triangle area and a 5.77 cm exterior gap. Reject it.
- The worst ankle witnesses include inward cuff-closure vertices close to the
  leg axis. Those surfaces should not drive the exterior profile. The source
  foot joint is 14.96 cm high; the target foot joint is 7.87 cm high. The cuff
  extends below the ankle landmark, so its relationship to the preserved heel
  and fitted foot must be resolved, not hidden by broad fabric inflation.
- `legs-profile2` excludes source radial hits below 1.8 cm from profile donors
  and uses the wardrobe with hardware removed. The calf's exterior gap falls
  to 1.32 cm, but contact still fails on 180 calf faces and 66 thigh faces per
  side; the largest area ratio remains 12.04. Its front/side render has a cleaner
  cuff outline, but this is still rejected. Do not call it fitted or ready.
- The next script revision explicitly rejects an area ratio above 4 as well as
  the existing orientation, collapse, displacement and contact failures. The
  exact scripts used for profiles 1 and 2 are retained in `recipes`.

`legs-profile1-witness.json` records the deepest contacts and largest moves.
`legs-profile1-centers.json` checks actual body slices against joint placement:
the target ankle center is consistent with the body, so a guessed skeleton-axis
shift is not supported. Inspect exterior fabric versus folded closures, and
coordinate ankle/heel fitting with the existing footwear workflow before another
lower fit. Keep the contact failures visible until the geometry and visual
evidence distinguish internal closures from exposed clipping.

The source also has a stocking material on body slot 0. Its CNS controls set
`ML_STK_SkinTint`, `ML_STK_StockingTint`, and the `Stockings Legs` texture option
(`Mask_Stocking`, Stockings/No Stockings). The exported `0_Body_46` material has
stocking intensity/roughness parameters and a black mask default, while the CNS
selection uses the white mask. Preserve that effective runtime selection when
adapting the stocking layer to the accepted Eve body. Transferring only the outer
fabric would miss part of the requested appearance. The neutral geometry renders
do not show this material layer and are not palette acceptance evidence.

Neither the wardrobe nor either lower-fit study is installed. Jacket, gloves,
footwear, hip/crotch fit, stocking materials, all six morphs and posed validation
remain unfinished, alongside the rest of the coordinated release goal.
