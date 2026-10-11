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

## Jacket projection diagnosis

The source skin is absent around the shoulders. Inspected source skin-slot 1
and 2 renders confirm that neither is a complete shoulder fitting reference.
`review-source-jacket1` records the original jacket from front, rear and side;
its source geometry is retained. The raw-source camera faces +X, while the
aligned camera faces -Y. Their different camera axes are intentional.

`jacket-fit1` is rejected. Its nearest-surface projection omitted `Eve Face`
from the body reference, pulling upper collar samples down toward the lower
neck. More seriously, its exterior classification admitted internal jacket
faces. Face 869 has source-aligned edges over 10 cm and joins vertex 547 near
the spine. Forcing that interior fan onto the body demanded a 4.39 cm contact
correction. The resulting field inverted geometry despite passing its sampled
clearance check. A clear sample margin is not an acceptance result.

The follow-up uses source visibility, outward directions relative to the source
bone segments, and exclusion of faces touching the skeleton core. The 1.5 cm
core test identifies internal fan poles, not the body's skin boundary. Both
source skin occlusion and normal agreement alone leave interior fan faces in
the donor set. `jacket-projection-audit6.json` records the remaining worst first
stage contact at the arm, about 2.06 cm inside, after those poles are excluded.
No source faces, UVs or original shape coordinates are removed by this study.

`jacket-fit2` aborts before saving when a source-facing projection cannot find a
matching body normal. `jacket-fit3` and `jacket-fit4` also fail. Front/rear/side
review of fit4 shows improved shoulder coverage but a large left shoulder fold
and a collar defect. Fit4's contact field extrapolates into the jacket core,
moving some used vertices as far as 16.49 cm. It is not a usable candidate.

`jacket-fit5` adds 48 source-to-target bone references inside the garment and
holds those references fixed during the contact correction. It changes only
the private jacket study, not the accepted body or skeleton. The core-anchor
contact drift is below 0.000001 cm, and the maximum used-vertex displacement
falls to 7.81 cm. The large left shoulder fold is gone in the inspected views,
but a small front defect, lower back hem defects and choker clipping remain.
The geometry gate still fails: main slot 19 has minimum local Jacobian -0.262,
40 transported-normal reversals, including five faces classified as exterior,
and triangle area ratios from 0.0168 to 5.443. Slots 20 through 23 pass that local
orientation/area screen, which does not establish their body clearance.

Keep `calf-surface2` as the previous cumulative fitting baseline. Fit5 is retained
only for diagnosis. Next inspect the original topology/UVs of faces 740, 870,
871, 907 and 910 and the associated interior fan; resolve their deformation
without deleting useful source fabric or accepting the classification blindly.
The choker also needs its own body-contact assessment. Do not cook or install
these jacket studies. Full garment fit, source materials and every included
palette, all six morphs, target weights, gameplay and the broader goal remain
unfinished. Black-and-white floral Hydrangea remains the required default.

Recipes `fit_peony_v2_jacket1.py` through `fit_peony_v2_jacket5.py`, projection
receipts and source/candidate renders are retained under `work/peony-v2-source1`.
Fit2 has no saved blend. Rejected blends 1, 3 and 4 are moved to the external
rejected-fit archive after SHA-256 verification; their receipts and renders
remain local. No V2 game trial, core update, save or appearance setting changed.

## Jacket and choker layer correction

The five previously classified exterior reversals all belong to source UV island
0, a 150-triangle interior fan. The original UVs and highlighted source views
identify that island inside the neck/chest opening, not on the outer back.
`jacket-face-audit2.json` and `jacket-inner-island0.json` retain the face IDs.
Keep this geometry, but exclude it from exterior fit drivers. That exclusion
alone does not solve the deformation: `jacket-fit6` still has 20 reversals and
minimum local Jacobian -0.364, so it is rejected.

`jacket-flow1` instead composes small fitting steps from the original aligned
source. Each step limits displacement and gradient, tests transported face
orientation and holds interior bone references. All 30 steps pass their local
orientation guard. This is not a proof against all self-intersections. The main
jacket still fails its total area-ratio screen (0.107 to 3.942) and has ten
classified outer faces with inside samples. `jacket-contact1` corrects those
residual contacts in three guarded passes. All 2,513 classified exterior faces
then have positive sampled body clearance, with a 0.296 cm minimum.

