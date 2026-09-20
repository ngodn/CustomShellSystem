# Game-derived foundation comparison, 2026-09-21

The isolated game-derived Skeleton does not by itself fix the remaining hand
deformation. With CSS translation modes, its H2 pose is identical to the earlier
compact reference-rotation candidate at all five tested samples. Using the game
translation modes instead visibly stretches hands/wrists and is rejected.
Reverting to V42 hand binding also fails the visual review. V43 remains installed;
no game assets, appearance settings or player input changed.

The [research](skeleton-foundation-research.md) and
[authored hand-graph checkpoint](authored-hand-ik.md) explain the test's scope.
These remain partial AnimGraph fixtures, not complete gameplay or release tests.

## Controlled comparison

`work/grip-grounding-v1/game-foundation-v2-probe-v1/` contains an imported
1,396-bone diagnostic Skeleton: the unchanged 1,199 game records followed by
197 CSS-only records with explicitly remapped parent indices. Eve's 379 mesh
bind records remain unchanged. Nine virtual bones are restored separately.
This probe does not migrate sockets, blend profiles, slots or other production
metadata and is not a distributable CSS skeleton v2.

A live read confirms V43's actual CSS Skeleton has
`bUseRetargetModesFromCompatibleSkeleton=false`. Therefore its own per-bone
translation modes apply in the tested state. Evidence: `live-mode-flag.json`
and the acknowledged read-only request log.

The engine compares MoreBeaute, V43, the compact rotation candidate, the full
foundation with game modes, and the full foundation with CSS modes. Five source
samples, raw/compressed paths and left IK off/on produce 100 cases. Enabled IK
reaches its target within 0.001 cm; production file hashes remain unchanged.
Full-foundation/CSS-mode poses exactly match compact-candidate poses at the
tested local transforms. Raw/compressed differences remain below 0.015 cm and
0.195 degrees. Passing mechanics do not accept the visible deformation.

Reviewed audit view B files under SeduXtress `work/nextgen-audit/`:

- `game-foundation-foundation-s1-v1`: game modes, visibly stretched wrists/hands.
- `game-foundation-foundation_css_modes-s1-v1`: same remaining sharp wrist and
  finger issues as the compact candidate.
- `game-foundation-v42-bind-s1-v2`: older binding still has wrong grip/fingers.

Unsigned finger distances can improve while the wrist stretches. Do not select
the game-mode candidate from its smaller distance numbers.

## Editor crash and fixture correction

The first V42 follow-up, `game-foundation-v42-bind-v1`, crashes during animation
compression before any pose output. The call stack enters
`FBoneContainer::Initialize` from compression resampling, with index 1275 into
an array of size 1275. This is the offline editor, not the running game.

The 258-bone MoreBeaute mesh contains 76 names missing from the 1,199-bone source
Skeleton fixture. In editor builds, `USkeleton::BuildLinkupData` adds missing
mesh bones automatically. This invalidates the assumption that the source stays
unchanged while compression prepares bone arrays and virtual indices. See
`Engine/Source/Runtime/Engine/Private/Animation/Skeleton.cpp`,
`BuildLinkupData`, in the pinned UE5.6.1 source.

`game-foundation-v42-bind-v2` constructs the source coverage explicitly: the
unchanged 1,199 game records plus 76 MoreBeaute-only records. The authored
`SK_DarkForm` reference retains its original raw entries and moves its nine
virtual entries after the added records. There are 1,275 raw / 1,284 evaluated
source records. No original animation track is removed. Every evaluation checks
that the source hierarchy and transforms remain unchanged.

Import and evaluation exit zero. All 60 control/V43/V42 cases complete. The
corrected MoreBeaute and V43 results exactly match the corresponding raw and
compressed poses from the first foundation run at all five samples. This
supports those earlier numeric comparisons despite the fixture's unsafe setup.
V42 bind data also independently matches the renderer's older source within
0.001, with exact translations/scales. The alternate renderer now requires both
an explicit mesh identity and an independent bind readback.

`BindDiagnosticMeshSkeleton` now rejects any missing raw bone or mismatched
parent before assigning the Skeleton. The editor build passes. Actual guard
checks reject the incomplete source and production Skeleton without changing
the mesh; a complete source is accepted with unchanged mesh bind and reference.
The change is saved in
[diagnostic-binding-coverage.patch](../../tools/authoring-patches/diagnostic-binding-coverage.patch).
It is already applied. The earlier hand/game-reference patches precede it.

## Next binding defect to isolate

The original importer maps both `upper_arm.bend.twk.*` and
`upper_arm.twist.twk.*` to `upperarm_*`, and both forearm counterparts to
`lowerarm_*`. A read-only Blender audit confirms the source body's upper-arm
twist groups affect 445 vertices per side and forearm twist groups affect 436
per side. All eight corresponding game arm-twist groups on V43 have zero body
weights. The H2 clip includes tracks for all eight bones.

This proves that the binding discarded separate twist influences. It does not
yet prove how much of each wrist, elbow or finger defect it causes. Next isolate
an anatomically correct twist binding and reference-pose alignment against the
same control, preserving original geometry/proportions and accepted dynamics.
Do not transfer all Genessa weights or scale Eve to the guide's example body.

Evidence: `game-foundation-v42-bind-v2/arm-twist-weight-audit.json`, source and
target hashes, `reference-frame-comparison.json`, `validation.json`,
`v42-render-binding-validation.json`, `binding-guard-results.json`, build and
command logs. The source/target blend hashes are unchanged.

Reproducible preparation and archived probes are in
[tools/authoring-probes/game-foundation](../../tools/authoring-probes/game-foundation/README.md).
The preparer's output matches both evaluated reference inputs exactly.
