# Eve cloth collision authoring

## Cloth-section morph readback

`CSSEveCloth -Inspect -Geometry -Morphs -Mesh=/Game/CSS/EveTest/SK_PFit14 -Report=<unused workspace JSON>` includes `cloth_section_morphs`: saved LOD0 position deltas indexed locally within each cloth section. Compare those indices with `render_geometry` mapping weights to establish whether the corrected attachment vertices remain skinned. Each section also reports `base_vertex`. This path reads assets without saving them. A successful dump is not proof of runtime morph playback.

## Private regional meshes

`prepare_skin_regions.py` creates closed, source-weighted collision copies. Use the verified `work/eve26/skin-regions2` input, not the first export containing disconnected remnants. Serialize them with system Python 3.14.7:

```sh
python3 tools/eve-fit/export_skin_regions.py --regions work/eve26/skin-regions2 --output work/eve26/unused-region-import
```

Import the three resulting JSON files separately with `CSSImportMesh -Input=<absolute file>`. They use unused `/Game/CSS/EveTest/SK_CPelv`, `SK_CThighL`, `SK_CThighR` and corresponding private skeleton packages. The importer refuses existing packages. The source 379-bone diagnostic rig is retained; these skeletons must never replace the production 386-bone rig. Cut geometry carries interpolated original weights and applicable morph deltas; morphs with no regional deltas are omitted and listed in the receipt. Face winding matches the existing import convention, with outward face normals.

The first right-thigh export failed native degenerate-triangle validation. Re-export only that region with `--only thigh_r --weld .001` into an unused directory. This repairs microscopic cut slivers and checks closed edge incidence. The completed inputs are `region-import/pelvis.mesh.json`, `region-import/thigh_l.mesh.json`, and `region-right-fix/thigh_r.mesh.json`. The first `region-import/thigh_r.mesh.json` is rejected. Keep the repair receipt with the candidate.

The level-set probe accepts these three exact mesh object paths through `-Mesh=...`. Whole-body recipe imports remain restricted to SK_CBody. Generate separate unused `PA_CBody...` outputs and fresh-load them using the same mesh argument. Read the actual `root_bone` and `bones` fields, since the automatic builder chooses a common ancestor, not a root based on the region's name. Use the source regional JSON as `-Samples=...` for rest or recorded-pose queries. `verify_region_rigs.py` checks saved rig/bind equality against SK_Waist, no assigned physics asset, and protected package hashes through the Python commandlet.

Successful import/generation is preparation only. Native surface/deformation checks, combined cloth motion, morph behavior and cost are still required before adopting these colliders.

## Offline lattice transfer comparison

Export all saved lattice nodes without changing the asset:

```sh
python3 tools/eve-fit/run_ue.py --log unused-grid.log -- -run=CSSEvePanel -LevelSet -InspectLevelSet -LatticeGeometry -Output=/Game/CSS/EveTest/PA_CBody128 -Report=/absolute/workspace/CustomShellSystem/work/eve26/unused-grid.json
```

From CustomShellSystem, use the pinned Blender and unused output directories:

```sh
../CSS-eins0fx-collections/reference-tools/blender/blender -b --python-exit-code 1 --python tools/eve-fit/test_local_lattice_grid.py -- --input work/eve26/ls128-grid.json --output work/eve26/unused-local --subdivide 4 --surface-frame sprint:56
../CSS-eins0fx-collections/reference-tools/blender/blender -b --python-exit-code 1 --python tools/eve-fit/render_lattice_surface.py -- --input work/eve26/unused-local/surface.json --output work/eve26/unused-views
```

Subdivision 1, 2 and 4 retain the original domain bounds. Reports compare all 291 recorded poses at default and combined hip/waist morphs. `weights.json` is an offline recipe, not a saved native collider. `surface.json` maps the original body triangulation and measures unsigned nearest distance; it does not evaluate the native implicit surface or containment. Colors carry face-average distances onto corresponding faces in both render sets. Original-grid frame-8 replay is checked against native frame 68 before accepting the complete report. Required existing inputs are named in the script; retain the body/grid hashes with each recipe.

