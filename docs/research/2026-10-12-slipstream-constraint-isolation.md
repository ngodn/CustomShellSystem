# Slipstream continuous-cloth constraint isolation

Status: private diagnostic, no garment or installed package change. Continue
from the retained native18 continuous surface, not a rejected earlier proxy.

## Setup and preservation

`tools/diagnostics/run_slipstream_constraint_isolation.py` reuses Eve's
`work/slipstream-cloth-native18` project and its pinned editor module. Both
batches exit 0 and report no changed source assets. The content directory is
mounted read-only; no full project copies or new cooks are made.

The module SHA-256 is
`325c74c14e539125d3592a1072b787155ca3c1c71c47dd337867c227e3dc766d`.
The proxy is `slipstream-cloth-continuous2/proxy.json`, SHA-256
`2cb0631e12f178899fa385332aaad03e39bd2994cddd098a125e559ba27f828f`.
It has 640 particles and 1,152 triangles. The private stage retains its older
secondary body rig for this comparison; final integration must use the accepted
Peony motion successor rather than restoring the older wide limits.

Evidence is in CSS's `work/three-outfit-rig-physics/`:

- `slipstream-continuous-isolation1`: baseline, no body collision, no backstop,
  no self-collision, no animation drive.
- `slipstream-continuous-isolation2`: the same cases in reverse order.

Each case captures 145 sprint frames at 60 fps with secondary motion enabled.
All ten captures contain identical animated poses. Actual diagnostic flags match
the requested cases. Independent Blender verification passes all 128 particles
below the 0.1 cm kinematic threshold; maximum anchor error is below 0.000025 cm.
The strain audit validates those receipts before evaluating edge extension.

## Results

The screen rejects an edge only when it exceeds both twice its rest length and
one centimetre of additional length. Passing is not complete fit acceptance.

| Private case | Failed frames, first / reverse batch | Maximum edge ratio, first / reverse |
| --- | --- | --- |
| Baseline | 8 / 5 | 2.490756 / 2.161212 |
| No body collision | 1 / 1 | 2.013803 / 2.013803 |
| No backstop | 0 / 0 | 1.933402 / 1.960513 |
| No self-collision | 0 / 0 | 1.999752 / 1.999752 |
| No animation drive | 11 / 2 | 2.255398 / 2.070078 |

The no-self-collision particle positions match exactly across batches. Other
cases differ, with maximum position differences of 6.92 to 12.08 cm, despite
identical animated poses. Thus exact failure counts are not stable, and the first
batch's apparent worsening without animation drive is not a repeatable conclusion.
Changing order and rerunning together do not isolate the source of variability.

Both collision-related removals pass this particular screen twice. This supports
investigating constraint interaction, not shipping with either category disabled.
No-self-collision is especially close to the ratio threshold. Maximum absolute
edge extension remains about 3.6114 cm in every case, illustrating the screen's
limited scope. Timings are retained but were not captured under controlled system
load and are not a game performance comparison.

## Contact check and radius trial

`slipstream-continuous-contact1` replays frames 0, 54 and 119 from the reverse
batch against the exact same posed body. It reconstructs the visible garment
through the saved native render mapping. The rest-position replay error stays
below 0.027 cm. The frozen `recipe.py` matches the hash in `report.json`.

The contact metric is signed distance to the nearest oriented body surface,
not closed-volume containment. Counts below sample garment vertices and triangle
centroids more than 0.5 cm inward. The pose checker verifies body and garment
skinning before measuring contact.

| Frame | Baseline | No backstop | No self-collision |
| --- | ---: | ---: | ---: |
| 0 | 308 | 531 | 310 |
| 54 | 1,759 | 5,142 | 2,196 |
| 119 | 1,482 | 3,555 | 1,501 |

Removing backstops substantially worsens this contact screen despite passing
the earlier stretch screen. Keep them enabled. At frame 54, the worst baseline
edge (428 to 438, ratio 2.1612) has endpoints about 5.46 and 3.95 cm inward from
the nearest body surface. Both are outside their reconstructed 3 cm backstop
spheres by about 7.3 cm. A small sphere permits movement around its side and
behind the body; being outside that sphere does not establish body clearance.

