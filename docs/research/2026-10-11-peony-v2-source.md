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

## Cuff witnesses and covered-foot pose

`review_peony_v2_leg_contacts.py` maps rejected contact face IDs back onto the
untouched source. The eight views in `review-leg-contact-source2` distinguish
internal calf cuff closures from exposed thigh seam patches. Several failures
are folded internal surfaces, but others are visible fabric. Do not discard all
contact failures or inflate the entire garment to satisfy them.

The existing footwear tools now accept the inspected V2 identity and separate
shoe, glow and cuff slots. Private studies retain the accepted bare body and
source Basis:

- `heels-profile1` preserves the source heel profile around the target ankle,
  with uniform scale about 1.149. The heel extends roughly 9.44 cm below Z=0.
  This is not a verified runtime ground offset. The source toe direction has
  about 9.67 degrees of horizontal yaw, which the initial tool assumed was zero.
- `heel-lining1` duplicates 2,689 accepted-body vertices and 5,328 faces below
  42 cm, with original UVs and six body morphs. Rotating both arch and toes
  together leaves obvious foot/boot intersections in the front and side views.
- `heel-lining2` tests the existing radial inside-boot projection against slot
  30 only, excluding cuff closures. It moves 704 vertices, misses 687 rays and
  displaces one vertex by 7.065 cm. The render has a pointed ankle artifact.
  Reject this fit; do not ship the contracted lining.
- `heels-profile2` corrects yaw using both source and target foot directions
  and asserts that they align. It still preserves the heel's local shape.
- `heel-lining3` keeps toes in their bind orientation while translating their
  ball pivot with the rotated arch. This avoids treating the whole foot as a
  rigid downward-pointing segment. No radial shrink is applied. Its render has
  level toes, but they and the ankle still intersect the boot. This is a useful
  pose study, not accepted footwear or a repair of the full lower garment.

`review_peony_v2_source.py --focus-feet` hides only the accepted-body faces
replaced by the lining in the temporary review scene. It now frames actual
face-owned evaluated vertices, includes the lining and accounts for image aspect
ratio, so both feet fit in the front view. It never saves over the input blend.
All three lining reviews completed. No V2 asset has been installed.

Next fit the boot upper and cuff to the separately posed foot, preserving the
source heel/sole design. Check contact, shape and body coverage together before
any morph or motion acceptance. The calf and thigh studies still fail, and the
jacket, hip pieces, target weights, final materials and release checks remain
unfinished. Do not continue the 7 cm lining contraction as a shortcut.

Exact heel and lining recipes are retained under `recipes`, including reconstructed
earlier versions whose SHA-256 hashes match their receipts. The rejected
`heels-profile1`, `heel-lining1` and `heel-lining2` blend files were copied to the
existing external archive, SHA-256 verified and removed locally, recovering
247,050,754 bytes. Their receipts and review evidence remain local. The latest
heel/lining studies and their input files remain available for further fitting.

## Shoe-upper constraints and internal collar surfaces

`fit_peony_v2_shoe_upper.py` tests the boot upper against the uncontracted
`heel-lining3` foot. It uses inspected main shoe components 6/7 from slot 30,
561 vertices and 930 faces per side. Heel mechanism components 0 through 5
remain unchanged, as do the accepted body and covered-foot lining. Nearby upper
trim/glow follows the fitted field. These are private geometry studies only.

- The nearest-normal field does not converge within 400 or 4,000 iterations.
  Those runs save no blend. The latter residual is 0.0843 cm against a 0.015 cm
  tolerance. The existing shared authoring solver now accepts an explicit finite
  iteration limit while preserving its 400-iteration default for other callers.
- `shoe-upper3` allows 40,000 iterations and converges after 6,303 left and 5,988
  right steps. Its rendered shape is badly distorted: maximum displacement is
  12.84 cm, area ratios exceed 22 and 112/104 faces still have sampled contacts.
  Reject it. Numerical convergence is not fit acceptance.
- `shoe-upper4` uses a radial ratio around the calf/arch/toe axis. Contacts drop
  to 12 faces per side, but it expands by 9.41 cm and area ratios exceed 47.
  Reject it. `shoe-upper5` uses additive radial offsets instead, but still moves
  up to 6.55 cm, has area ratios above 38 and exposes toes in the side render.
- `shoe-upper5-donors.json` and `shoe-upper6-donors.json` trace the drivers. The
  largest requested offsets come from inner collar surfaces, not the outer shoe.
  For example, vertex 3370 requests 5.975 cm while lying 6.714 cm behind the
  source exterior along that ray. Vertex 3365 lies 6.418 cm behind the exterior.
  Do not use these hidden folds as the outer profile to inflate around the foot.
