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

No rig, mesh, package, installed setting or runtime DLL changed in this pass.
The thumb case needs matched current-graph reproduction before a repair. Cooked
literal identity, runtime scheduling, wrist/weapon grip and motion continuity
remain unverified.
