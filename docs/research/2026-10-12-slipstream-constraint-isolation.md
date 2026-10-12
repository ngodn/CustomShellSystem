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