- `shoe-upper6` excludes those occluded drivers. Maximum shift falls to 3.64 cm,
  no faces reverse in the raw normal comparison, and the front/side outline is
  substantially less distorted. It still has 111/113 sampled contact faces and
  a maximum area ratio of 13.49. The toe and ankle remain visibly exposed through
  the shoe. This is not accepted and must not be cooked as a finished fit.

The next fit must address the toe box and collar locally, preserving folded
layers coherently. Inspect the posed toe against both the sole and upper surface
before changing its height or expanding the shoe. Do not resume fitting all
inner collar faces as exterior constraints, and do not contract the foot by the
previous 7 cm projection. The calf/thigh failures and all remaining upper-outfit,
material, morph, skinning, physics and release requirements remain active.

`heel-lining3-geometry.json` records a separate pose screen: 5,328 faces, area
ratios 0.2335 through 2.0372 and six raw normal-direction reversals. These raw
normal comparisons do not account for transported orientation and do not prove
that the lining is free of self-intersections. Do not label it validated footwear.
Recipes and diagnostic scripts are retained under `recipes`. Rejected shoe-upper
3/4/5 blends were SHA-256 verified in the external archive and removed locally,
recovering 247,393,159 bytes. Their receipts, logs and rendered evidence remain
available. No game file changed.

## Closed-boot coverage comparison

Two further private toe-box studies (`toebox1` and `toebox2`) remain rejected.
Their side views retain an abrupt arch transition and change the source shoe
shape. Do not continue expanding the shoe around every posed foot vertex.

The earlier [customization binding work](2026-10-10-peony-customization-bindings.md)
already supports hiding exact covered-body faces when boots are shown and
restoring them when removed. Reuse that mechanism with V2 face identities and
new catalog bindings, not the retired garment's section numbers.

`review-closed-boot-coverage` renders `heel-lining3.blend` with its original
source-preserving heel profile. The temporary scene hides the covered body faces
and the study's added lining. All three renders were inspected: the exposed toe
is gone and the original shoe outline remains, but the side view shows an open
gap between the calf fabric and shoe collar. Coverage alone therefore does not
produce an accepted boot. This is a neutral geometry comparison, not a material,
motion or runtime test. The renderer does not overwrite the input blend.

Next resolve the calf-to-shoe junction against the original source assembly,
then determine which body faces can be hidden and which openings need visible
skin. Keep coverage reversible when removable parts are hidden. Do not hide
unrelated fit defects or reshape the accepted bare body. The full garment fit,
all supplied palette templates, morphs, skinning and release scope remain open.

## Original heel connections restored

`audit_peony_v2_ankle_junction.py` compares the complete source, retained source
pieces before fitting, and the fitted assembly. `ankle-junction1` establishes
that removing source slot 4's low ankle fittings opens the calf-to-shoe junction
even before fitting. Evaluated coordinates match the intended boot profile to
within 0.000004 cm, ruling out shape-key composition as this gap's cause. The
user explicitly retained the original heels, so their connecting fittings belong
in the retained wardrobe.

`ankle-restored1.blend` restores slot 4 components 3, 5, 9, 10, 11, 12, 13, 14,
15 and 16: 2,372 triangles using 1,606 vertices. Their source bounds are below
24.22 cm; unrelated knee/thigh ornaments remain removed. The repair copies
original topology, UVs and source weights and applies the same ankle-anchored
heel transform. Checks verify every previously retained fabric face and fit
coordinate, all other existing shape coordinates, and the accepted body remain
unchanged. `prepare_peony_v2_wardrobe.py` now retains these components on future
rebuilds as well.

Fresh-process `ankle-junction2` verifies the saved profile coordinates, and
`review-ankle-restored1` provides inspected front, side and rear views with
covered foot faces hidden. The original ankle connection is present again
without inflating the shoe. This accepts only the restoration of that source
detail, not the unfinished lower-garment fit or gameplay behavior. Exact V2
covered-body bindings, removable-part combinations, target weights, six morphs
and motion checks remain pending. The old section IDs must not be reused.

Recipes are frozen under `recipes/*-ankle1.py`; the saved candidate receipt
records source/input/script hashes. Rejected `toebox1`, `toebox2` and
`shoe-upper6` blends were copied to the external CSS archive, hash-verified and
removed locally, recovering 247,402,906 bytes. Their measurements, recipes and
rendered evidence remain available. No game package or settings changed.

## Hip fitting donors and waist overlap

The initial hip field used nearest points on the complete accepted body. That
body contains internal anatomy as well as exterior skin. `pelvis-origin.json`
records rays that encounter those internal surfaces before reaching the skin;
using them as fitting donors produces incorrect corrections. This is not
evidence that the whole body has reversed normals. The body remains unchanged.

