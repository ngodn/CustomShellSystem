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