Slot 21 is the choker, including its pendant and hardware. Its original aligned
band reaches the target jaw. A separate placement audit tests vertical offsets
from 0 to -2.5 cm. `jacket-choker1` places it 2 cm lower and applies one affine
cross-section adjustment to the entire original part. It clears all 1,490
triangles in the body-contact sample screen. Front/rear/side review nevertheless
shows an overlap with the jacket's standing collar and defects at the jacket's
lower hem. Body clearance alone did not resolve garment layering.

The independent edge audit identifies the remaining layer conflicts: 150/107
crossings between choker and jacket, and 103/106 between jacket and corset.
The affected jacket UV islands differ from island 0. The `jacket-lining4`
harmonic interior-fan experiment therefore does not address those defects and
is set aside. Its geometry is not included in the current candidate.

The retained sequence is:

`calf-surface2 -> jacket-flow1 -> jacket-contact1 -> jacket-choker1 -> jacket-layer1 -> jacket-layered-choker1`.

`jacket-layer1` uses the earlier waist-layer approach: constrain the jacket
against the classified outer corset while retaining body clearance. It changes
slots 19, 20, 22 and 23, keeping the choker fixed. Three guarded passes clear the
tested overlap. `jacket-layered-choker1` then fits the complete choker around
both body and the standing collar. The final independent edge results are:

| Surfaces | First-surface edge crossings | Reverse edge crossings |
| --- | ---: | ---: |
| Classified outer jacket / complete body | 0 | 0 |
| Complete choker / complete body | 0 | 0 |
| Complete choker / complete jacket | 0 | 0 |
| Complete jacket / main corset | 0 | 0 |
| Complete jacket / complete body | 237 | 284 |

The full jacket/body crossings remain outside the classified exterior and are
unresolved. The test excludes edge endpoints and does not establish coplanar
overlap, full self-intersection or animation safety. Exterior jacket sampled
clearance is 0.296 to 2.372 cm; complete choker sampled clearance is 0.481 to
1.753 cm, including its pendant and hardware. Do not interpret those maxima as
uniform cloth-to-skin gaps.

Final front/rear/side images in `review-jacket-layered-choker1` were inspected.
They show a continuous jacket hem and choker without the preceding layer cuts.
Small corset surface defects remain visible in front. This is the current
cumulative authoring candidate, not an accepted outfit or runtime trial.
Resolve remaining inner contacts, corset/cleavage and glove fit, exact lower
body coverage, all six morphs, target weights and moving poses. All supplied
source palettes, material matching and the black-and-white floral default also
remain required before cooking and gameplay acceptance.

Saved-file preservation checks pass across 98 mesh objects for each retained
stage. Only its declared shape keys change; original source coordinates, UVs,
weights, IDs, previous shape coordinates and the accepted body remain intact.
The layer recipes are frozen as `fit_peony_v2_jacket_layer1.py` (SHA-256
`c7b8bdbbf4ca33025af98e0f3e7bfde476a273e30e90b50362c424791a0b17f8`)
and `fit_peony_v2_choker_layer1.py` (SHA-256
`960e38d9ea3213a4d894d2418c99f23b8fc171bb703f9465d97a8ce17af32a65`).
These authoring files live outside the CSS Git repository; the documentation
commit does not commit their contents. JSON receipts, preservation reports,
edge witnesses and frozen intermediate recipes remain beside the candidate.

Rejected fit5, fit6 and the set-aside lining4 blends were copied to
`/run/media/eins0fx/theark/CSS/peony-v2-rejected-20261011`, SHA-256 verified and
then removed locally, reclaiming 248,435,500 bytes. The archive manifest records
each file. No original archive, accepted body, installed package, save or
appearance setting changed. No V2 game test is requested yet.

## Corset contact and deeper neckline

