# Peony target transfer and posed contact

Private authoring work on October 10, 2026. No result in this note is an
installed repair or a release candidate. Eve remains first in the full
three-outfit goal. No game launch or user test is needed for these checks.

Evidence paths below are relative to
`CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/work`.

## Body transfer and inpainted targets

`peony-surface-transfer1` compares the existing proxy skin weights with a fresh
barycentric transfer from the final body. The existing garment already uses
body-surface transfer. Repeating that operation is not a missing repair.
Median weight L1 difference is 0.000672; the 95th percentile is 0.015946.
Both fields fail the target-edge screen in all 145 sprint frames. Worst edge
extension is 10.645 cm with existing weights and 10.699 cm after fresh transfer.

The problematic loose points are several centimeters from the body. At frame
88, edge 597/598 measures 13.867 cm with existing weights, 13.852 cm after fresh
transfer and 3.657 cm at rest. Its matched body-surface points are 12.191 cm
apart. Those points are not necessarily neighbors on the body topology, so
this is not proof that the body mesh or skeleton is broken.

Epic describes the limitations of nearest-surface transfer for loose clothing
and using matched regions to fill unmatched weights in its
[Panel Cloth transfer tutorial](https://dev.epicgames.com/community/learning/tutorials/Dl20/unreal-engine-panel-cloth-transfer-skin-weights-node).
The private `prepare_peony_inpaint_targets.py` trial uses a positive harmonic
graph solve, not UE's biharmonic implementation. It changes 344 loose vertices,
preserves all 32 native pins and uses nearby fitted vertices as fixed boundaries.
The target-only sprint screen improves from 145 to 79 failing frames, with
worst extension reduced to 5.926 cm. That does not establish simulated fit.

The actual cloth12 native simulations still fail:

| Clip | Failing frames / 145 | Maximum edge ratio |
| --- | ---: | ---: |
| Walk | 57 | 5.780 |
| Jog | 108 | 6.742 |
| Sprint | 140 | 10.926 |

All 32 native anchors reconstruct within 0.000025 cm. Fresh inspection preserves
render geometry, section morphs and mesh/skeleton/physics/secondary references.
The inspector does not independently read back free-particle skin influences.
Cloth12 is rejected as a repair. Its evidence and 109 editor assets are hashed
in `peony-cloth12/study-receipt.json`.

## Travel and drive controls

`peony-distance-trace1` uses the exact cloth8 assets. The additive native
`peony-travel-scale-probe.patch` changes only an offline preview component.
Its default remains 1, matching UE 5.6.1's component constructor. The patch
replays exactly, builds successfully and reads back the selected scale.
No asset or installed runtime setting is changed by this diagnostic.

| Sprint case | Failing frames / 145 | Maximum edge ratio |
| --- | ---: | ---: |
| Travel scale 1 | 106 | 8.318 |
| Travel scale 4 | 106 | 8.336 |
| Scale 4, no backstop | 102 | 8.294 |
| Scale 4, no backstop or animation drive | 109 | 7.131 |

All four native runs complete successfully and all 32 anchor checks pass.
Body collision and self-collision remain enabled. Allowing up to 120 cm instead
of 30 cm travel does not remove the defect. Neither does removing animation
drive after the travel/backstop controls.

The scale-1 repeat has the same bone poses and identical cloth positions in
frames 0 and 1 as the older capture. Subsequent positions diverge, and its
failing-frame count is 106 instead of 109. Do not claim bitwise deterministic
simulation or interpret a small failing-frame reduction as an improvement.
Both captures reproduce the same large maximum stretch.

## Collision centers during sprint

`fit_peony_posed_leg_collision.py` reconstructs the final body from recorded
poses, then queries 65 positions along each capsule axis and the standalone
sphere centers. Three-axis ray parity checks containment. The first 25-pose
sprint sample has 414 outside-center observations; all three rays agree on
those observations. The worst sampled surface/radius margin is -5.966 cm.
Consequently, changing radii alone cannot contain these existing axes.

A bounded center-position search improves the individual sphere clearances
but leaves 20 outside-axis observations on the left calf connection at frames
36 and 132. It emits no candidate. No proposed center movement was imported.
The radius projections from infeasible configurations can become negative;
those numbers are rejection evidence, never authoring inputs. A subsequent
fit must enforce positive radii and check the connected axes, not just sphere
centers. This sampled test does not prove continuous containment or cloth
coverage, and shrinking collision to pass a strain screen is not acceptance.

The cloth12 and distance-trace receipts were verified before removing only
their stopped private editor caches: 75,104,591 logical bytes. Immediate free
space did not increase on this filesystem. Sources, editor assets, reports
and installed packages were retained.

## Reference-pose lead-in

The motion harness resets cloth directly into sprint's first bent-leg pose.
That differs from entering sprint through a transition. The private startup
probe creates a transient, one-second sequence from the mesh reference pose
to the measured sprint pose. It retains all lead-in particle positions and
does not reset cloth again when the actual sprint begins. This tests
initialization sensitivity, not the game's actual locomotion blend.

The first two attempts stop at the pose-validation guard. Native animation
compression changes pelvis translation by about 0.003 cm, beyond the requested
0.001 transform tolerance. The third uses UE's raw-data debug option only for
the transient sequence and restores the prior setting before normal playback.
Every raw bone is checked against the authored blend on all 60 lead-in steps.
The final module builds and the native run exits successfully.

`peony-startup-trace3` still fails 103 of 145 sprint frames, with maximum edge
ratio 8.124 and worst extension 13.537 cm. All 32 sprint anchors reconstruct
within 0.000022 cm. The lead-in itself fails the strain screen in 51 of 60
frames, starting at frame 6, with maximum ratio 6.037. Those lead-in rows have
particle output and native local-pose checks, not independent anchor checks.
The abrupt initial pose therefore does not account for the whole defect.

## Next work

Continue with connected collision fit against the moving body. A radius-only
change on the current axes cannot satisfy containment. Preserve the rejected
controls so target transfer, travel limits and startup are not retried without
new evidence. No collider or cloth candidate from this comparison is accepted.

Preserve full cloth simulation, body contact, accepted outfit shape, body
controls and materials. Final material/Windows cook, CINE, doubles, gameplay,
performance and release checks remain required by the parent goal.
