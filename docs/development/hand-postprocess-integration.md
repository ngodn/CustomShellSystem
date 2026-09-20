# Hand, hair and body post-process integration

An isolated copy of the accepted body/hair AnimBP now appends the complete
hand rig. Actual skeletal-component evaluation passes 42 checks across six
static poses, followed by a seven-frame fresh-import regression. The tests
found and repaired missing morph-driver metadata in the mesh importer.
V43 remains installed. Full animation, weapon contact, cooked execution and
live performance acceptance remain open.

## Actual failure and fix

The original B2 mesh has all 22 morph payloads, and its hand graph produces
the expected curves. That was insufficient: the first component test found
five nonzero H2 corrective curves with zero active render morph weight.
The largest discrepancy was 0.198182076. Both the mesh and Skeleton lacked
morph flags for all 16 private curves. The failed probe exits 255 and remains
at `work/grip-grounding-v1/hand-postprocess-integration-v1`.

UE 5.6.1 `FAnimInstanceProxy::UpdateCurvesToEvaluationContext` intersects
evaluated curves with the required-bone curve flags before classifying them
as morph targets. `FBoneContainer::CacheRequiredAnimCurves` collects the
Skeleton and mesh curve metadata. A curve can therefore appear in
`GetCurveValue` while never driving an active morph. This explains why
checking payloads and rig curve values alone missed the failure.

A controlled copy of the same mesh adds `UAnimCurveMetaData` with morph
flags, leaving geometry, names, the bind and Skeleton unchanged. The
negative control reproduces the original missing weights in the same
process; the copied mesh then passes all 42 component checks. The largest
curve and active-weight difference from the standalone native oracle is
5.97e-8. This supports the metadata cause, not a claim that every reported
game hand defect is solved.

`CSSImportMeshCommandlet` now records morph flags on the mesh for every
successfully imported morph. It uses mesh asset user data so outfit-specific
curves do not mutate the shared Skeleton. The fresh-import regression
changes only package names in the same source interchange, reopens the new
assets in a separate editor process, and measures actual component weights.
All seven frames pass, including 20 nonzero-weight checks with zero error.
The bind is unchanged, and disabled frames have zero private morph weights.

## Graph and component checks

The graph order is linked input, accepted BodyV42, accepted HairV37, complete
hand rig, output. It copies the existing body/hair blueprint instead of
reconstructing its controls. The hand node transfers all fresh input bones
and curves in local space, resets its input pose and transfers exactly the
19 hand output bones. Hair and body output filters stay disjoint from it.
`CSSHandEnabled` and `CSSHandInputIsV43Compatible` default false.

The component fixture builds a compressed animation from each input pose,
using B2 translations. It compares the candidate with a second component
running the same body/hair graph with hand correction disabled. The direct
native hand oracle receives that second component's actual evaluated pose.
This catches input-transfer and output-filter errors rather than assuming
that the post-process receives the original source quaternions unchanged.

The six inputs include the source reference, H2 idle, a captured V43 overlay
pose, a retained thumb regression interior, the largest sampled correction
step and a high-curve running-attack pose. Each exercises initially disabled,
enabled, repeated fresh input, disabled, re-enabled, disabled and replacement
instances. Both raw-game and V43-compatible conventions are included.

Every output bone is compared with the native oracle or unaffected baseline.
Maximum rotation error is 0.000010246 degrees, translation error is zero and
scale error is 2.12e-8. Component curves, rig curves and the component's
`ActiveMorphTargets`/`MorphTargetWeights` are checked independently. Hair is
set to accepted stiffness 200 and damping 24 on both components; body controls
are inherited. These static fixtures establish coexistence and transfer,
not complete dynamic motion, total cost or gameplay acceptance.

## Reproduction and evidence

- `hand-postprocess-integration-v1`: original failing component report,
  source backups, editor builds, saved graph and command manifests.
- `hand-postprocess-integration-v2`: metadata-only copy, negative control,
  six measured component reports, 42-check verdict and terminal exit zero.
- `hand-morph-import-regression-v1`: renamed source with all other data
  unchanged, successful import, fresh component regression and protected
  asset hashes.

The editor-only changes are retained in
[hand-postprocess-integration.patch](../../tools/authoring-patches/hand-postprocess-integration.patch).
It is already applied to the workspace authoring project, following the
earlier authoring patches. Reverse-check the patch before applying it again.
The project pins C++20 and UE 5.6.1. Authored graphs contain shipped engine
nodes and do not depend on this editor module at game runtime.

`probe_hand_postprocess.py` uses `CSS_HAND_POSTPROCESS_DIR` for a fresh direct
child of the grip evidence folder. `CSS_HAND_METADATA=1` enables the separate
metadata-copy experiment and retained negative control. That experiment is
single-use and preserves its named assets. `verify_imported_morph_metadata.py`
checks the separately imported production-path fix without manual flagging.

The isolated mesh still has placeholder materials and lacks production
physics, virtual-bone and shared-Skeleton integration. Before deployment,
finish those bindings, select the actual incoming pose convention from game
evidence, and verify full weapon/sidearm poses, accepted dynamics, controls,
menu/game lifecycle and performance. Preserve original proportions, CSSX's
disabled state and the complete modular Next-Gen scope.


## Cooked integration checkpoint

`hand-postprocess-cook-v1` cooks the fresh imported mesh, its diagnostic
Skeleton, the three-rig AnimBP and the hand rig, and exits zero. Independent
pack/verify/decode at `hand-postprocess-cooked-readback-v2` also exits zero.
The first staging attempt stopped before packaging because the old helper
accepted only `/Game/CSSAuthoring/`; it now also accepts the cooker's existing
`/Game/CSS/` namespace and uses a fresh evidence directory.

The cooked mesh retains all 22 morph flags with no material flags or linked
bone restrictions. Its decoded ActorX file is byte-for-byte identical to the
previous verified B2 cook, including all geometry, UV, skin and morph records;
its decoded reference transforms also match exactly. The serialized pose path
is input, body, hair, hand, output, with disjoint 7/29/19-bone filters and both
hand controls connected. All inherited body/hair control values, node settings
and member-to-pin mappings match the previous V43 decoded blueprint. Generated
node-property GUIDs change on duplication; their actual mappings are checked
instead of accepting or rejecting GUID text alone.

The copied asset retains its inherited hair defaults of 150/18. The component
fixtures explicitly apply the user's accepted 200/24, and the installed
settings are untouched. Set the production candidate defaults to 200/24 when
promoting it and verify profile application; this diagnostic clone is not
ready for installation. Its hand stage remains disabled by default.

`verify_hand_postprocess_cook.py` records these data checks. They do not prove
cooked VM execution or actual gameplay. Full moving-animation integration,
public morph isolation, production material/physics/Skeleton binding, visible
weapon checks and combined performance remain required.