## Private skinned level-set probe

`CSSEveLatticeRecipe.inl` imports an offline recipe through `-LatticeRecipe=<weights.json>` when creating an unused `PA_CBody...` asset. Stage it with `CSSEveLevelSet.inl`. The importer requires the private PA_CBody128 source, copies its SDF, checks every node against the original domain in reference space, resolves and normalizes influences, and duplicates the physics asset before replacing its lattice. It never assigns the result to a mesh. Creation metadata from the first `lrecipe-create.json` still contains unused default requested-grid fields; use its actual `bodies` grid sizes. Later code omits those defaults for recipe imports.

Completed trial: `/Game/CSS/EveTest/PA_CBodyLocal64`, from `work/eve26/ls-local64-surface/weights.json`. Fresh inspection with `-LatticeGeometry -Samples=<body-collider.json>` provides readback for:

```sh
python3 tools/eve-fit/verify_lattice_recipe.py --recipe work/eve26/ls-local64-surface/weights.json --readback work/eve26/lrecipe-rest.json --reference work/eve26/ls128-rest.json --output work/eve26/unused-verification.json
```

The verifier permits 0.002 cm rest-distance difference between embedding grids, records the measured maximum, and compares every node position and weight. The initial 0.001 cm threshold failed at 0.001215 cm; p95 was 0.000408 cm. This is numerical rest agreement, not fitting acceptance. The older rest-only report identifies its scope in text rather than a `sample_frame` field. The first build needed an explicit `.Get()` when deducing a pointer from `TObjectPtr`; the successful build is `lrecipe-build2.log`.

With motion and `-TraceSamples=...`, the report also records every inverse query embedding's rest position/phi and the selected surface's rest/posed positions. This distinguishes direct mapping error from the implicit query's projection path. Keep both measurements. Local64's frame-56 native test remains unaccepted despite accurate recipe readback and much better mapping.

The cloth motion probe accepts `-Physics=/Game/CSS/EveTest/PA_CBodyLocal64.PA_CBodyLocal64` instead of `-Body`. It validates the single weighted volume's bone names, temporarily sets the private cloth asset's physics pointer, restores it on exit and never saves the assignment. It excludes `-Body` and `-NoBodyCollision`. `audit_cloth_clearance.py` still compares the garment to the original skinned body, but omits triangle-collider metrics when no triangle reference exists. The report explicitly records that absence. Do not supply fake triangle data for an implicit volume.

Local64 cloth trial inputs: `-Motion -Frames=69 -Cloth=/Game/CSS/EveTest/CA_Fit.CA_Fit -Mesh=/Game/CSS/EveTest/SK_Waist.SK_Waist -Physics=/Game/CSS/EveTest/PA_CBodyLocal64.PA_CBodyLocal64 -Input=<workspace>/CustomShellSystem/work/eve26/panel-warm-motion.json -Report=<unused path>`. Repeat with `-CCD` for the recorded temporal diagnostic. Both fitting results are rejected; see the goal checkpoint before rerunning them.

Stage `CSSEveLevelSet.inl` with `CSSEvePanelCommandlet.cpp` and its other includes. The authoring module needs the `PhysicsUtilities` dependency in addition to the existing panel dependencies. Do not replace the working project's Build.cs wholesale with the older tracked template.

`prepare_collision_body.py` extracts the unchanged body from the F12 export into private `SK_CBody` / `SKEL_CBody`. Import using `CSSImportMesh`, inspect using `CSSInspectMesh`, and run `verify_collision_body.py` through the Python commandlet. The private 379-bone import is not a replacement for the production 386-bone shared skeleton.

Generate with `-run=CSSEvePanel -LevelSet -Output=/Game/CSS/EveTest/PA_CBody64 -Grid=64 -Lattice=16 -Report=<unused workspace JSON>`. Add `-InspectLevelSet` to reload that saved collider without regenerating or saving it. Generation refuses existing output packages. Both modes refuse an existing report. This uses the exported UE 5.6.1 `FPhysicsAssetUtils::CreateFromSkeletalMesh` API with mesh assignment and progress dialogs disabled.

