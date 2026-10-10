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

## Native preservation and body-slider follow-up

`peony-hipnative1` imports fit6, restores shared skeleton/material/secondary
graph bindings, rebuilds the two retained cloth sections and adds 115 facial
expressions. Seven commandlet runs completed with exit 0. The saved face has
137 morphs, including the original 22. Fresh readback compares all 115 expression
shapes against their authored input; retained error is 0.000095176 cm and omitted
deltas stay within the importer threshold plus float tolerance. Native cloth
inspection matches the combined baseline, allowing only new section GUIDs.
Normal/facial cloth inspections differ only in asset path. All 115 staged
dependencies remain byte-identical.

This is native editor preservation, not Windows cooking or gameplay acceptance.
The non-cloth accessory deltas were not separately read back in this first
native trial. Its `study-receipt.json` freezes 160 files before removing
84,907,063 logical bytes of stopped private caches.

### Rejected fit and pose probes

- `peony-hipfit7` pulls the upper accessory toward the body with a smoothed
  offset. It changes the silhouette and introduces bind/sprint contact:
  30/47 inside vertices. Walk has 144, with a shallower 1.347 cm maximum depth.
  It is rejected despite passing the coarse edge-stretch screen.
- `peony-hipfit8` rotates the accessories forward 30 degrees, restores their
  corset mounts and refreshes surface weights. Walk contact falls to 48 points,
  but bind/sprint also have 48 contacts and jog/sprint fail the edge screen in
  12/63 frames. It is rejected. Fit7, fit8 and fit8's placement intermediate
  were losslessly compressed and decoded-hash verified, reclaiming 309,463,542
  bytes. Recipes, images, contact reports and restoration receipts remain.
- `peony-hipfit10` tests rearward placement with refreshed weights. At walk128,
  10/20/30 degrees rearward produce 66/114/151 contacts, with maximum depths
  2.001/2.466/2.531 cm. None clears this pose. No native asset was changed.
- `peony-hipfit12` tests lower mounting. Drops of 2/4 cm produce 101/103 walk
  contacts, at 2.301/2.275 cm. The 6 cm case exceeds the five-centimetre corset
  projection guard and aborts. The partial log is evidence only for the first
  two cases; there is no completed JSON result or accepted mesh. Blender exits
  zero after this Python exception, so its process exit is not a pass signal.
- `peony-hipfit6/arm-clearance-probe.json` rotates the upper arms only in an
  offline pose, tracking the original 95 witnesses. Even eight degrees leaves
  24 of them inside and moves the hands by about 6.2/6.7 cm. It does not count
  new contacts. No animation or runtime arm adjustment is adopted.

### Missing accessory morphs and the retained follow-up

Fit6's accessory has no entries in any of the 22 morph targets. The existing
Eve catalog generator defines body sliders from 0 to 1. A bind-pose comparison
finds 122 accessory points inside the body at Hip Width 1, with 1.502 cm maximum
depth, and 328 at all six body sliders set to 1, with 1.730 cm maximum depth.
Waist Width 1 has one shallow 0.040 cm contact.

`peony-hipfit9` transfers body morph offsets while preserving fit6 positions,
weights, topology, normals and every morph delta outside material 29. It clears
the sampled body contacts but pinches some triangles: Glute Size 1 produces a
minimum original-to-morphed normal dot of -0.334, and Hip Width 1 reduces one
triangle to 0.166 of its area. Contact alone does not accept this candidate.

`peony-hipfit11` smooths the transferred fields over each main plate with a 3 cm
Gaussian radius. Small hardware components follow the nearby plate correction
as a rigid translation. Neutral geometry and weights remain exactly fit6's;
other material morph deltas remain unchanged. Its mesh SHA-256 is
`5ece2a9a2ebf08c3583b496ab915091e71f8f062a9220ec36bcafb165b5a5efb`.

| Bind-pose case | Inside points | Minimum triangle area ratio | Minimum normal dot |
| --- | ---: | ---: | ---: |
| Neutral | 0 | 1.000 | 1.000 |
| Muscle definition 1 | 0 | 0.966 | 0.976 |
| Chest size 1 | 0 | 1.000 | 1.000 |
| Glute size 1 | 0 | 0.548 | 0.790 |
| Hip width 1 | 0 | 0.699 | 0.953 |
| Thigh definition 1 | 0 | 0.951 | 0.978 |
| Waist width 1 | 0 | 0.949 | 0.991 |
| All six at 1 | 0 | 0.730 | 0.925 |

The contact check uses full-body winding and a 0.02 cm depth threshold, with
no ambiguous vertices in these cases. Prior baseline records are reused only
after checking their input hash; the changed candidate is measured afresh.
Mount-anchor distances range from about 0.046 to 0.408 cm. The hip-max review
retains the pouch outline without the raw transfer's triangle fold. These are
neutral-material views, not final cloth/material renders.

Fit11 is retained for the body-slider repair. These checks do not cover all
slider combinations or posed shape extremes. Its unchanged neutral geometry
still has fit6's walking arm contact. `peony-hipnative2` stages this candidate
with 115 verified dependencies and the same pinned authoring module; native
rebuild/readback passes. Nothing is installed.

The fit11 native normal/facial meshes retain all 22 authored morphs. Indexed
readback verifies 55,175 affected import points and 105,705 built deltas, with
0.000007690 cm maximum retained error. The maximum omitted delta is 0.014997578
cm against a 0.015 cm filter threshold. Both meshes reject a deliberately
changed 0.1 cm expected delta through the same comparison. The verifier's
facial name is historical: this invocation supplies all 22 authored body and
corrective shapes, including the accessory deltas, from the interchange.

All 115 facial expressions also pass fresh readback. Both cloth sections match
the retained combined baseline, and normal/facial cloth data match each other.
The native asset hashes are:

- Normal: `39890b807eb693ab90f7a16b10c34ccbf8b502f69912cbbb7b6cf790a1b57734`
- Facial: `a077539c9aacb2402ee5403c2a475e4e107d0ba8044a3052705a9f4d3f4fb283`

Evidence is `peony-hipnative2/build-result.json`, `authored-morphs-readback.json`,
`face-reloaded.json` and `cloth-preservation.json`. This closes the native
morph/preservation check for fit11. It does not close neutral walking arm
contact, posed morph limits, Windows cooking, final material review, catalog
integration or gameplay. The broader three-outfit goal remains active.

All eight native commandlets completed with exit 0 before freezing this trial.
`peony-hipnative2/study-receipt.json` pins 170 files, including the preserved
assets, verifier inputs and negative controls. Process and lock checks preceded
removal of 85,442,521 logical bytes of its regenerable caches. Fit11's separate
`interchange-receipt.json` pins its 14 authoring/test/review files. Original
sources, releases and installed packages were not changed.
