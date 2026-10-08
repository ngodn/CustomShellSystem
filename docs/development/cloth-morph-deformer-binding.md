# Cloth morph deformer binding

Status: runtime and coordinated outfit/shared trial installed October 9;
authoring, reload, Windows cook and independent readback pass. Installed hashes
are verified; live validation remains pending.

An authored Optimus graph opts in by declaring zero-default Float variables
named `CSS_Morph_<morph name>`. For example, `CSS_Morph_PBMGlutesSize` receives
the same weight as the `PBMGlutesSize` body-shape control. The mesh supplies its
default deformer and target collection. CSS does not load a graph by filename,
replace a component's graph override, or create a second cloth simulation.

The runtime uses the game's reflected `GetMeshDeformerInstance` and
`UOptimusDeformerInstance::SetFloatVariable`. The latter takes a double, despite
its name. These signatures were checked against the shipped CXXHeaderDump and
the local UE 5.6.1 implementation. The public API reference is
[UOptimusDeformerInstance](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/OptimusCore/UOptimusDeformerInstance?application_version=5.6).

Each player, preview, worn item and private Astral component has its own binding.
Bindings hold weak object handles. Unchanged weights and unsupported variables
are cached for that instance. A replacement instance receives a fresh snapshot,
including variables that were unsupported by the previous instance. Missing
graphs leave the ordinary morph path intact. Updates run on the game thread
through the existing customization and maintenance paths.

Removing a controlled shape resets its graph variable to zero. Releasing the
binding resets only variables that it successfully wrote, and only while the
same component, mesh and deformer instance remain attached. Graph authors must
use zero as the uncorrected default and reserve these variables for CSS shape
weights. This interface does not animate facial graph variables every frame;
facial playback continues through its existing owned morph path.

The Genessa prototype has five shape bases and fifteen vertex-attribute buffers.
The graph adds their weighted offsets only to cloth sections whose normal morph
input is disabled. Normal morph sections retain their normal input. The graph
recomputes normals/tangents after the correction. Authoring, cooking and GPU
validation must cover both the graph and mesh; naming a variable alone does not
give an older cloth asset morph support.

Validation so far: the Windows core builds and all 24 registered host tests pass,
including binding release, missing variables, instance replacement, invalid
weights and failed-write retry. Those host tests do not execute Unreal
reflection, cloth or shaders. World/preview/doubles, replacement recovery,
materials, contact and performance still require the integrated candidate.

Private asset evidence: `genessa-deformer-render1/basis-runtime1`,
`genessa-deformer-cook4` and `genessa-deformer-pack3` under the rig study work
directory. Both meshes retain all 137 morphs, 26 sections and original cloth
data, with fifteen added vertex-attribute buffers and the authored graph and
collection references. Cooked graph readback confirms five zero-default GPU
variables, equivalent kernel source and SM5/SM6 compilation. The diagnostic pack
includes comparison meshes and must not be installed as an outfit release.

The final candidate is `genessa-cloth-assembly1`. It preserves all four existing
Genessa mesh paths and assigns the shared `DG_ClothMorphBody` graph and
`MDC_ClothMorphBody` collection under `/Game/CSS/eins0fx/Shared/Deformers/`.
The two regular meshes retain 22 morphs each; the two facial replacements retain
137 each. Independent cooked comparison permits only the documented graph and
attribute additions. The runtime contract is unchanged by the graph rename.
All 737 package import checks and 1,380 catalog checks pass. World rendering,
contact and runtime cost still require this integrated candidate in the game.
