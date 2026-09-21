# V44B2 reference binding and weapon review

Historical preparation checkpoint. V44 has since been installed for a trial;
see [current installation and live acceptance](v44-live-trial.md). References
to installed V43 below describe the state when these measurements were taken.

V44B2 now has an isolated, saved Skeleton binding with corrected animation
reference rotations, preserved CSS metadata and nine virtual bones. A fresh
process verifies its mesh, material bindings, post-process and accepted hair
200/24 defaults. The hand stage defaults enabled with raw-game input convention.
V43 remains installed. This is not a shared Skeleton migration or a release.

## Why the reference matters

`b2-animation-binding-v1` evaluates the full B2 mesh through the retained H2
source animation, compatible-Skeleton retargeting, two game CopyBone nodes
and left TwoBoneIK. Forty cases cover two reference policies, raw/compressed
source, five time samples and IK disabled/enabled. Both policies reproduce
the corresponding earlier arm prototype results, excluding the five restored
attachment leaves and derived virtual transforms. Maximum compared rotation
error is 0.000007637 degrees; translation error is zero.

With corrected game-reference rotations, the 19 hand inputs match the five
retained raw-convention calibration inputs within 0.001 degrees. The old CSS
reference differs by 33.74 to 38.26 degrees at the worst finger in each sample.
This is an actual reference-dependent difference, not matching bone names as
proof of compatibility. Select `CSSHandInputIsV43Compatible=false` for this
candidate. Captured V43 fixtures still require their separately tested true
convention. Do not apply the selection indiscriminately to other mods.

The saved metadata-preserving Skeleton then passes the same 40-case engine
comparison in `b2-metadata-animation-binding-v1`, including the explicit
old-reference negative control. Neither comparison runs the entire player
graph, all overlays, every weapon or live gameplay.

## Isolated metadata and defaults

The editor helper duplicates the shared CSS Skeleton into a new diagnostic
package and changes only raw reference rotations. It requires the unchanged
379-name/parent layout, preserves reference translations/scales and translation
modes, and cannot target the production package. The engine's
`FReferenceSkeletonModifier` rebuilds derived virtual reference data.
Persistent reflected Skeleton/owned-object metadata is compared independently
before saving and after fresh loading. Raw reference records are checked
separately. Existing compatible-game path and nine virtual definitions survive.

The saved packages are:

- `/Game/CSSAuthoring/DiagnosticReferences/SKEL_B2GameReferenceMetadata_V2`
- `/Game/CSSAuthoring/DiagnosticReferences/SK_B2GameReferenceMetadata_V2`
- `/Game/CSS/TransientProbes/ABP_CSS_ControlRigProbeHandB2ReferenceV2`

The mesh bind remains the independently verified 379-bone B2 bind. Its 30
material names, imported names, interfaces and overlay bindings match V43;
all 22 morph targets remain present. Its post-process targets the new Skeleton.
The editor helper sets both Blueprint variable declarations and compiled CDO
values for stiffness 200, damping 24, hand enabled and raw convention.
Fresh loading verifies those saved values without resetting them first.

`b2-reference-fresh-v2` passes 35 actual component checks from five newly
retargeted H2 poses and seven enable/reinitialize frames each. Outputs match
the standalone native hand oracle seeded from actual upstream poses; unrelated
bones preserve body/hair results. Maximum rotation error is 0.000002958 degrees,
translation error zero, active morph-weight error 2.981e-8. Private morph flags
survive the new mesh/Skeleton binding and disabled frames clear their weights.
Protected source, V43, shared Skeleton and candidate file hashes stay unchanged.

## Weapon render

`b2-reference-weapon-v1` replays measured sample-1 component output with
Martyr's Blade, source fit shapes and all 16 measured corrective weights
converted to absolute source values. The weapon renderer now accepts these
explicit values and a guarded raw-379-bone component snapshot; alternate
meshes still require independently verified bind data. It never invents nine
virtual transforms. Both views were inspected against the earlier fitted
uncorrected view. The left fingers are separated instead of the previous
folded/crossed arrangement. The supporting hand remains relaxed and open.
Do not force a fist just to make this partial idle fixture look like an attack.

This is an offline solid-material comparison. It does not accept weapon/skin
penetration, right-hand grip, all arm postures, transitions, garments or live
appearance. The source blend hash is unchanged. Maximum replay position
error is 0.000062555 cm and measured angle error is zero.

## Retained failures and limitations