Generation and fresh-load metadata do not prove surface coverage, morph support, motion stability or affordable runtime cost. Do not assign this test collider to production assets before those checks.

Add `-Samples=<body-collider.json>` to inspection for signed-distance samples at rest. Add `-SampleMotion=<panel-warm-motion.json> -SampleFrame=68` to skin the body samples and deform a transient copy of the saved lattice using that recorded pose. No mesh or collider is saved by this path. It records both general and cloth-style queries (the latter excludes empty cells), posed body positions and direct lattice-mapped positions. `audit_levelset_samples.py` summarizes distances, `verify_levelset_pose.py` independently checks body skinning in Blender, and `render_levelset_samples.py` renders front/rear/side heatmaps. Counts include internal surfaces and creases, so inspect the images as well.

September 27 result: neither the 64/16 nor 128/16 whole-body configuration is accepted. The finer distance grid improves the resting surface, but the 128/16 lattice directly misplaces sampled skirt-region body points by up to 24.18 cm at sprint frame 68. Inspect its weight distribution before another generation trial; do not run cloth against it as if resting fit proved animated fit.

`-TraceSamples=2796,2807,2795` records exact tetrahedron corners, interpolation coefficients and native node bone weights for those body indices. The parser disables `FParse::Value`'s default comma separator; the first trial accidentally read only the first index, so check the returned trace count. `audit_lattice_trace.py` verifies reconstruction and compares effective weights. `test_local_lattice_weights.py` uses Blender to test nearest-body-triangle weight transfer on those corners only, independently replaying the original native corner positions first. Three sampled thigh points improve from 23.5–24.2 cm error to 1.2–1.5 cm. This is a counterfactual, not a rebuilt collider or a complete correction.

These files are the tracked source of the isolated `CSSEveCollision` editor commandlet. Copy them into `CSS-eins0fx-collections/tools/CSSAuthoring/Source/CSSAuthoring` to build with the existing UE 5.6.1 project. Inspect current files and running editor/build processes before replacing or compiling anything. This commandlet is separate from CSS's native game DLL.

Build `CSSAuthoringEditor Linux Development` with `-NoHotReload -NoUBA` and an absolute workspace-local `-Log` filename. Use the established bwrap workspace binding, local temporary directory and `DOTNET_CLI_HOME`. UBA otherwise selects `~/.epic/UnrealBuildAccelerator`, outside the writable workspace. The first build attempt was explicitly stopped after confirming that configuration; ordinary compilation with `-NoUBA` succeeded.

Creation arguments:

```text
-run=CSSEveCollision -Input=<workspace>/CustomShellSystem/work/eve26/cloth-capsules.json -Output=/Game/CSS/EveTest/PA_Holiday
```

The output must be unused and under `/Game/CSS/EveTest/PA_`. Input has up to 32 spheres with `bone`, `local_center_cm`, `radius_cm`, and up to 16 connections with two `sphere_indices`. Connected spheres become tapered capsules; unconnected spheres remain standalone. Bone names are checked against the existing shared skeleton. The command saves only the new collision package. It does not assign it to any mesh.

Fresh inspection arguments:

```text
-run=CSSEveCollision -Inspect -Output=/Game/CSS/EveTest/PA_Holiday -Report=<workspace>/CustomShellSystem/work/eve26/cloth-collision-native.json
```

Run `python3 tools/eve-fit/verify_cloth_collision.py` against that report. Unreal's Python reflection does not expose `PhysicsAsset.skeletal_body_setups` in this build; use the C++ inspector instead. Preserve production-asset hashes before/after authoring.

The current geometry is still a collision experiment with known body-coverage gaps. A saved/read-back asset is not cloth simulation acceptance. Shared skirt-to-trim mapping, anchors in a native simulation, animated collision and body morph checks remain necessary before cooking or installation.


## Experimental cloth binder

