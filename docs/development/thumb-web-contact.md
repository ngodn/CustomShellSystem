# V44B2 thumb-web contact investigation, 2026-09-21

V43 remains installed. This work changes diagnostic poses and scripts only;
it saves no blend, production asset or game state. Original proportions,
accepted hair/body dynamics and disabled CSSX remain preserved. The complete
Next-Gen runtime, modular controls, UI, packaging and live acceptance remain
the objective.

## Reproduced defect

Five of the six retained synthetic thumb failures reproduce on actual B2 skin.
The sixth, running frame 3 at full overlay, now has only the four neutral seam
pairs. B2 is not geometrically identical to the old hand in every posed vertex,
so use the current skin results rather than copying old counts.

| Pose | Additional pairs on B2 |
| --- | ---: |
| Heavy 106, alpha 1 | 6 |
| Running 3, alpha 1 | 0 |
| Running 4, alpha 1 | 25 |
| Running 5, alpha 0.5 | 2 |
| Running 5, alpha 1 | 29 |
| Running 6, alpha 1 | 21 |

The recorded full-strength overlay during attacks is a synthetic stress input,
not evidence that the game plays these exact poses. Actual captured poses
remain a separate validation group. Replay substitutes B2 bind translations
and retains the saved body rotations; it is not a full V44 animation graph.

`arm-rest-thumb-stress-v1` exits 2 as expected. Local corrective formulas match
the independent saved full-world values within 0.000001239. Triangle weights
locate the defects in mixed thumb-base/index-metacarpal web tissue and near the
next thumb joint. A `thumb/thumb` label does not necessarily mean the distal
thumb is folding onto itself. Both baseline isolated-hand renders were
reviewed; these small web crossings are not clearly visible in those views,
so the skin audit provides the more specific evidence.

## Rejected isolated changes

The first ablation changes one factor at a time, then checks all five failing
poses. Halving base twist clears two but leaves three failures. Straightening
the outer two thumb joints, combining that with half twist, or disabling the
two base corrective shapes also fails as a general solution. Removing all
base twist still leaves two failures and changes orientation by up to 47.7
degrees. Halving base flex, zeroing source Z, and limiting negative twist to
ten degrees also fail. Preserve these results; do not deploy any of them.

Evidence is in `arm-rest-thumb-ablation-v1` and `v2`, with original script
snapshots for v1. These runs exit 0 because the experimental measurements
completed, not because every variant repaired the hand.

## Experimental web boundary

A one-axis sweep increases the original source-space thumb-base Z while
preserving its X/Y and every other joint. Across the six poses, the final
passing region in the tested 0..22-degree offsets begins at 10, 4, 12, 2, 16
and 16 degrees respectively. Running frame 3 is already clear at zero but
fails at two degrees; the contact set is not monotonic over the entire sweep.
Do not binary-search from an assumption that any partial correction is safe.

The experimental `thumb-web-guard-v1.json` is a pose-dependent boundary fitted
from these six cases. In the original rig's YZX Euler coordinates, in degrees:

```text
lower_z = min(10, 0.5 * x - 0.9 * y - 35)
correction = clamp(lower_z - z, 0, 24)
output_z = z + correction
```

This is an empirical candidate for this mesh, not a universal anatomical
limit. It has no animation-name/frame lookup and does not force a fixed fist.
It changes only `thumb_01_l`, then recomputes the 16 source corrective values.
It supplements the earlier finger/tip-clearance output. That earlier clearance
stage still requires a native implementation fitted to B2. It must not be
silently removed when this web correction is integrated.

The broad paired check is `verify_thumb_web_guard.py`: 411 synthetic poses,
35 captured poses, 13 original controls and five H2 samples. It retains the
exact neutral-pair criterion and rejects changed original controls or a
saturated correction. V1 passes all 464 saved poses, exit 0. It changes only
13 synthetic poses and none of the 35 captured poses, 13 controls or five H2
samples. Its maximum correction is 21.331 degrees. Both corrected isolated-hand
renders were reviewed: the thumb opens away from the index web, while the
other finger silhouettes remain consistent. They contain no weapon.

V1 is nevertheless rejected for integration. Seven intermediate samples per
affected running-attack interval expose one two-pair web crossing at frame
5.625, full overlay. The saved frames miss it. The transition harness exits 2
and retains that pose and its shapes/contacts. The maximum added rotation step
in its measured 1/8-frame samples is 0.846 degrees, which is a measurement,
not motion acceptance. V2 changes only the source-Z ceiling from 10 to 12
degrees; it must pass the same transition and broad pose checks before use.

V2 now passes both checks, with terminal exit 0:

- All 464 saved poses have only the exact four neutral seam pairs. The five
  failing B2 controls remain reproducible without the guard.
- All 105 tested intermediate poses pass. These cover seven subdivisions in
  each of 15 affected running-frame intervals, at the saved overlay weights.
- Only 13 synthetic saved poses change. All captured poses, original controls
  and H2 samples are unchanged, including every non-thumb local transform,
  translation and scale. No correction reaches the 24-degree cap.
- Maximum saved-pose correction is 23.331 degrees. The largest measured added
  1/8-frame rotation step is 0.891 degrees. Weapon contact and motion quality
  still require validation; passing skin contact does not accept either.

Evidence: `arm-rest-thumb-web-v2` and `arm-rest-thumb-web-transitions-v2`.
V1 and its failing frame remain retained. The V2 boundary is still an offline
candidate, with no native graph integration or game deployment.
Both V2 isolated-hand views were reviewed after the final run. They show the
thumb farther from the index web without an obvious web fold; other finger
silhouettes remain consistent. This is a single pose without a weapon, not a
motion or grip review. The render process exits 0 and its review is retained
in `arm-rest-thumb-web-v2/visual-review.json`.

## Faster actual-skin replay

`SkinFixture` retains actual Blender armature deformation and the existing
non-adjacent triangle-crossing audit. Its optional batched pose assignment uses
Blender's documented [Bone.convert_local_to_pose](https://docs.blender.org/api/current/bpy.types.Bone.html?highlight=matrix)
with supplied parent matrices, instead of updating the dependency graph after
each bone. The sequential method remains available for independent checks.

Eight paired poses, original open/closed controls and all six stress cases,
preserve the exact intersection pairs. All 36,789 body vertices agree within
0.000181 cm, against a predeclared 0.001 cm gate. Measured evaluation falls from
4.1..4.5 seconds to 0.20..0.23 seconds in this diagnostic. These are offline
authoring timings, not runtime cost or FPS claims. Evidence:
`arm-rest-batch-pose-v1`, terminal exit 0.

## Evidence and remaining gates

All evidence directories above are under `work/grip-grounding-v1/`.
`arm-rest-thumb-contact-locations-v1` retains triangle weights and joint
distances. `arm-rest-thumb-base-sweep-v1` retains every tested offset, first
passing candidate and shape values. Scripts live in
[hand-transfer probes](../../tools/authoring-probes/hand-transfer/README.md).

Even a passing finite pose set does not establish continuous/subframe contact,
weapon grip, native execution, cooked morph correctness or live acceptance.
Complete those gates before installing V44. The user should review the actual
in-game result after deployment, while V43 remains the fallback.
