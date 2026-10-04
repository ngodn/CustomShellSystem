# Shared asset authoring

This UE 5.6.1 editor-only module creates `/Game/CSS/SharedAssets/Astral/ABP_CopyPose`.
The generated animation template has no target skeleton and uses the engine's
Copy Pose From Mesh node. It reads its attached parent, including curves and
custom attributes, before the visual mesh's existing post-process graph runs.
Its generated runtime parent is `UAnimInstance`; the editor module is not a
runtime plugin or part of the DLL distribution.

The native double keeps its original mesh and combat animation. A separate
visual component can use this template to follow that double. Do not attach
it to the player, assign the player's idle, or replace the double's skeleton.

## Isolated check

Use the project's existing UE 5.6.1 engine, C++20 editor toolchain and bundled
Python 3.11. Host staging uses Python 3.14 on this machine. The runtime remains
C++23. Stage on the secondary drive; source asset links are read-only during
the commandlet. This module has its own Binaries directory and does not use
or rebuild Unholy Genessa's authoring module.

```bash
python3 tools/shared-assets/stage-authoring.py SOURCE_CONTENT NEW_STAGE
ENGINE/Build/BatchFiles/Linux/Build.sh CSSSharedEditor Linux Development \
  NEW_STAGE/CSSShared.uproject -NoHotReload -MaxParallelActions=4
bash tools/shared-assets/run-pose-check.sh ENGINE NEW_STAGE pose pose1
bash tools/shared-assets/run-pose-check.sh ENGINE NEW_STAGE physics physics1
```

The first check creates the template and refuses to overwrite an existing one.
It compares 60 moving frames on the same and different skeletons, with visual
post-process disabled. The second reloads the saved template and enables the
outfit's authored post-process. It checks that the extra wing bones move and
the source animation instance remains unchanged. Its `passed` field establishes
secondary animation execution, not combat-pose equivalence. Per-bone differences
are recorded for that separate assessment.

These are manually ordered component evaluations in an editor world, without
rendering or collision. They do not prove live tick ordering, attacks, weapon
grips, cloth collision, speed-driven wing transitions, cooked Windows behavior,
resource cost, or runtime cleanup. No game package or installed runtime is
changed by these commands.