`CSSEveClothCommandlet` is an isolated, unfinished experiment restricted to `/Game/CSS/EveTest/`. `-NoSave` runs binding and rebuild checks without saving the mesh. The primary-only diagnostic now passes after deferring PostEditChange until section metadata is stored. Manually sharing the cloth GUID across `-Attachments` does not survive the stock engine rebuild and is not production-ready. See `docs/development/eve-outfit-goal.md` and `work/eve26/cloth-bind3.log` before using or changing this code. Do not deploy its output as completed Holiday clothing.


## Panel-cloth feasibility candidate

`CSSEvePanelCommandlet.{h,cpp}` builds a separate Chaos Cloth Asset from the private Holiday mesh's five dress sections and the prepared connected proxy. It does not modify the source mesh, body or shared skeleton. It currently has no morph import and inherits the private source mesh's skeleton assignment, so it is not ready for runtime integration.

Authoring-project setup: enable the `ChaosClothAsset` plugin in CSSAuthoring.uproject and add `ChaosClothAsset` and `ChaosClothAssetEngine` to the module dependencies alongside existing `Chaos`. The managed array collection header belongs to Chaos in UE 5.6.1, not GeometryCollectionCore. The pre-change project/module files are backed up as `work/eve26/panel-project-before.json` and `panel-module-before.cs`. Do not overwrite later project changes with those backups. Stage these two commandlet files into the project's Source/CSSAuthoring directory and use the same workspace-local build procedure above. No game DLL is rebuilt.

Creation (output must not already exist):

```text
-run=CSSEvePanel -Proxy=<workspace>/CustomShellSystem/work/eve26/skirt-proxies.json -Output=/Game/CSS/EveTest/CA_Holiday
```

Fresh readback:

```text
-run=CSSEvePanel -Inspect -Output=/Game/CSS/EveTest/CA_Holiday -Report=<workspace>/CustomShellSystem/work/eve26/panel-saved.json
```

The initial candidate uses the 3D rest surface with isotropic configuration; its projected 2D coordinates are not a tailored panel pattern and must not be used as an anisotropic fabric rest layout. The 20 cm anchor band, 12 cm falloff and 18 cm displacement limit reproduce the earlier proxy trial. These and collision geometry are trial settings, not approved cloth behavior. `pinned` in the report counts distances below 0.1 cm; `zero_distance` counts exact zero.


## Combined skirt motion experiment

`CSSEveMotionCommandlet` duplicates the existing secondary graph into unused `/Game/CSS/EveTest/ABP_Holiday`, retaining original nodes and CSS defaults, then appends twelve skirt bones. It uses the existing module's `CSSDynamicsRecipe.inl`. Stage its `.h/.cpp` alongside the other commandlets and rebuild the editor module. Run with `-run=CSSEveMotion -Recipe=<workspace>/CustomShellSystem/work/eve26/holiday-dynamics.json`.

`holiday-probes.patch` records narrowly scoped editor probe changes for `SK_HolidayBones`, the new graph and the three Eve movement clips. `enable_motion_probe.py` applied these once with workspace backups; it intentionally refuses to overwrite those backups. Inspect current source before any later application. The Python launcher requires `-run=pythonscript -script=...`, not `-ExecutePythonScript`.

**The first simulation is rejected.** `verify_holiday_motion.py` detects enormous translations despite finite poses and preserved unrelated bones. Do not deploy this graph or recipe. The goal checkpoint records the earlier hair solver lessons and the next controlled diagnostic. F11 garment fitting is a separate validated offline result.

The second simulation is also rejected. `-Output=/Game/CSS/EveTest/ABP_Holiday2` permits an unused private copy with geometry-derived centers. `evaluate_holiday2_motion.py` rejected jog, with sprint not reached. `verify_holiday_motion.py --candidate holiday2` records missing clips as failures and confirms 18.30 cm maximum jog displacement. Do not deploy either graph.

The sequential editor patches `holiday-controls.patch`, `holiday-inertia.patch`, then `holiday-centers.patch` document the force/reset diagnostic argument and second exact asset path. Corresponding one-time scripts preserve `.before-controls` and `.before-inertia` backups and reject overwriting them. Probe `holiday_control` bits are 1 spring-off, 2 spheres-off, 4 equal-axis inertia and 8 reset; nonzero values are restricted to private Holiday assets. These are transient controls, not production defaults. Existing hair/body graph and shared skeleton hashes were rechecked after the rejected second evaluation (`holiday2-protected.json`).

