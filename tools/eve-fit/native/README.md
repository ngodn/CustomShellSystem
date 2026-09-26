# Eve cloth collision authoring

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
