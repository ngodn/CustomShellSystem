# Peony hip accessory fit

The user's circled flaps belong to material 29,
`Peony_hip_accessories_Lower_1`. They are distinct from the simulated fabric
sections in slots 34/38. This is an outfit geometry issue, not a CSS UI setting.

Evidence lives under Eve's `work/peony-panelattachment2` and
`work/peony-hipfit1`. Source outfits and installed game packages are unchanged.

## Cause and scope

`side-piece-trace.json` records orthographic pixel-to-triangle queries against
the posed garment mesh. The circled broad flaps resolve to material 29.
The initial single-point probes hit the arm or stockings and were insufficient
to identify the whole piece. The subsequent grid resolves the accessory.

`tools/fit_peony_lower.py` fits each same-side hip-accessory cluster with one
outward translation: the largest lateral body-clearance correction plus 0.8 cm.
That preserves the accessory's shape, but does not constrain its upper mounting
edge to the corset. In the assembled mesh, the upper anchor of each main plate
is 2.4528 cm / 3.2282 cm from the nearest corset surface. The anchor is defined
as the mean of the plate's top 10% of vertices by height. These distances are
measurements of that anchor, not a claim about every point on the edge.

`hip-components.json` records connected components, bounds, anchors and weights.
There are 1,420 material-29 points, including two main plates of 320 points each.
Eighty-two lower-edge points, 41 on each main plate, use the root bone.
`source-root-weights.json` traces these back to the extracted source's `Root`
influences. They were not introduced by CSS's bone-name mapping. Their motion
still needs review; no weight change has been made in this candidate.

## Closer-fit candidate, not accepted

`peony-hipfit1/fit.py` moves each material-29 assembly rigidly toward the measured
corset mount, stopping at a 0.25 cm anchor gap. Its receipt verifies 1,420 changed
positions and preservation of every other field, including weights, morph deltas,
UVs, normals and topology. Other material positions remain identical.

The candidate is `peony-hipfit1/peony-character.mesh.json`, SHA-256
`262a4f1f1615e42fe407c143142a9ae05cf74b19efed6ebb6e41ef139d59543e`.
It starts from the frozen `peony-panelfacial1` base interchange. This does not
constitute an updated native facial mesh or a cooked outfit.

`rest-review` and `walk0-review` contain front, back, side and quarter views.
Walking uses the original captured walk frame 0, without the earlier six-degree
arm experiment. These are neutral Workbench views without cloth replay or the
final opacity/body-mask setup. The mounting gap is visibly reduced, but accessory
intersections remain. Anchor distance alone is insufficient for acceptance.

Next work: fit the mounting region without pushing the lower accessory through
adjacent surfaces, inspect the inherited root-weighted edge during motion, and
check the resulting fit through body shapes and gait poses. Preserve the accepted
lower cloth candidate and facial data. Do not promote this draft or ask for a
game restart yet.