## Animation-follow foundation

`../build_skirt_follow.py` creates private `/Game/CSS/EveTest/CR_HolidayFollow` through existing Python graph helpers, requiring no editor module rebuild. It pairs with `holiday-follow.mesh.json` produced by `prepare_skirt_follow.py`, not the old circumferentially weighted mesh. Eleven carrier bones copy original garment driver deformation with bind compensation; no dynamics yet. `evaluate_skirt_follow.py` reloads the saved rig and checks 291 recorded upstream poses. Its first run failed on RigElementKey constructor ordering; the fixed named arguments pass in `follow-eval2.log`. Do not use the old follow-eval.log as the final result. Neither paired mesh import nor post-process component integration has been done yet.

Update: paired mesh import and component integration now pass. Stage the current CSSEveMotionCommandlet.cpp and review/apply `holiday-follow.patch` against current editor sources, then rebuild only while that authoring project's editor is stopped. `-run=CSSEveMotion -Follow -Output=/Game/CSS/EveTest/ABP_HolidayFollow -Recipe=<workspace>/CustomShellSystem/work/eve26/skirt-follow.json` creates an unused copied graph. It transfers all 24 skirt bones to retain compensating child locals. `evaluate_holiday_follow.py` compares both graphs on imported SK_HolidayFollow; `verify_follow_component.py` checks resulting skinning transforms in Blender. Results and scope are recorded in the goal checkpoint. The private baseline has no new cloth dynamics and is not a release candidate.


## Saved panel motion probe

`CSSEvePanelCommandlet.cpp` includes `CSSEvePanelMotion.inl`; stage both when building the editor module. The motion path loads the exact private SK_Holiday/CA_Holiday assets, consumes recorded local poses and exports actual Chaos particles without saving assets.

```text
-run=CSSEvePanel -Motion -Frames=9 -Input=<workspace>/CustomShellSystem/work/eve26/follow-sprint-base.json -Report=<workspace>/CustomShellSystem/work/eve26/unused.json
```

Optional diagnostic controls: `-Iterations=1..16`, `-Substeps=1..16`, `-CCD`, `-NoSelfCollision`, `-NoBodyCollision`. Iteration/substep zero means asset defaults. NoSelfCollision changes component properties and recreates the simulation proxy so the non-animatable property takes effect. NoBodyCollision temporarily clears the loaded asset physics pointer and restores it on scope exit; it never saves the asset. Record and check protected file hashes separately. Collision-off results are attribution controls, never fitting acceptance. Reports include requested settings, effective property values and used iteration/substep counts.

Verify coordinates with Blender `verify_panel_motion.py --input <report> --output <unused receipt>`. The verifier checks fixed points against the exact old panel proxy weights, not the newer F11 mesh. Its pass only confirms coordinate/pose ordering. `prepare_panel_render.py` requires that exact report hash in the receipt. Rendering its output with `render_skirt_bone_trial.py --surface-motion <prepared report>` replaces the main surface only; trim is not an engine render-mapping validation.

`CSSEvePanelBody.inl` adds optional `-Body=<body-collider.json>` to the motion probe. `extract_body_collider.py` exports the original body only. The helper builds a transient physics asset containing one FKSkinnedTriangleMeshElem, following the reference-root and inverse-bind construction in UE 5.6.1 PhysicsAssetUtils.cpp. It replaces the private asset's physics pointer for this process and restores it on exit. It saves nothing. Stage the new include with the other native source files. Full-resolution body input is an expensive feasibility reference and must not be cooked or installed as a finished collision setup.


`-Body` motion reports also export `body_reference_skin_cm`, independently skinned through the native shape API for comparison with `verify_body_proxy.py`. This is not internal solver collision-state readback. The lightweight proxies produced by `simplify_body_collider.py` remain rejected fitting experiments; see the September 27 measurements in the outfit goal log before reusing them.


