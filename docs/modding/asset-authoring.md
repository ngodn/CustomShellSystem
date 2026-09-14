# Make the outfit work before packaging

The application is **Unreal Editor**, part of Unreal Engine. Blender, Maya or another modeling application handles geometry, UVs and skin weights. Unreal Editor imports that work, defines Unreal materials and physics, and cooks runtime assets. CSS packages those finished assets and presents them in the wardrobe.

An existing working Mortal Shell II replacement can skip authoring and go directly to the [package workflow](README.md). An FBX, `.blend`, uncooked `.uasset` or another game's `.pak` cannot.

## Target the game's rig

The locally tested game is UE 5.6.1, CL93241. Use a verified reference outfit for the gameplay skeleton you intend to support. Record its Skeleton object path, reference hierarchy/transforms, skeletal mesh, material slot order, Physics Asset and post-process animation references.

CSS currently requires the selected mesh and current player mesh to reference the **same loaded Skeleton asset**. This is a direct object comparison in [Appearance::apply](../../native/src/engine.cpp), stricter than matching bone names. Keeping an imported copy called `SKEL_Human` is not equivalent to referencing the game's actual skeleton. Do not ship a duplicate skeleton stub or replace the shared game skeleton just to pass this check.

Preserve bone names, parents and reference transforms. Fit the body and garment together, including hands, feet, face boundaries and intended openings. Check scale, facing direction, normals, winding, UV seams, slot order and weights. Test bends, twists and attack poses before cooking. A good rest pose cannot reveal all skinning defects. Epic's [skeletal mesh pipeline](https://dev.epicgames.com/documentation/en-us/unreal-engine/fbx-skeletal-mesh-pipeline-in-unreal-engine?application_version=5.6) describes import and material-slot requirements; its [skeleton guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/skeletons-in-unreal-engine?application_version=5.6) explains hierarchy compatibility.

For FBX, record the exact exporter settings and round-trip a known reference first. Epic's UE 5.6 importer uses FBX 2020.2. There is no verified universal Blender axis/scale preset in this repository. Do not apply a copied “rotate 90 degrees” recipe without comparing reference transforms and actual output. [Blender FBX documentation](https://docs.blender.org/manual/de/4.5/addons/import_export/scene_fbx.html).

## Materials and colors

Reconstruct the shader inputs the target material expects: base color, alpha, normal, roughness, metallic and any packed channels. Verify sRGB, normal-map orientation, transparency and material slot assignment. Shader graphs from another game do not transfer with extracted textures.

For new materials, expose intentional vector/scalar parameters for parts you want users to edit. Keep cloth, skin, metal and emissive effects distinct where appropriate. For existing game materials, inspect parameters that actually affect the cooked shader. CSS's [color recipes](colors.md) bind to those parameters or provide per-region texture layers. Parameter names are asset-specific, not universal CSS keywords.

New master materials require a working cook and shader path for the Windows game. Retoc repackaging does not compile shaders. A material that renders in Blender or a Linux editor is not evidence that its Windows shader resources exist. Static switches are compile-time choices, so CSS's sliders do not toggle them. [Epic material instances](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-materials-in-unreal-engine?application_version=5.6).

## Physics and animation

CSS does not implement Kawaii Physics. It retains supported mesh-linked Unreal physics and post-process animation data while the game continues to drive player animation. Separate these authoring tasks:

- Physics Asset bodies, constraints and collision.
- Secondary-motion bones, weights and a compatible post-process animation blueprint.
- Cloth simulation geometry, painted weights, collision and LOD-section bindings.

Do not assume imported geometry contains working cloth or body motion. A source mod that relies on a custom actor, extra skeletal components, gameplay Blueprint initialization or an absent native plugin needs additional integration. The package format does not spawn arbitrary accessories or load physics plugin code. See Epic's [Physics Asset Editor](https://dev.epicgames.com/documentation/en-us/unreal-engine/physics-asset-editor-in-unreal-engine?application_version=5.6) and [clothing tool](https://dev.epicgames.com/documentation/en-us/unreal-engine/clothing-tool-in-unreal-engine?application_version=5.6).

## Cook and stage only the outfit

Cooking produces platform-specific runtime assets. Use a project and cooker verified for the target Windows build, then stage the outfit's cooked dependencies. Preserve references to game-owned skeletons and reusable game assets; include new or modified outfit assets without unintentionally replacing unrelated game content. Epic separates [build, cook, stage and package](https://dev.epicgames.com/documentation/en-us/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine?application_version=5.6).

The CSS converter accepts finished Unreal legacy `.pak` files or IoStore `.utoc/.ucas` containers with their companion `.pak`. For an already-verified loose **cooked** legacy asset tree, repak can make the input `.pak`:

```text
cooked-staging/
  YourProject/Content/YourAuthoringProject/Characters/MyOutfit/...
```

```sh
repak pack '/path/to/cooked-staging' '/path/to/MyOutfitSource_P.pak' --version V8B
```

Keep `.uasset`, `.uexp` and bulk companions together wherever the cooker emits them. This command only archives files; it does not cook editable assets, fix skeleton references or create shaders. A project-wide game executable, map, configuration or unrelated content is not an outfit input. [Repak](https://github.com/trumank/repak) handles pak archives; [retoc](https://github.com/trumank/retoc) handles IoStore and cooked-format conversion.

Use descriptive, sufficiently long asset paths before cooking. Current relocation preserves encoded lengths and requires at least 42 ASCII characters before a numbered package suffix. Short paths such as `/Game/Mesh` are refused. A path such as `/Game/YourAuthoringProject/Characters/MyOutfit/Meshes/SK_MyOutfit` leaves room. Do not lengthen cooked paths with a text editor.

CSS scans containers using the game's base packages to resolve external dependencies, including `global.utoc` and `global.ucas`. Those dependencies are development inputs, not files to ship inside the outfit ZIP.

## What is not yet a general SDK

The Seductress work proved a specific Blender-to-Unreal route with a custom UE 5.6.1 asset cooker, authored meshes/textures and existing game shader resources. Its Windows asset cook from a Linux editor was limited and used custom tooling. It does not establish a stock Linux-to-Windows material pipeline for every modder.

The [experimental authoring source](advanced-tools.md) includes the mesh exporter, UE project and custom commandlets from that work. It does not include target skeleton/reference assets, a one-click source-game importer or a complete general creation SDK. The package standard starts at a compatible cooked outfit. The [research note](../modding-research.md) records the remaining proof needed for a general creation workflow. Modders with an existing MSII replacement workflow can use the package tools now; new asset authors still need to establish that game integration.
