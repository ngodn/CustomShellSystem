# Native hand calibration and V44 morph payload, 2026-09-21

The 19-joint finger calibration now executes in native Control Rig, followed
by the 16 original corrective curves. The saved diagnostic graph passes 91
execution cases. Its exported H2 sample also passes independent Blender skin
checking with exactly the four neutral seam pairs. V43 remains installed.
The contact-clearance stage is still missing from the native graph.

## Graph contract

Asset: `/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCalibrationV1`.
It contains 924 shipped RigVM/Control Rig nodes and requires no custom game
class or per-frame Python. The test harness is Python; the evaluated mapping
and corrective math run in the graph.

The graph snapshots all 19 input locals before any bone write. It preserves
the source swing, calibrates twist using the existing per-joint parameters,
transports it into Eve's frame and writes the resulting local rotations.
Four metacarpals retain source animation without a closed-pose calibration.
Translations and scales are copied from the input, and child propagation
preserves unrelated local transforms. The corrective curves run afterward.

`Enabled` defaults false. Disabled evaluation leaves all bones untouched and
writes zero deltas to the 16 private curves. It preserves an explicitly seeded
unrelated curve. The graph is stateless, but its mapping is not idempotent:
every evaluation must receive a fresh source pose from the animation graph.
Feeding the already corrected output back as input applies calibration twice.
The repeated-evaluation test deliberately supplies the original input again.
Actual AnimBP input/curve propagation remains an integration gate.

There are two explicit diagnostic input conventions:

- Game-reference H2 fixtures use source-coordinate rotations directly.
- Captured V43 fixtures first undo their measured compatible-Skeleton mapping.

`InputIsV43Compatible` defaults false and selects the second convention for
the captured fixtures only. It is not a player-facing control or automatic
Skeleton detection. Production binding must select the validated convention;
do not infer it from matching bone names or use this graph on arbitrary mods.

Quaternion antipodes produce the same result. If a finger's swing/twist
projection has squared norm at most `1e-10`, its twist is ambiguous. The graph
passes the entire hand through and clears its private curves instead of
applying a partial calibration. `ValidInput` exposes the result for diagnostics.
The guard is checked with a 180-degree perpendicular thumb swing and its
quaternion antipode. It is not a general validator of arbitrary corrupt poses.

## Native and skin verification

The 91 cases comprise 43 inputs and their quaternion antipodes, plus five
lifecycle cases. Inputs include five engine H2 samples, 35 captured graph
poses, source open/closed anchors and the singular thumb case. Lifecycle checks
cover disable, re-enable, repeated fresh input and new-instance disabled/enabled
states. The direct native outputs match independent saved offline fixtures:

- Maximum local rotation error: 0.000031854 degrees (gate 0.001).
- Maximum corrective-curve error: 0.000000461 (gate 0.00001).
- Wrist, right hand and all other unmodified bone locals are preserved.
- Every local translation/scale and the unrelated input curve are preserved.
- The canonical Skeleton and production V43 asset hashes are unchanged.

The hierarchy is imported from the independently checked B diagnostic mesh.
The B2 attachment-preserving full mesh has not yet been imported for this
graph. B/B2 hand geometry and finger locals match; their five attachment-leaf
differences still require the final full-mesh integration check.

The measured native finger outputs for H2 sample 95 are inserted into the
original complete H2 fixture for independent B2 skin replay. All other fixture
data is retained. The 16 measured curve deltas are converted back to source
shape values by adding their recorded baked values. Blender's actual armature
deformation has exactly the four neutral seam pairs and no additional pairs.
This is one skin sample, not broad contact acceptance. The five earlier H2
and 35 captured skin checks retain their separately documented scopes.

Both isolated-hand renders of the native H2 output were reviewed. The thumb
is visibly separated from the fingers, with no obvious crossed finger mass.
Some fingers occlude others in these views. The renders contain no weapon or
full arm, so they do not establish grip contact or full-pose acceptance.
The renderer exited 0; its images and review record are retained with the
native calibration evidence.

Evidence: `work/grip-grounding-v1/hand-native-calibration-rig-v4/`, including
`report.json`, terminal exit 0, saved-asset hash, measured pose/shape outputs,
skin audit and exact-pair comparison. Fixture preparation is in
`hand-native-calibration-fixtures-v2/`. The parameter archive and scripts are
in [hand-transfer probes](../../tools/authoring-probes/hand-transfer/README.md).

## Failed harness assumptions to avoid

The earlier runs are retained, not overwritten:

- RigVM quaternion pins are atomic. `Result.X` is not a valid quaternion
  subpin. Quaternion dot products with a pure-axis quaternion and identity
  supply the twist projection and scalar component without decomposing pins.
- A curve value added to the authoring hierarchy is not a seeded incoming
  value on a new rig instance. The preservation check now explicitly sets and
  reads the instance's unrelated curve before executing the graph.
- The closed source-Eve control has a static metacarpal, but the calibrated
  game pose deliberately retains pinky-metacarpal motion. Comparing that bone
  with the static control falsely reported a 5.79-degree error. The endpoint
  fixture now uses the independently saved full-graph result for metacarpals,
  while retaining original-Eve closed-control expectations for the 15 finger
  joints. No animation was frozen or tolerance relaxed to make it pass.

These are `hand-native-calibration-rig-v1` through `v3` and
`hand-native-calibration-fixtures-v1`. The final raw/compatible cases and
numerical tolerances remain unchanged.

## V44 corrective mesh export

`work/grip-grounding-v1/arm-rest-correctives-export-v1/candidate.mesh.json`
exports B2 with all 22 morph targets, six public body morphs plus 16 private
finger correctives. It passes the ordinary canonical bind policy. It retains
379 bones, 133,066 points, 193,261 faces and 30 materials.

Every interchange field except the isolated mesh package name and added
morphs matches the previous six-morph B2 export exactly. This includes points,
topology, UVs, normals, colors, weights, materials and all bone records. All
six original morph records are identical. The 16 new names and baked values
match the native driver exactly, and their deltas are nonempty and finite.
The output hash matches the exporter audit.

An independent source-delta check now evaluates each B2 hand corrective at
zero and one through Blender's dependency graph, with the other saved fit
keys retained. It compares all 16 shapes at all 36,787 exported body points
without calling the exporter's delta routine. Neutral point mapping is exact;
maximum delta error is 0.000012014 cm against a 0.0002 cm gate. No coincident
point needed disambiguation. Source and interchange hashes remain unchanged.
`arm-rest-corrective-source-deltas-v1` contains the report and terminal exit 0;
`verify_corrective_source_deltas.py` reproduces the check.

Full Unreal import, cooking and GPU morph execution remain unverified. The
earlier V43 corrective cook is not proof that the reposed B2 cook is correct.

## Remaining work

Follow-up: the [B2 thumb-web guard](thumb-web-contact.md) passes 464 saved
poses and 105 affected transition samples offline. The native graph described
here is unchanged and does not yet include this guard or the earlier
finger/tip-clearance stages.

Fit the clearance constraints to B2, resolve the existing thumb/palm and
same-thumb failures, and add that stage between calibration and corrective
curves. Then verify full heavy-weapon and sidearm poses, preserved tolerated
Axe & Dagger/Axatana behavior, motion continuity and cost. Complete the B2
asset import/cook and audited game-reference binding before deployment.

The full Next-Gen scope remains active: combine with accepted hair/body
dynamics and independent native controls, verify UI/profile/reset/lifecycle
behavior, performance and live gameplay, and finish packaging and documentation.
Original proportions, hair 200/24, accepted body settings and disabled CSSX
remain unchanged.