For reduced-proxy diagnostics, stage `CSSEvePanelMapping.inl` together with the commandlet and existing helpers. Motion accepts `-Cloth=/Game/CSS/EveTest/CA_<name>.<name>` and exports particle normals. Inspection adds full render geometry and baked cloth mappings only with `-Geometry`. Proxy JSON can preserve the old pin/falloff height using `anchor_top_cm`; do not infer a new pin boundary from a simplified mesh's altered bounds. `resolve_panel_render.py` replays the pinned shader position formula, but is not a game-render acceptance test. Current CA_Small mapping has confirmed rest outliers and must not ship.

Stage `CSSEvePanelDeformer.inl` for explicit mapping experiments. `-MultiMap` generates five influences through the public ClothingMeshUtils API. `-RepairMap` additionally rejects inaccurate position/normal/tangent reconstructions, tries nearby alternative triangles, and fails before saving if any moving vertex remains unsupported. Its 0.01 cm reconstruction tolerance, barycentric L1 limit of 8, and alternative distance limit of nearest plus 2 cm are experimental authoring guards, not accepted production defaults. Preserve fully skinned endpoints exactly before uint16 blend conversion. `check_panel_mapping.py`, run in Blender, checks fresh native mapping readback against the proxy at rest. Neither a passing rest check nor a successful build proves motion fitting.

`-ContactCm=<0.1..1>` creates a separate collision-gap trial; default remains 0.3 cm. Pair any change with the same proxy, mapping, pose sequence and collider. `audit_cloth_clearance.py` compares simulation vertices/triangle centroids, resolved fabric and skin-only fabric against both the original posed body and native collider reference. Its signed nearest-normal measurements cover only the selected frame and default morph, so visual inspection and broader motion/morph checks remain required.

Creation and motion accept `-Mesh=/Game/CSS/EveTest/SK_<name>.<name>` for separate fitted candidates. The default remains SK_Holiday. Motion receipts identify the actual source mesh. Stage both commandlet and `CSSEvePanelMotion.inl` when adding this option. Do not overwrite a baseline mesh to test new geometry. `update_waist_proxy.py` preserves the old proxy's intentional pelvis-based simulation weights, topology and pin height while transferring the verified F12 fit through saved barycentric correspondence; render weights remain those of the new source mesh.

### Garment-follow and backstop diagnostics (September 27)

`prepare_follow_proxy.py --output work/eve26/panel-follow-proxy.json` creates an explicit weight-only trial from the fitted source. Run it with the workspace system Python. It refuses to overwrite output and verifies unchanged non-weight proxy data.

The panel creation commandlet accepts `-MaxMove=3`, `-BackstopRadius=30` and `-BackstopDistance=0`. Defaults remain 18, 0 and 0. Use a new private asset path, the prepared proxy and `-RepairMap`. The inspector exports `weight_maps`; verify saved values before running motion. CA_Follow uses default movement, CA_Near uses 3 cm, CA_Stop also enables the backstop. All use the same garment-follow proxy and existing body-joint6 motion input.

Pass `--mapping work/eve26/panel-stop-saved.json` to `verify_panel_motion.py` so its fixed-particle check uses the saved map. Resolve with `resolve_panel_render.py`, then audit using `audit_cloth_clearance.py --source-mesh work/eve26/holiday-waist-source.mesh.json`. Render rear and side with `render_skirt_bone_trial.py --mapped-render ... --lower-dress`; do not combine mapped-render with upstream mode.

These candidates are unaccepted. See the goal checkpoint and `panel-stop-comparison.json`. Official backstop reference: https://dev.epicgames.com/documentation/unreal-engine/clothing-tool-in-unreal-engine---properties-reference . Pinned implementation: `Chaos/PBDSphericalConstraint.h`, nonlegacy sphere center uses animated position minus (radius + distance) times animated normal. A correct target fit is still required.

## Prototype tail trial

`CSSEvePanel` accepts `-Slot=PlanetTail_17` to use only that render material and the identically named proxy key. Optional `max_distances` supplies one finite value per proxy particle, bounded by `-MaxMove`; absent this map, the historical Holiday anchor calculation remains. `-Physics` selects an existing `/Game/CSS/` physics asset. These options are for private candidates, not a production deployment.

