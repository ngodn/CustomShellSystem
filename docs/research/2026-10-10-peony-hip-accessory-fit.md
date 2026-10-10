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

## Follow-up: attachment rotation and weight defect

The next trial (`peony-hipfit2`) rotates each assembly toward its upper mount
around its lower edge, instead of translating the whole accessory inward.
Right/left rotation is +4.9933/-6.3688 degrees about Y. Small Y/Z translations
complete the mount alignment; the lower pivot's X stays unchanged. Both measured
upper anchors remain 0.25 cm from the corset. Positions, normals and positional
morph deltas receive the same rotation. Edge lengths remain unchanged within
0.00001 cm. This preserves the original attachment shape, but does not establish
motion or contact acceptance.

`peony-hipfit2/audit_motion.py` skins material 29 through all 145 captured frames
of each gait, using the upstream animation snapshots. It measures every edge.
The severe-stretch screen requires both a ratio above 2 and extension above 1 cm.
It is a deformation screen, not a complete visual quality criterion.

| Candidate | Walk failing frames / peak extension | Jog | Sprint |
| --- | --- | --- | --- |
| Original fitted assembly | 135 / 12.876 cm | 145 / 18.665 cm | 145 / 19.304 cm |
| Attachment rotation, original weights | 135 / 12.442 cm | 145 / 18.658 cm | 145 / 19.529 cm |
| Local harmonic repair of 82 root-weighted points | 135 / 6.678 cm | 145 / 8.207 cm | 145 / 12.364 cm |
| Uniform pelvis attachment | 0 / below 0.000001 cm | 0 / below 0.000001 cm | 0 / below 0.000001 cm |
| Uniform source-derived hip/thigh blend per side | 0 / no positive extension | 0 / no positive extension | 0 / no positive extension |
| Underlying body-surface weights | 0 / 2.848 cm | 0 / 2.988 cm | 0 / 3.839 cm |

The original and rotation-only candidates' worst edges include root-weighted
points. Filling only those points from neighbouring weights is insufficient.
It reduces the worst deformation but leaves severe stretching. The uniform
candidates eliminate stretching but introduce sampled thigh contact during sprint.
They are rejected as complete repairs.

### Current private candidate

`peony-hipfit6` retains the attachment rotation and transfers weights from the
underlying torso/leg surface. The transfer excludes arm-weighted triangles.
The large plates use interpolated surface weights; each small attached component
uses its mean surface weights to avoid internal distortion. Only material 29's
1,420 points change weights, with at most five influences per point. Skeleton,
all other geometry/weights and cloth sections remain unchanged. Body-surface
weights are a candidate treatment for these leather accessories, not a global
outfit rebind.

Its interchange SHA-256 is
`6989b7c5a02af0f3720ca08987214411d9119e65ea21187b8f352a4e86def9ae`.
All 435 sampled gait frames pass the severe-stretch screen. This still permits
the smaller measured extensions above; do not describe the geometry as perfectly
rigid or all deformation as eliminated.

The contact script compares unique accessory points against the same posed body,
using whole-body winding and a 0.02 cm penetration threshold:

| Pose | Original fitted assembly | Body-surface candidate |
| --- | --- | --- |
| Bind | 0 inside | 0 inside |
| Walk frame 128 | 57 inside, 1.8654 cm deepest | 95 inside, 2.1935 cm deepest |
| Sprint frame 6 | 0 inside | 0 inside |

`walk-contact-regions.json` separately sums the arm-triangle contribution to
whole-body winding. All 57 original and all 95 candidate witnesses have an
arm contribution above 0.55 in magnitude; none has a remaining-body contribution
above 0.55. The arm and remainder are open subsets, so these contributions locate
the overlap rather than certify separate closed volumes. Whole-body winding
remains the containment test. Nearest triangle labels alone were misleading:
some witnesses inside the arm were nearer a hip triangle.

The current candidate clears the sampled sprint contact and fixes the measured
mounting gap and severe root-weight deformation, but worsens the sampled walking
arm overlap. It is **not accepted for shipping**. The next fit/contact work must
address the arm envelope while retaining the closer attachment and corrected
skinning. Do not revert the weight repair to make a broken, root-pinned flap
appear clear. Do not change the shared gameplay skeleton or install a global
arm rotation as a shortcut.

Front/side/back/quarter reviews are saved with the original captured poses.
These remain Workbench views without final material opacity, body masks or cloth
replay. Native import/cook, facial regeneration, body-morph limits and live checks
have not been performed for this accessory candidate.

### Cleanup and recovery

Rejected `peony-hipfit1`, `3`, `4` and `5` mesh JSON files were losslessly gzip
compressed, then decompressed in a stream and SHA-256 compared before removing
the redundant raw JSON. Their `archive-receipt.json` files explain restoration.
Recipes, audit files, measurements and images remain. This reclaimed
411,962,997 bytes (about 393 MiB). The rotation input (`2`) and current candidate
(`6`) remain uncompressed for ongoing work. Original inputs, cooked packages
and installed files were not changed.

The first `peony-hipfit6/repair.py` run saved its mesh and audit, then failed
while serializing a NumPy integer into the receipt. The recovery run converts
that integer explicitly and verifies the existing mesh/audit bytes against its
fresh calculation before writing the missing receipt. It did not replace the
geometry being inspected. Both logs are retained.