The outer corset's small visible spots are actual body intersections. The
independent edge test finds 4 garment-edge and 30 reverse body-edge crossings
on its radially classified exterior, plus 6/2 on trim slot 25. The witnesses
all belong to `Eve Body` at heights 131.89 to 138.25 cm. Running the same audit
on `corset-profile5` gives the same exterior and slot-25 counts. This rules out
the later waist correction and an omitted body material as the causes of those
specific defects. The earlier sparse sample pass did not exclude small
intersections between sample locations.

`corset-contact1`, built on `jacket-layered-choker1`, corrects upper corset and
trim contacts with a shared smooth field. Coordinates at or below 127 cm remain
fixed, protecting the previous waist-layer fit. Two guarded correction steps
clear the checked outer corset and all three trim parts. Maximum movement is
0.588 cm; main-fabric sampled clearance is at least 0.297 cm. Front/rear/side
review shows the small front spots resolved. The jacket/corset edge check still
passes, and full lower-layer crossing counts are unchanged.

The requested deeper sweetheart design is authored separately in `neckline1`.
It uses the earlier Peony neckline approach on the corrected V2 geometry, with
one smooth vertical map for fabric and trim. The 4 cm depth and 6 cm width are
design parameters, not a final opening measurement. A 12 cm vertical blend
keeps the map locally positive; maximum measured initial drop is 3.981 cm.
The initial design has no tested triangle reversal, with minimum sampled
Jacobian 0.508 and minimum area ratio 0.426. It requires further body fitting:
the first front render and edge test show intersections at the new neckline.

`neckline-contact1` is rejected. Its fixed normal-based donor selection omits
faces that turn outward during fitting. Faces 1911, 1921, 1924 and 1925 still
intersect the body after the sampled margin passes. Expanding that selection
per iteration in `neckline-contact2` is also rejected: it admits inner UV-island
1, whose witnesses demand corrections approaching 4 cm. That result fails
the contact gate. Do not extend its iterations or use it as a baseline.

`neckline-face-audit1.json` identifies the true source fabric as UV island 1086,
with 2,364 faces. Highlighted original front/rear/side views in
`review-corset-main-source1` verify that island covers the exterior fabric,
including the neckline faces missed by the normal threshold. The complete
inspected face list is retained in `corset-main-island1086.json`.

`neckline-contact3` starts again from `neckline1` and fits the upper region of
that original fabric island, moving its attached trim through the same field.
It does not include either rejected contact candidate. Five guarded correction
steps give a 0.299 cm minimum sampled main-fabric clearance. The independent
edge test then checks the entire 2,364-face island, not just the fitted upper
samples: both garment-to-body and body-to-garment crossings are zero. Complete
slots 25, 26 and 27 also have zero crossings, and jacket/main-corset crossings
remain zero in both directions. The verifier additionally retains previously
checked exterior faces so rotation cannot silently remove them from its set.

Final front/rear/side views in `review-neckline-contact3` were inspected. The
front opening is deeper and curved, the fabric remains continuous, and rear
lacing and the lower waist remain in place. These are neutral opaque authoring
views. Shading, textured appearance, all six body controls and moving poses
remain to be verified. The whole slot-24/body check still finds 173/277 crossings
outside the inspected fabric island, so full-mesh internal-surface clearance is
not accepted. Lower hip/corset full-mesh counts are unchanged at 19/2 for slot
9, 393/236 for slot 10 and 13/2 for slot 11. These remain tracked separately from
the previously checked outer-waist fit.

The current cumulative candidate is `neckline-contact3.blend`, SHA-256 recorded
in its receipt. Its retained sequence adds `corset-contact1 -> neckline1 ->
neckline-contact3` to `jacket-layered-choker1`. Saved-file preservation passes
across 98 mesh objects for all three steps: accepted body, topology, original
positions, UVs, weights, source IDs and previous shape coordinates are unchanged.
The neckline recipe is frozen as `shape_peony_v2_neckline1.py`, SHA-256
`110bfddd582121df186bf6aac89e57130eea78bd405e6e08e115159fa3815de7`;
the final contact recipe is `fit_peony_v2_neckline_contact3.py`, SHA-256
`f3515c950f5c64fd58689691f120f678b38052c095f17d0a101b596e2df45a3b`.
The original contact and both rejected recipes are retained too. Authoring
tools and blends remain outside the CSS Git repository.

