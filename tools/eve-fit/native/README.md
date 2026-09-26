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
