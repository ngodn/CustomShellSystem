# Hand targets and corrective consumers

The pinned Eve and Genessa editor snapshots contain the hand rig's literal
targets, even though the cooked reader currently skips RigVM property-bag values.
This closes the source-target inventory gap. It does not establish the values
executed by the installed VM or prove good grip/contact during gameplay.

## Findings

Both source graphs have identical transform-writer and curve-writer records
after removing their package prefixes:

- 115 `FRigUnit_SetTransform` nodes target 19 distinct left-hand bones: four
  metacarpals and three joints of each of the five fingers.
- Target, space, initial-pose, propagation and weight pins have no incoming
  links, including parent `Item` links. All use local space, current pose,
  child propagation enabled and weight 1.
- Against each cooked skeleton hierarchy, targets plus descendants remain
  those 19 bones. Neither wrist nor right hand is in that set. Body/hair
  `BoneNames` arrays do not overlap it.
- 96 curve-write nodes refer to 16 distinct left-hand corrective curves. All
  16 names exist as cooked morph exports on the 12 active normal/facial Eve
  meshes and four active Genessa meshes. Eve's ten auxiliary mesh exports also
  contain them. Existence does not prove useful deltas or runtime activation.
- Commander White's decoded graph has body and hair rigs, not this adapter.

The source recipe explains why 115 nodes do not mean 115 sequential corrections
every frame. `tools/authoring-probes/hand-transfer/native_hand.py` captures incoming
locals, applies calibration, articulation, finger clearance, thumb clearance and
web/corrective stages, and contains restoration writes on rejected branches.
Disabled/rejected paths clear private curves. Branch activation and installed VM
behavior remain separate checks.

`native_clearance.py` builds posed finger segments and radial hull samples from
authored arrays. This hand self-clearance consumer does not replace a weapon
socket, Physics Asset or garment collider. These findings provide no reason to
change a weapon attachment or enlarge a general-purpose collider.

## Evidence and reproduction

Reports are under `work/three-outfit-rig-physics`:

| Report | Source snapshot and asset | Cooked hierarchy |
| --- | --- | --- |
| `eve-hand-targets1.json` | `eve-rig-editor2/before.json`, `/Game/CSS/SeduXtress/CR_Hand` | `eve-final-container1/decoded.json`, `/Game/CSS/eins0fx/Shared/Skeletons/SKEL_Base` |
| `genessa-hand-targets1.json` | `genessa-rig-editor2/before.json`, `/Game/CSS/UnholyGenessa/CR_Hand` | `genessa-face-namespace-pack2/decoded.json`, `/Game/CSS/eins0fx/UnholyGenessa/Skeletons/SKEL_Wings2` |

Each target report records input SHA256 hashes, inspected writers/pins, incoming
links, targets and potentially affected descendants. Run
`tools/diagnostics/inspect_hand_targets.py` with `--snapshot`, `--asset`,
`--readback`, `--skeleton` and a fresh `--output` using the table above.
`eve-hand-consumers1.json` and `genessa-hand-consumers1.json` record cooked
body/hair arrays and per-mesh missing-curve results.

`python3 tests/hand_targets_tests.py` passes six tests covering linked parent or
target fields, unrelated inputs, missing bones, invalid parents and descendant
propagation without siblings/ancestors. Linked settings remain unresolved.

## W3 posed-skin screen

The subsequent Blender 5.2.2 screen reconstructs linear skinning from the pinned
editor-built W3 render buffers, actual mesh-local binds, weights and exported
corrective deltas. The render capture's UTF-8 hash matches the migration source
snapshot. Both W3 variants were evaluated independently on 453 retained native
hand-output fixtures. These include synthetic attack/idle overlays and captured
calibration inputs. They are not a new replay of the installed game graph.

For each variant, 452 cases retain the same four reference triangle pairs.
One case, `synthetic-running_attack-5-alpha-1.0.json`, adds two thumb/thumb pairs.
The two W3 variants give identical contact results. A deliberately uncalibrated
input with corrective curves disabled produces 1,102 pairs, establishing a
negative control. Neutral skinning reconstruction error is 0.0000168 cm.

The flagged triangles are `[29,28,30]` and `[30,28,31]` against `[736,739,735]`
in the extracted 1,808-vertex, 3,110-triangle hand. They have no shared indices.
A double-precision plane check puts one red-triangle vertex 0.2673 mm across
the opposing triangle plane. This is a plane distance, not penetration depth.
The two-angle mesh plot shows a local thumb crease overlap. It does not establish
its visibility during actual running attacks or justify changing the whole rig.