Rejected contact1 and contact2 blends were SHA-256 verified in the external
rejected-fit archive before local removal, reclaiming 166,000,379 bytes.
Glove fit, remaining internal contacts/coverage, body morphs, target binding,
source materials and every supplied palette, physics/motion and runtime
acceptance remain open. No game files, settings or saves changed. No V2 trial
is installed and no game test is requested yet.

## Glove alignment and contact study

`glove-terminal1` branches from `neckline-contact3`. It applies palm-frame roll
and the previous Eve fitting method's measured terminal-finger tails to the
retained V2 glove and glow geometry. The old RAR supplies no geometry here.
Primary glove components are 5/6 in slot 3; slot 14 contains retained glow.
The source and target palm normals differ by about 19.17 degrees under the
initial one-axis alignment. Terminal corrections move retained glove points by
at most 2.664 cm. This improves the finger alignment, but the initial hand
surface still clips heavily. Original-glove isolated and assembled renders are
retained; clipping against the original supplied body does not establish that
the source game masks that body.

`glove-fit1` incorrectly assigns no forearm support to `Ab-L-Forearm-Tw0` and
its right counterpart. Its source-to-target name lookup omits the ancestor
inheritance used by the original alignment and previous bound-glove workflow.
As a result, it applies triangle-interior pressure to every cuff face. The
largest residual wrist vertex is still about 1.21 cm inside and the cuff
inflates. This candidate is rejected.

`glove-fit2` follows source bone parents to a mapped ancestor and sums weights
that reach the same target bone. This restores vertex-only cuff treatment for
446 faces per hand. Maximum fitting movement falls to 1.794 cm. Left-front,
rear and side views show the inflated cuff removed, but small wrist and
knuckle defects remain. Full-body nearest-vertex checks find 19/18 inside
vertices, reaching 0.212/0.270 cm. This is a useful diagnostic comparison, not
an accepted glove fit. Its saved-file preservation check passes.

`glove-fit3` tests the union of contributing digit surfaces at mixed-weight
webbing. Whole-body witnesses still include neighboring digits outside that
donor set. For example, left glove vertex 3542 has full source middle-finger
weight but intersects an accepted-body index-finger surface. Body face IDs
must be resolved through the import's wedges before querying point weights.
The independent bidirectional edge audit finds 186/232 left crossings and
186/225 right crossings (garment edges/body edges). Of those, the 2,278-face
main UV islands account for 152/201 and 154/191. A separate 38-face island
accounts for most remaining crossings; its physical purpose is not yet
verified. Source-coincident seam groups have no measured separation above
0.001 cm, so opened duplicate seams do not explain these defects.

`glove-fit4` allows all corresponding-hand surfaces after terminal alignment,
including neighboring fingers and nails. It does not solve the contact fit:
left/right full edge counts are 192/259 and 185/261, and close-ups still show
distorted details. It is rejected too. Whole-hand nearest projection is not
accepted merely because most vertices clear the body. Neither candidate is a
reason to hide or shrink the accepted hands. The next correction must preserve
the glove's surface shape and resolve the specific exterior edge witnesses,
with original UV-island ownership checked before deciding which surfaces can
drive contact. Do not repeat a blind iteration or donor-radius sweep.

The audit reports face-area ratios and normals opposed to the pre-fit normals.
Those normal counts alone are not proof of inversion, because a valid local
rotation can reverse that dot product. The edge witnesses and rendered defects
already reject these candidates. Endpoint/coplanar contacts, self-intersections,
body controls and moving poses remain outside this neutral check.

Saved-file preservation passes across 98 mesh objects for terminal alignment
and fit2/3/4: source coordinates, topology, UVs, original weights, previous
shape coordinates and accepted body remain unchanged. No candidate here adds
target skinning or body morphs. `neckline-contact3` remains the cumulative
baseline; `glove-terminal1` and `glove-fit2` remain local diagnostic inputs.

