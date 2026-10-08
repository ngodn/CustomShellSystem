# UE 5.6.1 source animation audit

This C++20 editor module exposes commandlet-only snapshot helpers to Python.
It is authoring tooling, not a CSS runtime module.

`SnapshotDeformer` reads Deformer Graph nodes and shader text, including referenced
function graphs. `CloneClothMorphStudy` duplicates only UE 5.6.1's
`DG_LinearBlendSkin_Morph_Cloth_RecomputeNormals` into a fresh package under
`/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphStudy...`. It expands the copied
functions, retains the skinned morph offset during cloth blending, and leaves
the normal/tangent reconstruction kernel unchanged. It does not save or bind
the result. Invoke it through Python in an isolated commandlet, snapshot the
result, and explicitly save that private asset with `EditorAssetLibrary`.
Engine assets and installed packages must remain read-only.
`author_cloth_morph_study.py` is the commandlet driver. Set `CSS_PANEL_STAGE` to
the private writable output directory and `CSS_DEFORMER_CONTENT` to the engine's
`Plugins/Animation/DeformerGraph/Content` directory. Start with an empty private
Content tree and allow writes only inside that project/output, as in the
recorded bubblewrap command. The fixed study destination must not already exist.

The helper checks the source identity, output namespace, expected kernel text,
writeback and graph compilation. Graph compilation alone is not GPU shader
validation. UE 5.6.1's `ExpandCollapsedNodes` returns an empty array even after
success, so the helper checks that the reference node was actually removed.

`check_cloth_morph_study.py` compares baseline/candidate shader snapshots and
tests position math against captured `UG_Skirt` render morphs and cloth weights.
Its rotations and cloth displacements are synthetic. It checks loss of the
direct morph contribution, not penetration, physical contact or shading.
The private Windows cook and package evidence are in
`work/three-outfit-rig-physics/genessa-deformer-{candidate,cook,pack}1`.
This prototype still needs GPU execution, visible fit, lifecycle and cost checks.

`StepClothDeformerStudy` drives an isolated preview component at 60 Hz with its
post-process graph disabled. It accepts only current Genessa facial meshes in
a preview world, disables automatic ticks and waits for each cloth step. It
reports the deformer instance and finite cloth particle count. This isolates
reference-pose cloth/morph behavior; it does not reproduce game animation or
body-spring behavior.

`render_cloth_morph_study.py` compares legacy skinning, the stock deformer and
the trial at glute weights 0 and 1 on both variants, using fixed rear/side views
and flat diagnostic materials. It requires the private CSSRigAudit and existing
CSSAuthoring modules, read-only current Genessa source assets, and the study
graph. Its `CSS_PANEL_STAGE` directory must contain `sources.json` and an empty
`captures` directory. The recorded `genessa-deformer-render1/command.json` uses
offscreen Vulkan and permits writes only inside that trial. The initial render
run is pending, so this script is not yet validated end to end.

The recompute-normals graph requires cooked half-edge mesh data or retained CPU
index/position buffers. UE 5.6.1 `OptimusDataInterfaceHalfEdge.cpp` validates
this and recommends registering the graph in the mesh's `TargetMeshDeformers`.
Editor on-demand buffer generation does not prove packaged compatibility.
If accepted visually, recook private Genessa meshes with that dependency and
verify their cooked buffers before installing this approach.

`SnapshotAnimation` reads named source tracks through `IAnimationDataModel`,
hashing explicit transform components rather than struct padding. It records
frame/key counts, sample rate, model GUID, curve/attribute counts and retarget
reference transforms. `SnapshotSkeleton` records raw bones, parent indices,
retarget modes, reference transforms, virtual definitions and sockets.

`SnapshotAssetProperties` inspects blueprints, bone compression settings,
materials, textures, skeletal meshes and physics assets without saving them.
It captures non-transient reflected fields on
package-owned objects, including codec subobjects and class defaults, plus
graph pin types, defaults and connections. Path normalization is explicit in
the migration comparison. Cooked readback remains necessary because reflection
does not capture every engine serializer's private data.

