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