`glove-study1-receipt.json` freezes the study reports and recipes under
`work/peony-v2-source1`. The final diagnostic fitter is frozen as
`fit_peony_v2_gloves4.py`, SHA-256
`8b6d33a6f04f22cbbb44bd9cc95122704808460da588881ee49f8d3d9007ac83`.
Recipes for earlier fitting variants, alignment, auditing and rendering remain
beside it. Authoring files are outside the CSS Git repository. Roll-only,
fit1, fit3 and fit4 blends were copied to the external rejected-fit archive,
SHA-256 verified and removed locally, reclaiming 332,419,086 bytes. Logs,
renders and receipts remain local. No installed package, save or settings
changed; no V2 game test is requested.

## Original glove surface and earlier-fit correspondence

The original-shape renderer now reads preserved Basis coordinates from a V2
wardrobe copy, leaving the file unchanged. It uses that copy's retained face
indices, avoiding a mismatch with the unfiltered source library. Highlighted
front/rear/side views establish that left UV island 91 is the thumb (316
faces), 5392 is the main shell (2,278 faces), and 5426 is an inner wrist closure
(38 faces). The corresponding right islands are 317, 5192 and 5464. The
316-face islands are not cuffs. `glove-surface1.json` additionally classifies
original face direction against its weighted source bone segments. Its
highlighted low-radial faces include the wrist interior and narrow folds near
the knuckles. This is a driver classification, not proof that every excluded
face is invisible.

`glove-flow1` stops before saving: a wrist sample has no target surface within
3 cm agreeing with its direction. `glove-flow2` excludes those inspected
inward-facing drivers while retaining and moving their geometry. It uses 2,455
driver faces per hand, moving the entire glove and its details through one
smooth field. Each hand completes 35 guarded steps. Minimum per-step sampled
Jacobian is 0.803 left and 0.799 right; no tested per-step triangle reversal is
detected. These checks do not establish a correct final shape. The main shell
still has 188/271 left and 191/264 right garment/body edge crossings. Full
slot-3 face-area ratios reach 17.48 relative to the terminal-aligned input.
Front/rear/side review does not accept the remaining finger fit.

`glove-contact1` applies the corset contact-inequality method to that result,
using actual vertex/edge-midpoint/face-center sample interpolation in the
shared field. It completes 16 guarded steps per hand and reduces some edge
crossings, but visibly bulges a finger and displaces retained glove geometry
by up to 2.965 cm. The main shells still have 85/235 and 85/232 crossings.
This is rejected, not a reason to increase iterations. A smooth spatial field
over the whole hand can spread one finger's contact correction into its
neighbor. The next correction must respect source finger correspondence and
the reference silhouette; numerical orientation checks do not override the
rendered failure.

The earlier glove source can be compared directly. `glove-source-comparison1`
and `glove-source-comparison2` read the preserved source surfaces in
`new-outfits-gloves5.blend` and the V2 copy. All 2,632 UV-triangle signatures
match per hand, with 1,502 unambiguous matching UV points. After a fitted
similarity transform, the maximum source-position difference is 0.01797 cm.
Both studies reference the same accepted body file and hash. This verifies a
useful source correspondence; it does not import the retired garment or prove
that the earlier glove passed all current contact checks. Its earlier acceptance
was limited to the recorded neutral and selected posed views.

`glove-terminal2` replays the earlier terminal-direction-only method, without
the newly introduced palm/proximal roll adjustment. `glove-fit5` then replays
the frozen fit2 recipe with correct forearm ancestor mapping. Median neutral
position difference from the earlier fitted glove is 0.0468/0.0447 cm; the
95th percentiles are 0.1833/0.1887 cm. Maximum differences reach 3.336/3.331 cm
and require individual witness inspection before attributing them to any
part. The V2 main-shell edge checks still fail at 150/197 and 145/194. The
left-front close-up also shows a small breakthrough. These are retained
diagnostics, not an accepted fit or a reason to copy old displaced geometry
without checking it. Use the verified UV correspondence to compare each
finger's source position, fitted position and binding explicitly.

Saved-file preservation passes across all 98 mesh objects for flow2, contact1,
terminal2 and fit5. Body, source coordinates, topology, UVs, original weights
and previous shape coordinates are unchanged. The cumulative authoring baseline
remains `neckline-contact3`. No V2 binding or motion claim
is made from these neutral studies.