The comparison excludes only the `RigVMGraph.LastStructureHash` cache in
addition to transient/deprecated fields and empty generated animation mutable
structs. UE 5.6.1's `URigVMGraph::PreSave` recomputes that hash from nodes and
pin type-registry hashes. Nodes, pins, types, links and defaults remain part
of the comparison. The rejected first Genessa trial records the cache-only
difference in `genessa-rig-editor1/snapshot-mismatch.json`.

For cross-process rig comparison, the Python driver also omits empty
`ModularRigController` helpers and normalizes the generated type name at the
start of RigVM's two property-bag memory values. It retains every stored value,
node and pin. UE's `FModularRigModel::GetController` builds the helper name from
its outer path; `PropertyBag.cpp` generates the transient bag name from descriptor
hashes. The original snapshots remain in the reports. These are source checks;
independent cooked rig readback is still required.

The implementation uses the local 5.6.1 `UObjectHash.h` and `UnrealType.h`
declarations. Epic's [package object enumeration](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/CoreUObject/UObject/GetObjectsWithPackage?application_version=5.5)
and [property API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/CoreUObject/FProperty)
provide reference context; their current web versions are not the source of
the pinned 5.6.1 signatures. `ExportTextItem_Direct` is intentional: the
delta-export wrapper returns false for a property matching its default.

`SnapshotMesh` waits for the requested mesh's compilation, then inspects render
weight buffers, section/bone ranges, finite positions, morph indices and deltas.
It also records every mesh reference bone's local translation, quaternion and
scale, so a bind-transform change cannot pass as a namespace-only edit.
It does not edit or save the mesh. `audit_meshes.py` adds bindings/materials and
source hashes. Set `CSS_RIG_AUDIT_STAGE` to a private output directory containing
`input.json` with `meshes` entries (`package`, `source_file`, `sha256`). Mount
authoring Content read-only and retain a private writable DDC. Results distinguish
editor-built buffers from installed cooked data.

`SnapshotMeshSections` reads selected source material groups by slot name. It
records global vertex/triangle/instance IDs, positions, raw skin weights,
corner normals/colors/UV channels and source morph deltas. Missing or ambiguous
slots fail explicitly. It does not save or modify the mesh. The Genessa facial
integration uses these snapshots to establish point and corner correspondence
before editing private candidates.

For a complete mesh snapshot, pass an empty slot array to enumerate every
source polygon group. `SnapshotRenderSections` likewise accepts an empty array
to enumerate every actual render section. Declared material tables can contain
unused slots, so requesting the entire table as if it were a section list is
incorrect. Named selection retains its strict missing/duplicate checks. The
complete material table remains part of `SnapshotAssetProperties`.

The Eve Planet Diving check in `eve-rig-editor1/section-selection-check.json`
confirms 29 declared materials, 27 render sections and 29 source groups covering
all 175,142 source triangles. Full enumeration equals explicit selection of the
actual groups; missing/duplicate render requests and a missing source request
still fail. The read-only cooked baseline records six meshes with unused
material slots in `eve-mesh-comparison1/unused-material-slots.json`.

`SnapshotRenderSections` captures the built position/tangent/UV/color buffers,
skin weights, section-local indices, cloth mappings and explicitly requested
morph deltas. Local indices allow comparisons when earlier sections gain
vertices. It currently requires one LOD. This is an editor-built-buffer audit,
not evidence of cooked or in-game equivalence.

`CompareMeshCloth` compares distinct meshes' clothing assets in array order.
It checks every reflected asset property, including physical mesh data and
instanced solver configurations, plus the non-reflected transition mappings
and tether arrays. It reports mismatches by property, shared-object aliases
and counts. A detached physical-data copy verifies that the comparison rejects
a changed simulation vertex. Empty clothing arrays and unsupported types fail.
The probe does not mutate or save either mesh. Matching original data is not
evidence that the original simulation behaves correctly in every pose.

