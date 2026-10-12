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

## Next decision

Inspect the backstop and self-collision geometry at the stretched edges, then
compare visible garment penetration as well as extension for a targeted repair.
Keep body collision and the accepted source fit as requirements. The existing
native18 topology improvement and earlier rejected travel/weight experiments
remain relevant; these results do not justify repeating broad global tuning.

The two command receipts, captures, anchor checks and strain reports are retained
for reproduction. No settings, renderer options or installed assets were changed.
