# Native directional finger clearance

The four-finger clearance stage now executes in a saved, disabled-by-default
Control Rig made from shipped nodes. It uses one correction step with a
1-degree central-difference sampling interval. All 934 native execution cases
pass the original 0.001-degree numerical gate and preservation checks.

This is a diagnostic stage, not a deployed V44 repair. The separately verified
[native thumb-tip stage](native-thumb-tip-clearance.md) now follows it. The
complete combined graph, cooked assets, all weapons and live acceptance
remain open. Original proportions, accepted body/hair settings, disabled CSSX
and the full modular Next-Gen goal remain preserved. V43 stays installed.

## Inputs and behavior

Input must already contain calibration and source-axis articulation. The B2
geometry model supplies directional radial hulls for three segments of each
non-thumb finger. The solver adjusts only four proximal splay rotations; it
preserves independent curls, the thumb, metacarpals, translations, scales and
all unrelated bones and curves. The single step is limited to 2 degrees.

The native graph snapshots local and parent transforms, evaluates segment
separation and directional support, computes the central-difference gradient,
then exports and applies the correction. It poses each radial once per trial
angle vector. A conservative capsule bound skips directional-support scans
that cannot change the selected violating pair. Unsupported non-unit hand
scales bypass the whole correction rather than relying on an untested shear
approximation. A fresh pose is required on every evaluation.

## Why the original 12-iteration port was rejected

The first completed port was functionally bounded and preserved other data,
but its difficult poses took about 10.7 ms median for 12 iterations in the
editor. It differed from the offline reference by up to 0.10671 degrees.
Although its measured outputs subsequently passed all 464 actual-skin checks,
that did not establish acceptable cost or strict numerical agreement.

Caching geometry alone did not remove the expensive cases. Conservative pair
pruning preserved all 933 applied poses and recorded angles exactly against
the preceding version. It reduced the observed no-correction median to about
0.55 ms, but difficult cases still took about 8.7 ms. Do not integrate that
12-iteration graph into the game on the strength of its skin pass.

Separate offline four-step and one-step candidates each pass all 464 retained
skin cases. This permits testing a bounded single-step implementation without
dropping poses or loosening the skin criterion. It does not prove every proxy
constraint is solved, or establish continuous-time contact for arbitrary poses.

The 0.05-degree derivative sampling step leaves up to 0.00626 degrees native
comparison error with one iteration. Sampling at 0.5 degrees reduces this to
0.001127, still failing. Sampling at 1 degree reduces the maximum to 0.000760
degrees, passing the unchanged gate. The sampling interval is part of this
candidate's model, not a user-facing slider or a change to the thumb solver.

## Verified native result

Asset: `/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftFingerClearanceV1_I1_E1p0`.
It contains 948 shipped nodes; no runtime Python, custom engine class or new
UE4SS per-frame callback is required. `hand-native-finger-clearance-one-v3`
contains the report, terminal exit 0, saved-asset hash and protected hashes.

The 464 poses are each evaluated normally and with quaternion antipodes.
Five lifecycle cases and an unsupported-scale bypass bring the total to 934.
Calculated rotation error is at most 0.000760 degrees. Applied rotation error
against the engine's small-write tolerance is at most 0.000089 degrees.
The verifier seeds B2 translations and captured scales explicitly, and checks
all 379 bones. Canonical Skeleton and V43 mesh hashes remain unchanged.

Observed editor execution is 0.610 ms median and 1.200 ms p95 across changing
fixtures, including the Python invocation bridge but excluding fixture setup.
These are separate-run measurements, not a paired benchmark, shipping-game
FPS or the complete hand/hair/body cost. Combined cost remains an acceptance
requirement.

## Measured-output skin and visual review

`hand-native-finger-clearance-skin-v2` inserts all four measured native finger
rotations into each of the 464 articulated/control fixtures, then recomputes
thumb-tip clearance, web protection and corrective shapes offline. Every case
has no additional skin intersections; Blender exits zero. The report hashes
its native input report and records that the numerical gate passed. This is
native finger clearance plus offline thumb stages, not the combined native rig.

Both opposite hand views of captured overlay pose 11, the largest numerical
comparison case, were inspected in `native-finger-clearance-one-after-render-v1`
under the SeduXtress audit directory. The curled fingers remain individually
visible and the thumb lies alongside the index finger, without an obvious
crossed finger. These isolated views do not establish weapon contact, complete
arm posture or motion quality.

## Transition coverage

`hand-fresh-transitions-one-v1` interpolates calibrated inputs, then recomputes
articulation, single-step finger clearance, thumb-tip clearance, web protection
and corrective shapes. It checks every midpoint of the 111 running intervals
at three overlay alphas, plus seven interior samples in each of the 15 retained
thumb-regression intervals. All 423 sampled poses have no additional skin
crossings, and Blender exits zero.

This differs from the earlier web-only test, which interpolated already-cleared
poses. Calibration is still supplied by saved inputs, and these are offline
outputs. The largest additional proximal rotation step is 1.234 degrees in the
sampled comparisons. That is a measurement, not visible-motion acceptance or a
proof over continuous time.

## Reproduction and next work

All evidence directories are under `work/grip-grounding-v1/`.

- `hand-native-clearance-fixtures-one-v3`: 464 input hashes and single-step,
  1-degree reference values.
- `hand-native-finger-clearance-v1`: rejected missing integer-select unit.
  The final builder uses exact integer arithmetic for that selection.
- `hand-native-finger-clearance-v2`: verifier rejected the empty default
  transforms of a fresh disabled instance. Bone preservation itself passed.
- `hand-native-finger-clearance-v3` through `v5`: completed but rejected
  12-iteration ports, including pruning-equivalence evidence.
- `hand-native-finger-clearance-one-v1` and `v2`: retained smaller-sampling
  numerical failures. Neither saved a candidate asset.
- `hand-clearance-four-iterations-v1` and `hand-clearance-one-iteration-v1`:
  offline skin checks for the reduced iteration budgets.

Use the [hand-transfer probes](../../tools/authoring-probes/hand-transfer/README.md).
Next port the thumb-tip stage,
combine the fresh-input pipeline, then verify full B2 import/cook, weapon and
sidearm handling, combined dynamics/cost, UI lifecycle and visible gameplay.