Both normal/facial variants' complete `UG_Skin*` sections compare equal, including
weights and existing corrective deltas. Thus facial integration did not change
the screened hand skin. Source/candidate equality is recorded separately from
runtime acceptance.

Evidence under `work/three-outfit-rig-physics`:

- `genessa-hand-pose-screen4`: Eve-body repeat, exact pair fingerprints, negative
  control, flagged posed geometry and `thumb-contact.png`.
- `genessa-hand-pose-screen3`: independently evaluated Genessa-body variant.
- `hand-facial-skin-equality1.json`: all four normal/facial skin comparisons.
- `cleanup-hand-pose-screen1.json`: removed 453 superseded per-sample dumps
  (395,364 logical bytes); the first report and distinct later evidence remain.

Reproduce with the Unholy Genessa mod's `tools/screen_hand_poses.py`, using
`blender --background --factory-startup --threads 2 --python-exit-code 1
--python <script> -- --variant Eve --output <fresh-directory>`; use `Genessa`
for the other body. The script uses Blender's documented
[BVH polygon construction](https://docs.blender.org/api/current/mathutils.bvhtree.html)
and the existing edge-crossing probe. Coplanar contacts and enclosed surfaces
are excluded; render seams remain split. Public body morphs stay zero, with
only finger motion replayed and no weapon or cloth simulation.

## Current graph replay

`genessa-current-hand1` executes the migrated character-owned `CR_Hand` source
directly in UE 5.6.1 for all 453 nonsingular fixtures. Every evaluation reports
valid output, finite transforms, unchanged translations/scales and no rotations
outside the 19 intended finger bones. Compared with retained native output,
the maximum angular difference is 0.00008483 degrees and the largest corrective
curve difference is 0.000000865. The source UAsset's hash is unchanged.

The editor exited 0, reporting seven warnings. It ran in a filesystem sandbox
with only a fresh local evidence directory writable; attempted editor-config
writes were refused. `command.json`, `exit.json`, `engine.log`, `report.json`
and `retained-output-comparison.json` record the execution. The current recipe
is the Unholy Genessa mod's `tools/probe_current_hand_graph.py`.

`genessa-current-hand-skin1` passes those fresh outputs through the same W3
deformation screen using `--native-report`. It reproduces the same two extra
thumb triangles in the same single fixture; all other poses preserve the
reference contact pairs. The overlap is therefore reproduced with the current
editor graph, not attributable to stale retained hand outputs. It remains a
small synthetic-overlay contact case, not a reproduced gameplay regression.

No rig, mesh, package, installed setting or runtime DLL changed in this pass.
Before a targeted correction, check the same input through the full component
graph and confirm the actual visible surface. Do not turn this local thumb
crease case into a whole-hand or skeleton redesign. Cooked execution, runtime
scheduling, wrist/weapon grip and motion continuity remain unverified.


## Full W3 component replay

`genessa-hand-component2` passes the three representative inputs (reference,
running attack and shared idle) through compressed transient sequences and the
actual migrated `ABP_Wings2` component graph on both W3 variants. Each case has
seven frames covering disabled, enabled, disabled again, reenabled and animation
instance reinitialization. All 42 samples are finite. The 16 corrective curves
match active render morph weights exactly, become zero on disable and return to
the same enabled values after reinitialization. Both variants produce identical
component outputs. Four missing wing input bones use their actual mesh binds.
The report does not substitute reference translations from another character.

`genessa-component-hand-skin1` replays the component's enabled finger rotations
and actual render morph weights. It reproduces the same two extra thumb crease
triangle pairs only on the running-attack fixture. Reference and idle retain the
four reference seam pairs. The selected reference-input negative control gives
181 crossing pairs; the earlier 453-case screen used a different negative input
and gave 1,102. These are controls, not comparable defect counts.

The generic C++ component probe lives in CSS's `tools/authoring-probes/rig-audit`;
the character recipe is the mod's `tools/probe_hand_component.py`. Build commands,
UE logs and lifecycle validation are retained beside the reports. The first
isolated attempt (`genessa-hand-component1`) failed while constructing the
transient animation: setting frame metadata outside a controller bracket triggered
compression on incomplete data. The corrected probe uses UE's scoped controller
bracket; the second run exited 0. This was an editor test harness failure and did
not touch the game or source assets. Component cleanup now also runs on error.

This closes the current graph and morph-consumption question for these inputs.
The remaining thumb case is small and synthetic; do not change accepted hand
anatomy without a visible gameplay defect. Wrist/weapon grip, continuous motion,
right-hand behavior and gameplay scheduling are outside this screen. No installed
DLL, package or settings changed during this work.