`SnapshotClothCustomData` records each clothing asset's transition mappings and
Euclidean/geodesic tether batches in array order. It serializes explicit fields,
not struct padding, and rejects nonfinite data or unsupported clothing classes.
The namespace migration combines this with reflected cloth/configuration
properties and render/source geometry digests. An empty clothing array is valid
for this snapshot because not every migrated mesh has cloth.

Copy `Source/` into an isolated project with an editor module named
`CSSRigAudit`, then build its `CSSRigAuditEditor` target against the local
UE 5.6.1 installed engine. Enable PythonScriptPlugin and
EditorScriptingUtilities in that project. The namespace trial uses
[`migrate_namespace_trial.py`](../animations/migrate_namespace_trial.py).

Set `CSS_NAMESPACE_STAGE` to the isolated project directory. Its `input.json`
contains `assets`, each with `old_package`, `new_package`, `source_file` and
`source_sha256`. Copy the input assets and their authoring dependencies into
the private Content tree first. Optional `refresh_retarget_reference_pose`
requires a loadable source reference mesh and records the pose change
separately from the migration comparison. Never mutate source assets through
hardlinks. Use read-only mounts for the original project.

Run the script through `UnrealEditor-Cmd -run=pythonscript -script=...`.
After a successful migration, move the old redirector files out of Content
and run a fresh process with `CSS_NAMESPACE_VERIFY=1`. Cook and independently
decode the result before accepting it. Source-data equality does not establish
compressed byte identity, visual quality or game compatibility.

For the expanded shared batch, `stage_namespace_animations.py` selects pinned
sequences/blend spaces and only their explicit dependencies. Its required
native-reference receipt supplies the real game skeleton in the private tree;
the main authoring project's smaller placeholder must not be used for this cook.
`refresh_packages` limits retarget-reference refresh to the named legacy clips.
`--include-playback` adds the five outfit adapters and three shared codec
settings. Pinned dependencies can come from their respective outfit authoring
roots; missing main-project copies are not a reason to create placeholders.

`package_namespace_trial.py` checks source/reload receipts and container payload
roundtrip. Run AssetReadback with `--animation-digests` on both the baseline and
candidate, then `compare_namespace_cook.py` to distinguish path changes from
changed compressed bytes. Differences are reported, not silently accepted.
`verify_namespace_motion.py` uses `CSS_NAMESPACE_STAGE` and `CSS_NAMESPACE_COOK`
to compare every independently decoded Eve key with raw UE poses. It writes
only to the staging project, so the cooked inputs can stay read-only. Its
`eve-decoded` input comes from `decode_shared_cooked.py --all-frames
--runtime-sampling` and the cook's explicit `catalog.json`.

The first accepted offline trial is recorded in
[`three-outfit-rig-physics-status.md`](../../../docs/development/three-outfit-rig-physics-status.md).

`--include-astral` adds the shared ghost materials, culling instances, neutral
textures and copy-pose graph. Inputs on another filesystem use independent
copies; same-filesystem inputs require reflinks. Both paths verify source hashes
and reject hardlinks. Material graphs/instances use reflected property snapshots;
textures additionally hash every source mip. The current texture audit requires
one populated block/layer and refuses unsupported source layouts.

The source-mip access is checked against the installed 5.6.1
`Engine/Texture.h` (`FTextureSource::GetMipData`) and
[Epic's API reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FTextureSource).
The compiler's empty mutable-data cleanup is verified in local
`AnimBlueprintCompiler.cpp`, `FAnimBlueprintCompilerContext::BuildDataArea`.
Only an empty `AnimBlueprintGeneratedMutableData` directly owned by its generated
animation class is excluded from the property snapshot.