The saved physical normals match the authored proxy normals exactly. The saved
radius is 3 cm, distance 0.3 cm and legacy mode is false. The installed UE 5.6.1
source in `Chaos/PBDSphericalConstraint.h` places the nonlegacy center at
`skin - (radius + distance) * normal` and projects only particles inside that
sphere. This agrees with the purpose described in Epic's
[clothing tool documentation](https://dev.epicgames.com/documentation/unreal-engine/clothing-tool-in-unreal-engine).

For a captured displacement, let `v = actual - skin + distance * normal`.
When `dot(v, normal) < 0`, the radius must exceed
`dot(v, v) / (-2 * dot(v, normal))` to exclude that position. The two frame-54
witnesses require about 14.73 and 14.05 cm. A private 16 cm radius-only saved
trial tests this prediction with both collision systems retained. Other sampled
points need much larger radii or lie on the outward side of the backstop plane;
radius alone is not expected to settle every contact defect.

The trial uses a writable overlay of the 8.5 MB garment mesh directory. Other
source content is read-only, and hashes are checked after every command. It
does not copy the full authoring project or install a game package. The recipe
is `tools/diagnostics/run_slipstream_backstop_trial.py`.

The saved trial passes fresh readback: render geometry (including weights and
mapping), six cloth-section morphs, skeleton, physics and secondary graph are
unchanged. The only inspected cloth field that changes is `backstop_radii`;
the rebuilt cloth section also receives a new identifier. The first inspection
omitted the geometry/morph export flags and was repeated with both flags before
verification or simulation. Its earlier receipt is retained.

Both 145-frame secondary sprint runs retain body collision, backstops and
self-collision. All 128 anchors pass below 0.000025 cm. Each run fails the
stretch screen in 52 frames, with maximum ratio 3.23636 and maximum extension
3.85389 cm. Visible contact improves substantially, but not everywhere:

| Frame | 3 cm baseline inward samples | 16 cm run 1 / run 2 | 16 cm worst signed distance, run 1 / run 2 |
| --- | ---: | ---: | ---: |
| 0 | 308 | 208 / 208 | -2.907 / -2.907 cm |
| 54 | 1,759 | 310 / 310 | -6.870 / -6.870 cm |
| 119 | 1,482 | 216 / 220 | -6.412 / -6.554 cm |

The repeat positions still vary, by up to 4.25 cm. Counts alone conceal the
remaining deep contact points. Matched frame-119 front/side/back renders in
`slipstream-backstop16/review119` and `slipstream-continuous-contact1/review119`
confirm much better rear coverage, but the trial still has visible front-leg
penetration. These are diagnostic colors and offline native-mapping replays,
not final material or game acceptance. Keep the result as a useful direction,
not an installable fix.

An additional native frame-119 contact capture exits zero with protected sources
unchanged. Its 32 body collision shapes are spheres. For sampled stretched-edge
endpoints 169, 179, 428, 438, 429 and 439, native minimum distances range from
2.71 to 10.48 cm outside the collision shapes. The target positions are also
outside. Thus these particular final-state edges are not directly blocked by
a body collider at that instant. This does not exclude earlier collisions or
forces transmitted through neighboring particles. Do not confuse the original
physics used here with the rejected fitted-sphere/level-set trials in the older
binding record.

Two runtime-only 16-iteration controls keep the same saved radius trial and
all collision systems. Native readback confirms 16 iterations; anchors still
pass below 0.000025 cm. They fail strain in 55/56 frames, maximum ratio
2.96258/2.94594. Frame-119 inward visible samples are 237/242, versus 216/220 at
eight iterations. This is not a solution. Mean captured cloth step time rises
from roughly 2.45/2.53 ms to 4.27/4.05 ms, but these runs were not a controlled
performance benchmark. No iteration change is saved to the mesh.

The trial occupies about 217 MiB, mostly five native motion captures. It creates no
new cook, engine build, Wine prefix or complete project copy. Retain the small
mesh overlay as the current diagnostic input for constraint/coverage work and
the captures needed to reproduce these comparisons. Root and theark free space
round to 127 GB and 82 GB both before and after this experiment.

## Next decision

Inspect constraint compatibility at the still-stretched rear pair 429/439 and
the front pair 169/179, then address front-leg collision coverage. The latter
pair's skinned target already reaches a 3.209 ratio at frame 119. Separate that
target deformation from solver-induced extension; the rear pair's skin ratio
is only about 0.986. The radius and iteration results do not justify another
blanket stiffness or iteration increase.
Keep body collision and the accepted source fit as requirements. The existing
native18 topology improvement and earlier rejected travel/weight experiments
remain relevant; these results do not justify repeating broad global tuning.

The two command receipts, captures, anchor checks and strain reports are retained
for reproduction. No settings, renderer options or installed assets were changed.

## Ordered replay and surface-normal trial

The retained native13 ordered-copy module is reused through a read-only binary
overlay, with the saved 16 cm mesh and current dependencies. Its embedded-motion
source differs from the current mapping harness only in the independent ordered
copies and tether export. The module hash is
`19b0ceebbdda709e5247c6ce6636e08d5a7c75bfd8cbcd51bb0cf6d638373eed`.
`slipstream-backstop16/ordered-trace.exit.json` records exit zero and no changed
source/candidate assets. All 39 poses match the regular capture; particle
positions match exactly at frame zero and differ by at most 0.00390 cm at
frame 38. Keep the different module and non-identical later state explicit.

At frame 38, front edge 169/179 has a 1.72329 cm rest length and a 4.37705 cm
captured length. A copied edge pass shortens it to 3.13425 cm. Applying backstop
afterward opens it to 4.22267 cm. In comparison, area gives 2.91766 cm, tethers
3.08923 cm, maximum distance 3.13425 cm and animation drive 3.14035 cm. This
identifies a concrete backstop conflict in that final-state replay; it is not
a measurement of live solver ordering. The rear pair 429/439 has a different
response and does not show that same frame-38 conflict.

The two front reference normals were transferred from nearby thigh surfaces:
approximately `(-0.460, -0.887, -0.018)` and `(0.405, -0.912, -0.069)`.
Their dot product is 0.624. Averaged unit-face normals from the continuous
skirt surface give a dot product of 0.996 there. This motivates changing the
physical backstop directions, rather than repeating the earlier rejected pelvis
weight fade. Skin weights remain a separate possible source of target stretch.

`slipstream-surfacenormals16` tests only those physical normals against the
16 cm-radius candidate. Geometry, weights, travel, distance, collision settings
and body-shape deltas are intended to remain unchanged and are checked on fresh
native readback. The small overlay runner now accepts `--surface-normals` and
an explicit private output directory. Fresh readback confirms only physical
normals change (plus the rebuilt cloth identifier); all those preservation
checks pass. Both captures retain the same 145 baseline poses and pass all
128 anchors. This trial is not installed.

Reject the full-normal replacement: it fails strain in 74/75 frames with maximum
ratios 3.81078/3.77243 and maximum extension about 4.59 cm. At frame 119 it has
244/284 inward garment samples, versus 216/220 with the original normals and
16 cm radius. Its worst signed depth improves to about 5.42/5.66 cm from
6.41/6.55 cm, but neither this nor the frame-zero contact improvement establishes
a better overall garment. The front pair's frame-38 ratio improves from 2.540
to 2.248, while its worst ratio over the full stride worsens from 3.236 to
3.664. Thus a local normal edit is not justified solely by that one-frame gain.

The next diagnostic extends the retained ordered-copy module with an explicit
`ConstraintFrame` argument. It keeps the existing frame-zero probe and defaults
to frame 38 when no override is supplied. This lets the same actual sprint
capture expose the rear failure at frame 119 without changing animation start
time or copying a full content project. Source and build recipe are retained in
`slipstream-constraint-frame1`; it is private diagnostic code, not a CSS core or
gameplay change.

The probe build passes after correcting declaration order in the new argument
validation. The first owned build was cancelled and retained separately; no
game or asset build was interrupted. The successful module SHA-256 is
`2311ac816a6915fbc8edbe1c6b76ddb3615768f688b05491641f15495abe345c`.
The 145-frame `ordered119.json` capture exits zero, preserves source/candidate
assets and exports ordered copies at exactly frames 0 and 119. All poses match
the regular radius trial. `ordered119-summary.json` records the measurements.

At frame 119, the front pair measures 4.64023 cm. The edge pass reduces it to
3.95178 cm, then maximum distance reopens it to 4.49603 cm. Backstop leaves
that particular correction unchanged. The rear pair measures 2.99277 cm;
one global edge pass actually leaves it at 3.04133 cm, a second reduces it to
2.70103 cm, and backstop after one pass gives 3.13110 cm. Its neighbors matter;
do not describe a single edge projection as an isolated two-particle solve.
Together with the frame-38 result, this rules out treating one backstop-normal
edit as a complete explanation or repair.

## Preserve cutouts while repairing vertex fans

The radial annulus deliberately bridges authored openings. That is a different
connectivity from the fitted fabric. Before more parameter changes, investigate
whether the original surface can be made manifold without those bridges.
`tools/diagnostics/split_slipstream_proxy_fans.py` duplicates a shared vertex
when its incident faces form disconnected fans. Its bow-tie control checks that
two sheets touching at one point become separate valid corners.

`slipstream-splitfans1` changes 624 particles to 674 and removes all 49 invalid
vertex fans. All 976 triangles retain exactly the same corner coordinates;
normals, weights, travel, backstop maps and all six morphs are copied from each
original donor. It exposes 20 components, two without a native pin. Do not
simulate this unanchored version as a usable garment.

Those two islands have only two and one triangles, areas 0.13513 and 0.10006
square centimetres. The independent Blender support check samples their
vertices, edge midpoints and triangle centroids against the remaining surface:
21 samples, maximum distance 0.16820 cm. This is sampled proxy support, not a
bound on every rendered point or a successful native mapping.

`slipstream-splitfans2` removes only those three simulation sliver triangles
and their seven now-unused particle instances. It has 667 particles, 973
triangles, valid edge/vertex topology and 18 components, all with native pins.
Every retained triangle corner and particle attribute still matches its original
donor. No visible garment triangles or material sections are removed, and no
new triangles bridge the cutouts. The recipe and output hashes are retained.

Next bind this candidate to the preserved garment, verify mapping and morphs,
then compare native motion and body contact. Topology alone does not establish
that it fixes the observed garment, and the older surface's known mapping errors
must be checked rather than inherited. The accepted Peony installation remains
untouched; the full outfit goal stays active.