`prepare_planet_tail_cloth.py` uses the aligned original 18-point proxy and four authored pins with nearest existing tail skin weights. Its 18 cm motion cap is a trial setting, not copied Blender solver behavior. The module compiled in `planet-panel-build-console.log`; private `/Game/CSS/EveTest/CA_PTail` was built in `planet-tail-create.log`. Fresh geometry inspection and rest mapping checks must precede motion claims. `check_panel_mapping.py --slot PlanetTail_17` selects this proxy for the existing readback verifier.

## Embedded Prototype tail candidate

The motion probe accepts `-Mesh=/Game/CSS/EveTest/SK_PFit13` for the fitted candidate, while retaining F7 as the default. Use `-Secondary` for the existing postprocess graph. `verify_planet_f13_sections.py` checks fresh materials/gameplay references and transient ShowMaterialSection hide/restore behavior; it does not exercise CSS persistence. If encountering the documented trace-worker startup crash, the pinned engine supports `-notracethreading -notraceserver`; these flags affect tracing, not cloth simulation. Preserve the crash evidence and distinguish startup failure from asset failure.

F13 uses a separate `/Game/CSS/EveTest/SK_PFit13` imported from `planet-f13-import/`, then SkeletonOnly-bound without MergeBones. `prepare_planet_f13.py` restores the original material/gameplay references after checking the shared skeleton counts and exact mesh bind pose. Bind the same authored proxy/config to `PlanetTail_17` with `PA_PTailRear` and prefix `P13Tail`. Fresh inspection can be checked with `verify_planet_embedded.py --input <report> --output <unused receipt> --asset /Game/CSS/EveTest/SK_PFit13.SK_PFit13`. Keep the F7 SK_PTailRun comparison asset intact. Current embedded motion defaults to F7; do not mislabel that output as F13 motion evidence.

Add `-Secondary` to the embedded motion probe to enable the mesh's existing postprocess graph; it fails if the instance is missing and records `secondary_enabled`. Inspection accepts `-Geometry` to export saved cloth-only render sections through the shared mapping helper. Stage `CSSEvePanelMapping.inl` with the cloth commandlet as well as with the panel commandlet. `resolve_panel_render.py` can replay this inspection directly. `review_outfit_export.py --cloth-render <replay> --pose-motion <same native report> --pose-frame <same frame>` replaces those named material sections with the replay and renders them alongside the posed body. It checks the simulation hash and rejects extra morphs that were absent from the simulation.

Stage `CSSEveEmbeddedMotion.inl` alongside the cloth commandlet. `-run=CSSEveCloth -Motion -Clip=Sprint -Report=<unused workspace report>` runs the private SK_PTailRun skeletal component for 65 frames. Walk and Jog are also accepted. The probe enables physics in its preview world, disables the postprocess blueprint for isolation, explicitly waits after each cloth tick and records particles, normals and raw bone poses. It never saves packages. A non-null simulation pointer is insufficient: preview worlds default to simulation disabled, which prevents cloth actor creation. Verify the output with `verify_panel_motion.py`, the authored tail proxy and the fitted mesh using `--slot PlanetTail_17`. Passing this check establishes pin coordinates only, not final visual quality or game performance.

The existing `CSSEveCloth` skeletal-mesh path accepts the same named `slots` proxy. It now honors an optional `max_distances` array exactly (finite0..35 cm, one value per particle) and an optional per-slot `Iterations` setting1..16. Defaults remain unchanged without those inputs. The original four tail pins must survive this path; do not substitute the component-top heuristic.

`prepare_planet_embedded.py` duplicates the fitted mesh into `SK_PTailRun` and restores existing production gameplay physics/secondary references without saving production packages. Bind only `PlanetTail_17`, using `planet-tail-cloth.json`, `planet-tail-config.json`, and private `PA_PTailRear`. This avoids requiring a new standalone ChaosClothComponent item in CSS. It still needs fresh saved-asset and actual skeletal-cloth motion validation; panel-component tests are not equivalent runtime evidence.