- `b2-reference-metadata-v1` exits 255 because Blueprint `new_variables` is
  unavailable through this Python interface. No Skeleton or mesh was saved;
  the earlier diagnostic graph creation had saved its own package. The C++
  helper now sets declaration defaults and compiled defaults explicitly.
- `b2-reference-metadata-v2` saves the three candidate assets, then exits 255
  when report generation dereferences the absent Physics Asset. The fresh
  process verifies the saved artifacts instead of treating that failed run
  as success. The preparer now records null safely.
- `b2-reference-fresh-v1` exits 255 at whole material-struct array equality.
  The actual binding contract compares names, interfaces and overlays in v2;
  UV density belongs to each mesh's geometry. Python's UV struct string only
  exposes an address, so it does not diagnose the reason for the whole-struct
  difference or verify density values. No mesh data was changed to pass it.
- The V43 editor mesh has **no Physics Asset**. The candidate consequently has
  none. This confirms a gap already observed in V42; it is not a collision
  repair. Runtime overrides, damage filtering and parry still need checking.
  The earlier live collision observation could not query `GetPhysicsAsset`.
- The shared CSS Skeleton has no socket objects, blend profiles or slot groups
  in the inspected metadata. Preserving that source does not import the
  game's additional metadata. Audit what gameplay needs before promotion.
- This exact binding/default revision has not been cooked or installed. Earlier
  B2 cooks do not prove these new references/defaults survived cooking.

## Reproduction and continuation

Editor source changes are in
[b2-reference-metadata.patch](../../tools/authoring-patches/b2-reference-metadata.patch),
applied after the hand integration/motion patches. The separate
[renderer patch](../../tools/authoring-patches/weapon-render-corrective-shapes.patch)
is applied at the SeduXtress mod root. Both reverse-check successfully. The
editor build uses the existing C++20 / UE 5.6.1 project; probes use its Python.

`probe_b2_animation_binding.py` accepts `CSS_B2_BINDING_DIR` and optionally
`CSS_B2_REFERENCE_SKELETON`. The output is a fresh direct child of
`work/grip-grounding-v1`. Preparation and fresh verification scripts retain
fixed single-use package/evidence names; do not overwrite the evidence or
rerun preparation over saved packages. Command manifests and terminal exit
records are beside the reports. The metadata helpers do not save automatically.

The pinned engine source is authoritative for the modifier and metadata APIs.
Epic's [Skeleton documentation](https://dev.epicgames.com/documentation/unreal-engine/skeletons-in-unreal-engine)
provides general compatibility context, not evidence that this particular
character is compatible or that current-version fixes exist in UE 5.6.1.

Next verify cooked references/defaults and the required gameplay metadata,
resolve the collision/Physics Asset gap, then test the complete weapon/sidearm
and movement matrix in-game with screenshots/recording and measured cost.
Preserve original proportions, accepted body/hair motion, six public morphs,
modularity, CSSX disabled and the full native/UI/profile/lifecycle/distribution
Next-Gen scope. V43 remains the fallback until the replacement passes.

## Cooked binding checkpoint

`b2-reference-cook-v1` cooks the exact saved metadata/default candidate plus
the unchanged complete hand rig and exits zero. Independent packing,
verification and decoding in `b2-reference-cooked-readback-v1` all exit zero.
`verify_b2_reference_cook.py` verifies the new mesh/Skeleton/AnimBP references,
all 379 raw Skeleton reference records at numeric float32 precision, nine
virtual definitions, compatible-game path and unchanged translation modes.
The cooked hand stage is enabled, uses raw input convention, and retains
hair defaults 200/24. Other inherited body/hair defaults and source-member
mappings match V43. All 22 morph flags survive.

Every decoded ActorX chunk except material names is byte-identical to the
previous B2 cook: points, faces, normals, colors, weights, UVs, reference bones
and all morph payloads. Assigned material names now replace placeholders;
non-name material records are unchanged, and all 30 cooked material bindings
match V43. The complete hand-rig JSON is unchanged. The Physics Asset remains
null, so this package is still an isolated diagnostic, not the final repair.

The first reference verifier compared float32 bytes and stopped on signed
zero: the editor JSON writes `0`, while the cooked decoder retains `-0.0`
on 25 components. Numeric float32 values match exactly. The verifier now uses
exact numeric float32 equality, with no tolerance increase; the initial
failure and component list are retained. No asset was recooked or changed
to pass this check. Actual cooked execution, collision fitting and full live
acceptance remain open.
