# Combined native V44 hand graph

The full hand sequence now runs in one saved, disabled-by-default Control Rig:
calibration, source-axis articulation, finger clearance, thumb-tip clearance,
thumb-web protection and 16 corrective curves. The 914 native checks pass;
all 453 valid original-input poses also pass actual B2 skin checking using
the combined native rotations and curves. V43 remains installed.

This verifies hand-stage composition. Complete animation transitions, weapon
contact and full-body posture, production binding, B2 import/cook, combined
hand/hair/body cost, UI lifecycle and live-game acceptance remain open.

## Inputs and comparison

`hand-combined-fixtures-v1` contains 454 original-input cases:

- 411 poses reconstructed from decoded source clips and retained overlay
  weights, with the original captured body/wrist held fixed.
- 35 captured graph inputs with the V43-compatible conversion enabled.
- Five game-reference H2 inputs, two source anchors and one deliberately
  singular calibration input.

`input-reconstruction.json` compares the reconstructed inputs, after offline
calibration, with the independently retained earlier calibration evidence.
All 454 cases pass; maximum rotation difference is 0.000030864 degrees. The
singular case preserves its input. Source and fixture hashes are recorded.
This checks fixture reconstruction, not complete game animation selection.

The native composition oracle evaluates the separately saved calibration,
articulation, finger, thumb and web/curve rigs in sequence. Each receives the
preceding native output, B2 translations and captured scales. The combined
graph receives the same original input. Both quaternion signs, disabled and
re-enabled states, fresh instances and unsupported hand scale are included.
All unrelated bones, translations, scales and a sentinel curve are checked.

The maximum comparison error is 0.000020491 degrees, on singular passthrough;
curve error is zero. The 0.001-degree rotation and 0.00001 curve gates remain
unchanged. No Python or UE4SS callback performs runtime hand math.

## Graph and failure behavior

Asset: `/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCombinedV1`.
It contains 3,742 shipped nodes. `native_hand.py` assigns separate member
names to the finger and thumb solvers, so their arrays and iteration state
cannot overwrite each other. The stage order retains the already tested
finger one-step/1-degree sampling and thumb 12-step/0.05-degree bounds.

Every evaluation snapshots fresh hand input. A rejected stage restores the
incoming hand and clears private corrective curves; disabling clears those
curves without evaluating the corrections. Restoration uses Unreal's ordinary
small-transform-write tolerance. `HandValid` resets on each evaluation.
The graph does not freeze upstream animation or change unrelated body motion.

`hand-combined-native-v1` contains the terminal exit 0, report, original
authoring sources, command and saved-asset hash. Shared Skeleton, V43 mesh and
all five reference-stage asset hashes remain unchanged.

Editor execution across changing valid poses measures 0.954 ms median and
1.905 ms p95, including the Python invocation bridge and excluding setup.
This is not shipping-game FPS, a paired comparison or total dynamics cost.
Authoring took much longer than the isolated probes: a saved stack trace
shows repeated diagnostic AST traversal while adding graph links. That is
build-time work, not the measured runtime correction cost. The original build
was allowed to finish, not restarted after a wait expired.

## Mesh and visual evidence

`hand-combined-skin-v1` applies the 19 measured hand rotations and reconstructs
source corrective shape values from the 16 native curves and baked offsets.
It performs no offline calibration, articulation, contact solve or curve
recalculation. All 453 valid poses have exactly the four neutral seam pairs
and no added triangle crossings. The one deliberately singular pose is
explicitly excluded from deformation-quality acceptance and tested instead
for passthrough. Blender exits zero; the source B2 blend hash is unchanged.

Both hand views of running-attack frame 5 at overlay alpha 1 were reviewed
in `native-combined-hand-after-render-v1`. The fingers remain individually
visible and the thumb is separated from the index in this pose. The views
do not establish weapon contact or continuous motion. Image hashes and this
bounded conclusion are retained in `visual-review.json`.

The B2 export's 16 correctives independently match evaluated Blender shape
geometry at all 36,787 body points within 0.000012014 cm. See the
[corrective source check](native-hand-calibration.md#v44-corrective-mesh-export).

## Next acceptance

`hand-combined-lifecycle-v1` adds 11 saved-asset checks using running-attack
frame 95 at alpha 1, whose private curve magnitudes sum to 8.376. Fresh
instances seeded with stale private curves clear them while disabled. Warm
disable, re-enable, singular input, unsupported scale, recovery, repeated
fresh input and replacement instances all pass with zero rotation/curve
comparison error. The process exits zero and the asset hash is unchanged.
This tests Control Rig instance behavior, not the complete CSS UI lifecycle.

Next verify sampled native transitions from original inputs. Use the saved
graph instead of rebuilding it for each
probe. Verify full B2 mesh import and cooked curves before binding the graph
to production. Select the raw-game or V43-compatible input convention from
actual evaluated pose evidence. Preserve original proportions, accepted hair
200/24 and body motion, disabled CSSX, the canonical skeleton contract and
the full modular Next-Gen architecture throughout weapon/gameplay validation.
