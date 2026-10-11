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
