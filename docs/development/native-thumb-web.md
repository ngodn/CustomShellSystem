# Native thumb-web stage, 2026-09-21

The V2 [thumb-web correction](thumb-web-contact.md) now executes in a saved
native Control Rig with the 16 original corrective curves. It passes 1,143
execution cases. All 569 measured native pose/curve outputs also pass actual
V44B2 Blender skin checks with exactly the four neutral seam pairs.

This is a verified stage, not the complete native hand pipeline. Inputs already
contain calibration, original-Eve hinge projection with experimental proximal
swing gain 0.125, and finger/tip-clearance results. Articulation and the
directional solvers still need native integration. V43 remains installed;
original proportions, accepted dynamics and disabled CSSX are preserved. The
full modular Next-Gen runtime, UI, pipeline and live acceptance remain active.

## Contract and graph

Saved asset: `/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandWebCorrectivesV3`.
The asset version distinguishes the corrected graph default; it still uses
the V2 web-boundary parameters. It contains 223 shipped native RigVM/Control
Rig nodes and no custom game class or runtime Python.

`Enabled` defaults false. Enabled evaluation snapshots the incoming thumb-base
local transform, converts its rotation to original-Eve coordinates, applies
the fitted Z boundary, transports back and updates only `thumb_01_l`.
The 16 corrective curves evaluate afterward, as desired source weight minus
the mesh's baked weight. The input snapshot prevents lazy graph reads from
feeding the corrected rotation back into the same evaluation.

Disabled evaluation preserves bones and clears all 16 private curves plus
`WebCorrectionDegrees`. An unrelated seeded curve is preserved. An enabled
case requiring no web correction keeps the incoming rotation exactly. All
translations, scales and unrelated local rotations stay unchanged in the
tested hierarchy. Every evaluation receives a fresh input pose.

The graph imports the independently checked B diagnostic hierarchy, as the
earlier calibration graph does. B2 full-mesh import remains a separate gate.
Fixture seeding uses captured rotations with the imported hierarchy's initial
translations/scales. The skin replay uses B2 translations and retained fixture
scales. This does not validate arbitrary scaled hand hierarchies or the five
B/B2 attachment-leaf differences.

## Native evidence

Fixtures comprise the 464 saved poses and 105 transition samples from the
offline guard check. Each is evaluated normally and with the incoming thumb
quaternion negated, giving 1,138 executions. Five additional checks cover
disable, re-enable, repeated fresh input and a new instance disabled/enabled.

- Maximum thumb rotation error: 0.00002354 degrees, gate 0.001.
- Maximum corrective-curve error: 0.000000771, gate 0.00001.
- Maximum correction-value error: 0.00002232 degrees, gate 0.001.
- All 379 local transforms are checked for preservation outside the allowed
  thumb rotation, along with every translation/scale and the unrelated curve.
- The canonical Skeleton and production V43 mesh hashes remain unchanged.

`hand-native-web-fixtures-v1` contains the prepared inputs and expectations.
`hand-native-web-rig-v2` contains the saved graph, 1,143-case report, protected
hash checks and terminal exit 0. Fixture SHA-256:
`792df84389d8988a9f9be32d4fdf9ab69a465418241f27ff90afa125eb45cce8`.

The earlier `hand-native-web-rig-v1` also passes the math, but Unreal warns
that `()` is not a valid member default for a standalone FTransform. The final
builder uses the same empty/default convention as the existing rig tooling.
The corrected graph is saved under V3; the earlier V2 graph and warning log
remain diagnostic evidence. No production asset was overwritten.

## Skin and cost

`hand-native-web-skin-v1` inserts each measured native thumb output into the
original fixture and drives shapes from the measured native curve values.
It does not recompute the expected web correction as a substitute for native
output. All 569 actual Blender skin checks have exactly the neutral four
pairs, terminal exit 0. This verifies the stage on retained inputs, not a
complete fresh calibration/clearance evaluation or weapon contact.

Both measured-output hand renders in
`CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/work/nextgen-audit/`
`native-thumb-web-v3-after-render-v1/` were reviewed from opposite sides. The
thumb is separated from the index web and the fingers remain individually
visible. These isolated, neutral-material views do not show weapon contact,
full arm posture or motion.

Four alternating editor timing blocks contain 800 measured executions per
variant after warmup. Fixture reset is outside the timer; both variants include
the same Python `execute` bridge. Correctives alone measure 17.413 microseconds
median / 28.400 p95; web plus correctives measure 19.320 / 30.987. These are
isolated editor measurements on one active input, not a live FPS claim or
the cost of calibration, directional clearance and all body/hair dynamics.
Raw samples are retained in `hand-native-web-rig-v2/timing.json`.

## Next integration work

The required order is calibration, articulation, four-finger clearance,
thumb-tip clearance, web protection, then corrective curves. Articulation
projects ten distal joints onto their original source X hinges and scales
swing on four non-thumb proximal joints; thumb-base motion and independent
curls survive. The 0.125 proximal gain is experimental, not a final accepted
setting. Never feed calibration alone directly into the clearance fixtures.

Use the V44B2 geometry fit to recompute the preceding directional stages,
check their resulting skin and transitions, and implement them natively before
combining calibration, articulation, clearance, web protection and corrective
curves. Measure
the combined cost. Complete full B2 import/cook, weapon and sidearm coverage,
and visible gameplay/lifecycle acceptance before deployment.

Evidence paths above are under `work/grip-grounding-v1/`. Reproduction tools
are in [hand-transfer probes](../../tools/authoring-probes/hand-transfer/README.md).
