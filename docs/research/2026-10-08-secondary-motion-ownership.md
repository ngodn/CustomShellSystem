# Cooked secondary-motion ownership

This inventory describes the four coordinated trial containers. It does not
accept visible motion, contact quality or frame cost. The active facial/visibility
runtime correction still needs the user's in-game check.

## Inputs and repeatable check

`work/three-outfit-rig-physics/secondary-bindings4/report.json` pins hashes for the
three final decoded outfit containers, shared catalog and nine-variant manifests.
The report contains 18 normal/facial mesh records, ten animation graphs, five
playback adapters and eight Control Rig assets.

```sh
python3 tests/secondary_bindings_tests.py
python3 tools/diagnostics/audit_secondary_bindings.py \
  --catalog work/three-outfit-rig-physics/catalog-namespace3/shared-inline.json \
  --outfits work/three-outfit-rig-physics/catalog-namespace3/migrated-outfits.json \
  --readback work/three-outfit-rig-physics/eve-final-container1/decoded.json \
  --readback work/three-outfit-rig-physics/commander-final-container1/decoded.json \
  --readback work/three-outfit-rig-physics/genessa-face-namespace-pack2/decoded.json \
  --output /tmp/css-secondary-bindings.json
```

The output path must not already exist. Eight negative/control tests pass.
All five adapters retain their source graph's CSS control types/defaults, rig
classes, input binding identities, pose-transfer flags and explicit bone
transform targets/spaces. Generated node GUIDs may differ. Swapping two
same-type inputs is rejected. Repeated endpoint entries inside a single hair
node are distinct from two nodes claiming the same named bone.

## Region owners

All paths below are relative to `/Game/CSS/eins0fx/`.

| Outfits and region | Cooked owner | Driven region and contact inputs | Reset/control evidence | Remaining proof |
| --- | --- | --- | --- | --- |
| All six Eve outfits, body | `EveSB/CR_Body` | Seven helpers: chest pair, glute pair, thigh-twist pair, belly; seven contact-frame entries | `ResetEpoch`, `UpdateSerial`; regional frequency/damping/amount/enabled plus morph-dependent offsets, inertia and contact axes | Continuous motion, frame-space correctness, contact fit, tick/reset behavior |
| Both Genessa variants, body | `UnholyGenessa/CR_Body` | Same seven helper names; character-owned asset and shape inputs | Same exposed inputs as Eve; source and performance adapter agree | Same motion/contact checks on both body variants and ornaments |
| Commander White C4/E9, body | `CommanderWhite/Rig/CR_BodyB3` | Seven helpers; smaller authored angular/travel limits; all seven `ContactActive` defaults false | Regional settings and reset/update fields retained in `ABP_BodyHairB4` and its adapter | Determine effective runtime contact settings before changing colliders |
| Eve, hair | `EveSB/CR_Hair` | 36 point entries, 29 distinct named hair bones; 14 contact entries | `ResetEpoch`, `UpdateSerial`, gravity/stiffness/damping/enabled; 1/120-second authored step | Root anchoring, actual weighted strands, update order, clearance and cost |
| Genessa, hair/hood-associated chains | `UnholyGenessa/CR_Hair` | Same named-point inventory as Eve; this alone does not prove which visible parts are weighted to it | Same exposed reset and hair controls | Map chain weights to actual visible geometry before altering this solver |
| Commander White, hair | `CommanderWhite/Rig/CR_HairH4` | 36 point entries, 29 distinct named bones; character-specific contact radii/margins | Reset/update fields and hair controls; 1/120-second authored step | Head/shoulder clearance, reset behavior and measured cost |
| Eve and Genessa, hand correction | Their own `CR_Hand` | Compiled hand/articulation/clearance solver; no `BoneNames` array | `Enabled`, `InputIsV43Compatible` bound on both normal and playback graphs | Literal bone targets and parent propagation are not decoded by this check |
| Eve Bikini/Knitwear/Alice, feet | `ABP_BikiniFeet2` or `ABP_KnitFeet1` | Left/right foot additive rotations in bone space | Targets/spaces retained by corresponding playback adapters | Posed shoe clearance, contact phase and native IK interaction |
| Genessa, wings | `ABP_Wings2` and `ABP_Genessa` | Four `ug_wing_*` bones; additive bone-space rotations | All four transform targets and spaces retained | Gait-driven values, hinge motion, nearby cloth and transition timing |
| Cloth | Per-mesh `ClothingAssetCommon` objects below | Independent simulation meshes and per-cloth Physics Assets | Cooked ownership and vertex counts below | Pin weights, morph/simulation correspondence, active colliders and visible contact |

No overlapping **named `BoneNames`** writers were found in any of the ten graphs.
This is not proof that all writers are disjoint: hand-rig literal targets,
parent propagation, graph execution order and cloth render transfer remain to
be examined. The cooked reader exposes RigVM literal property descriptions and
serialized size, but not the literal values needed to resolve every hand target.

Eve/Genessa list the two thigh contact frames in the opposite left/right order
to Commander White. That is an observation, not a diagnosed defect. Compare
actual reference transforms and the model's local contact geometry before
changing either mapping.

All three hair assets declare 64 contact iterations. This is a solver default,
not a measured per-frame execution count or permission to increase it. The
effective loops and their cost require runtime or VM evidence.

## Cloth inventory on facial replacements

Counts below are LOD0 physical simulation vertices, not render vertices or
performance measurements. Each normal mesh and its facial replacement is recorded
separately in the JSON report. There are nine variants and 18 distinct mesh paths.

| Variant | Cloth assets | LOD0 simulation vertices | Cloth collision asset |
| --- | ---: | ---: | --- |
| Eve Black Pearl | 0 | 0 | None |
| Eve Prototype | 1 | 18 | `EveSB/.../PA_PTailRear` |
| Eve Skin Suit | 0 | 0 | None |
| Eve Vacation Bikini | 0 | 0 | None |
| Eve Casual Knitwear | 1 | 1,679 | `EveSB/.../PA_KCloth4` |
| Eve Midsummer Alice | 1 | 624 | `EveSB/.../PA_ACloth3` |
| Genessa, Eve body | 6 | 3,420 | `UnholyGenessa/PA_Body`, `PA_SkirtFit` |
| Genessa, Genessa body | 6 | 3,420 | `UnholyGenessa/PA_Body`, `PA_SkirtFit` |
| Commander White E9 | 1 | 4,893 | `CommanderWhite/.../PA_ClothC13` |

The JSON records exact paths, cloth names, used bones, per-LOD vertex/index counts
and render section counts. A component Physics Asset is not automatically the
collision source of every attached cloth asset. Preserve this distinction when
testing a contact repair.

## Next checks

1. Accept the installed runtime correction through facial idle, visibility and
   cross-outfit switches. Preserve native action interruption and doubles.
2. Resolve hand-rig literal targets and pose graph order from the compiled graph
   or a pinned source-to-cooked comparison.
3. Record simulation space, anchor/weight coverage and reset behavior for the
   region owners above. Capture matched enabled/disabled motion before proposing
   a geometry, weight or collision change.
4. Keep the full regression and performance matrix open. This structural audit
   does not replace those tests.