`hip-surface1` leaves groin intersections. Enforcing clearance against unfiltered
donors in `hip-surface2` produces 84 orientation reversals in the underwear and
a 12.40 cm maximum displacement. Both are rejected. The radial alternatives
also fail and are not a production path. Excluding the entire anatomy material
in `hip-surface3` misses exposed detail and leaves complete-body intersections.

`hip-surface4` uses identified exterior body/leg materials plus 333 visible
detail triangles for fitting. Internal detail is excluded only from donors,
not removed from the body or the intersection test. One shared smooth field
fits slots 9, 10 and 11, with triangle-center and edge-midpoint constraints.
All three pass the measured neutral body-contact check, have positive local
Jacobians and no transported-normal reversals. Bidirectional edge tests find
no intersections with the complete 61,814-triangle body. Minimum sampled skin
clearance is 0.293 cm; the largest remaining sample distance is 1.840 cm, so
this does not establish a uniformly close fit. A fresh-process preservation
check passes for existing geometry, UVs, weights, shape coordinates and the
saved evaluated fit. Recipes are frozen under `recipes/*hip4.py` and
`recipes/fit_peony_v2_hip_surface4.py`.

Front, side and rear close-ups still reject the assembled waist: the fitted
underwear waistband protrudes through the independently fitted corset.
`review-hip-surface4-owner/front.png` highlights slot 10 and identifies that
piece directly. The first spacing audit includes inward-facing corset closures,
so its 1.200 cm worst overlap and sub-0.1 cm clearance must not be interpreted
as measurements of the outer fabric alone. The paired solver restricts its
corset contact surface to outward-facing main fabric. Its initial maximum
required correction is 0.965 cm. Do not describe the neutral body-contact pass
as visual, morph or gameplay acceptance.

The lower leg fabric, jacket and gloves still need fitting. Target skinning,
all six body morphs, V2 visibility bindings, source material/palette matching,
motion and runtime checks remain open. The installed rejected garment is not
changed by these private studies.

Rejected `hip-surface1` through `hip-surface3` and `underwear-radial1`/`2`
blends were copied to the external CSS archive, hash-verified, then removed
locally, recovering 412,830,895 bytes. Receipts, logs, recipes and rendered
evidence remain local. The current candidate and its input are retained.

## Paired waist-layer correction

`waist-layers1` was stopped before saving: it attempted to move existing inward
corset closures out of the body, creating a 6.655 cm constraint. This repeats
the distinction between garment exterior and internal closures; never fit
every source triangle as if it were visible fabric.

`waist-layers2` fits slots 9/10/11 and corset slots 24/25/26/27 together using
two smooth fields. The accepted body stays fixed, the trim follows its fabric,
and upper corset anchors limit unrelated movement. Neutral renders remove the
waistband stripe through the front. An edge intersection test still finds one
underwear/main-corset crossing that the point sampling missed.

`waist-layers3` adds direct edge-contact constraints for that gap in coverage.
The fresh `waist-layer-audit4` finds no sampled overlaps or bidirectional edge
crossings against the classified exterior main corset fabric. Minimum sampled
layer margins are 0.243, 0.062 and 0.304 cm for slots 9, 10 and 11. Maximum
additional movements are 0.382 cm for underwear and 0.932 cm for the main corset.
All seven parts retain positive local Jacobians and have no transported-normal
reversals; triangle area ratios remain between 0.902 and 1.123. This is a
neutral layer correction, not proof of close fit everywhere or motion support.

Front, rear and side views in `review-waist-layers3` were inspected. The visible
front overlap is absent, while the leg fabric is still visibly unfinished.
The full-mesh intersection gate still fails outside the classified exterior
main fabric; do not discard those findings as harmless without inspecting the
remaining closures and bindings. Existing corset/body intersections also need
classification. No whole-outfit acceptance is recorded.

`waist-layers3-preservation.json` passes across 98 mesh objects, with only the
seven intended active shape edits. Original face/vertex identities, UVs,
weights and existing shape coordinates remain unchanged, and saved evaluated
coordinates match the new key. Frozen fit/audit recipes and a separate visual
review record preserve this candidate's evidence. No V2 game trial is installed.

## Thigh fit and remaining waist contacts

`waist-layer-audit5` classifies the remaining waist contacts without dismissing
them. The main corset faces involved are inward-facing, with radial normal dot
products from -1.000 to -0.401. Eighteen contacting faces belong to source
component 1, a small back section. These still need inspection and a binding or
geometry decision. The outer main-fabric pass does not cover them.

The first thigh studies show why the hip method cannot simply be applied to the
whole leg. The source skin stops at 57.83 cm, while the retained thigh garment
extends to 47.32 cm. A torso-aligned, source-body-only field therefore
extrapolates below its support and produces visible bulges. `thigh-surface-base1`
also has a negative minimum Jacobian and four transported-normal reversals.
The unrestricted contact variant `thigh-surface1` is rejected as well.