`glove-study2-receipt.json` freezes reports and scripts. The final rejected
contact recipe is `fit_peony_v2_glove_contact1.py`, SHA-256
`937ed8494c49d38594ab6f3fb89a2c0ca743a31c7790ceb45227e9338d198b05`.
The terminal-only recipe is `align_peony_v2_gloves2.py`, SHA-256
`4d8ee4f341b34b8308ce613091b3417d8689b7f3253d5e75611e606d1a78765b`.
Frozen scripts run with the authoring `tools` directory on Python's module
path; a frozen script's work directory alone cannot resolve shared helpers.
The final source-comparison, original-shape renderer and face-direction
inspection scripts are also frozen beside the receipts. These authoring files
remain outside the CSS Git repository.

Flow2 and contact1 blends were copied to the external rejected-fit archive,
SHA-256 verified and removed locally, reclaiming 166,280,034 bytes. The source
archives, accepted body, installed packages, settings and saves are unchanged.
All supplied palette templates and the black-and-white floral default remain
required. No V2 trial is installed and no game test is requested.

### Earlier glove baseline and local contact comparison

`glove-old-baseline1.json` applies the same neutral accepted-body check to the
earlier bound `new-outfits-gloves5` primary gloves. It finds 160/233 garment/body
edge crossings on the left and 157/253 on the right, with 25/24 vertices inside
the body. The deepest points are 0.345/0.337 cm inside near the index knuckles.
The earlier limited rendered acceptance did not establish whole-glove contact
clearance. Do not describe copying that fitted geometry as a verified repair.
This audit reads its neutral Basis, not a posed or morphed result.

`glove-source-comparison3.json` makes matched UV ordering deterministic and
records the twelve largest fitted differences per hand. The largest difference
on both sides uses UV `(0.389160, 0.971344)`: the old fitted points are at
approximately `(±39.498, -17.14, 114.85)` cm and the V2 points at
`(±36.613, -17.02, 113.19)` cm. The point is in the wrist region; its membership
in the inner closure has not been verified. Most matched source points remain
within the previously reported sub-2-mm 95th-percentile difference.

`glove-local-contact1` tests local vertex constraints on `glove-fit5`, instead
of a spatial field shared by neighboring fingers. Contact drivers are the
verified thumb and main-shell UV islands. The separate inner wrist closure
is not a driver. The recipe welds coincident source seams, includes exact
garment-edge/body intersections, limits each step to 0.5 mm and total movement
to 6 mm, and checks per-step triangle area and orientation. Ornaments follow
the corresponding primary shell displacement. Maximum movement is 4.132 mm
on the left and 4.852 mm on the right.

This candidate is still unaccepted. The main shells retain 133/183 and 135/183
garment/body crossings, compared with fit5's 150/197 and 145/194. Thumbs have
zero crossings in both versions; wrist closures still intersect the body.
The left front, rear and side views were inspected. A small skin breakthrough
remains near the knuckle in the side view. The 98-object saved-file preservation
check passes, including the accepted body, all previous source shapes, UVs and
weights. A numerical improvement alone does not establish a finished glove.

The cumulative baseline remains `neckline-contact3`. The next investigation
must isolate the remaining visible knuckle contact and its source/body
correspondence before another fitting change. Do not increase whole-hand
expansion iterations or assume all remaining crossings are hidden surfaces.
Binding, all six body controls, all supplied palette templates, motion and
runtime verification remain open.

`glove-study3-receipt.json` freezes the comparison, old-baseline audit, local
contact recipe and verification reports. Frozen local-contact recipe SHA-256:
`ed5c170f7e880ae87dd6aeb97011a0b8e9c296b9948afe63cdbb9df5f21c48ee`.
These authoring scripts remain outside the CSS Git repository. The rejected
blend was copied to `/run/media/eins0fx/theark/CSS/peony-v2-rejected-20261011/`,
verified against SHA-256
`0159d44e4da75713e49d1612ab298786da7b76c5a6bbaa7be04ca73754d25a16`,
then removed locally, reclaiming 83,110,908 bytes. Logs and rendered evidence
are retained. The initial render invocation omitted the required source-library
argument and failed before creating output; the corrected invocation completed.
Installed files, saves and settings are unchanged. No V2 trial is installed.