`thigh-surface2` uses the existing ankle/knee/thigh/spine alignment and constrains
only outward-facing fabric. It removes the orientation reversals, but retains
up to 3.851 cm sampled separation and a visibly oversized outline. This is not
accepted fit. The earlier `fit_nextgen_garments.py` stocking workflow separates
fabric from trim, uses per-leg projection, and checks triangle interiors. The
next V2 study uses those same distinctions instead of projecting inward cuffs.

`thigh-surface3` fits slot 8's retained fabric using per-leg exterior body rays
and a shared smooth displacement field. It preserves the source-aligned seam
heights in the initial projection and excludes unused ornament vertices and
inward closures from fitting constraints. The maximum displacement is 3.372 cm;
the minimum local Jacobian is 0.595, there are no transported-normal reversals,
and triangle-area ratios range from 0.645 to 2.112. The independent contact audit
checks 3,356 classified exterior faces at vertices, edge midpoints and centers:
no inside samples, with 0.290 to 1.207 cm clearance. This is a sampled neutral
exterior pass, not a full intersection or close-fit guarantee.

Front, rear and side renders in `review-thigh3` show a closer leg outline. The
isolated untouched slot 8 views in `review-thigh-original` confirm that the knee
folds are present in the V2 source. They must not be reported as introduced by
this fit. Calf fit, the knee junction and the source stocking material still need
review. Another 386 faces outside the exterior classification have inside
samples, so the full-mesh contact gate remains failed. Do not call the outfit
ready from the exterior subset alone.

Fresh-process `thigh-surface3-preservation.json` passes across 98 mesh objects;
only slot 8's new active shape changes. Existing source topology, UVs, weights,
shape coordinates and the accepted body remain unchanged. The exact fitted
recipe is frozen as `recipes/fit_peony_v2_thigh_surface3.py`, with matching
receipt hash. Its legacy `source_skin_slot: 0` field names the source body
reference; `fabric_leg_donors: true` and the review record identify slot 8 as
the actual fitting donor. Future receipts include that donor slot explicitly.
Morph transfer, target skinning, materials, motion and runtime remain pending.

Rejected thigh studies 1, base1 and 2, plus superseded waist-layers2, were copied
to the external CSS archive and hash-verified before removing local blends,
recovering 330,740,459 bytes. Measurements and rendered evidence remain local.
The latest thigh candidate and its waist input are retained. No game package,
save or settings changed.

## Calf shaft and preserved heel junction

`calf-surface1` stops before saving because the thigh skin filter excludes
`Eve Covered Feet`. The follow-up donor audit locates a missing ray at height
41.753 cm; its nearest complete-body surface is that material, only 0.346 cm
away. The material covers part of the calf as well as the foot. The calf fitter
therefore includes it explicitly instead of guessing a replacement surface.

`calf-surface2` changes only slot 4's retained fabric components 6 and 7. The
restored ankle hardware and original heel remain in place. Initial alignment
blends the preserved boot profile into the leg alignment. A smooth field is
fixed below 16 cm and reaches full fitting strength at 27 cm. Contact correction
is restricted to the outer shaft above that transition. This is deliberate
preservation of the original heel connection, not clearance of the lower boot
against the unposed bare foot.

The independent audit checks 1,272 classified exterior faces wholly above the
27 cm transition: no inside samples, with 0.291 to 0.856 cm clearance. All
retained calf fabric has positive local Jacobians (minimum 0.917), no
transported-normal reversals, and area ratios from 0.936 to 1.944. The largest
move among used fabric vertices is 2.691 cm. The older fit receipt's 5.017 cm
maximum includes unused source vertices; `calf-surface2-contact2.json` records
the relevant used-vertex measurement and future fit receipts use that scope.

The saved-file preservation check passes across 98 mesh objects, with only the
new calf shape active on slot 4. The 1,606 restored hardware vertices and 974
lower anchor vertices match the preserved boot profile within 0.000006 cm.
Front, rear and side renders show the heel connection still present. Those
renders use the existing covered-foot preview partition; they do not establish
the final V2 runtime coverage bindings or removable-boot behavior.

The full contact gate still fails: 912 classified exterior faces intersect the
unposed body below the transition, and 175 non-exterior faces above it have
inside samples. Resolve these through the exact geometry/covered-body and
visibility assessment before acceptance, rather than treating a shaft-only pass
as a finished boot. Body morphs, target skinning, pose/ground placement and motion
remain open. The untouched accepted body and all existing source data remain
preserved. Recipes and the three-view record are frozen for this candidate.

The next upper-garment baseline, `review-jacket-baseline1`, shows shoulder skin
through the front jacket and an oversized back/sleeve outline. These views were
inspected and remain rejected. Correct the jacket and sleeve alignment before
using its contact data to fit it. No new game trial is installed.
