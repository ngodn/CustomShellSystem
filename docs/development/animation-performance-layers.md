# Explicit-time performance layers

2026-10-06. Editor candidates only. Not cooked, integrated or installed.

Five isolated post-process blueprints compile without Blueprint warnings and
pass fresh-process contract checks. Their original skeleton, CSS control
defaults, graph nodes and pin defaults remain intact.

| Family | Authoring source | New adapter | Preserved controls / nodes |
| --- | --- | --- | --- |
| Eve | AnimLab/ABP_Idle | Locomotion/Adapters/ABP_Eve | 31 / 38 |
| Eve bikini | EveTest/ABP_BikiniFeet2 | Locomotion/Adapters/ABP_EveBikini | 32 / 44 |
| Eve knitwear | EveTest/ABP_KnitFeet1 | Locomotion/Adapters/ABP_EveKnit | 32 / 44 |
| Genessa | UnholyGenessa/ABP_Wings2 | Locomotion/Adapters/ABP_Genessa | 31 / 56 |
| Commander White | CWTest/ABP_BodyHairB4 | Locomotion/Adapters/ABP_CommanderWhite | 29 / 35 |

Paths in the table are beneath `/Game/CSS/`. Eve's authoring idle layer is
`AnimLab/ABP_Idle`; the released graph uses `SeduXtress/ABP_Secondary` after
packaging relocation. The old editor `SeduXtress/ABP_Secondary` lacks the idle
controls and is not the source for this adapter.

Each new branch sits after the existing idle switch and before its original
secondary-motion chain. It adds four reflected controls:

- `CSSPerformanceSequence`: AnimSequence, null by default.
- `CSSPerformanceEnabled`: false by default.
- `CSSPerformanceTime`: explicit seconds, zero by default.
- `CSSPerformanceBlendOut`: 0.18 seconds by default; runtime may request immediate interruption.

A SequenceEvaluator samples explicit time without advancing its own clock,
extracting root motion or running notifies. Runtime must supply the clock,
looping/once/hold policy and interruption rules. This avoids editing a live
node's internal accumulator or evaluated bone buffers. See Epic's
[SequenceEvaluator API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/AnimGraphRuntime/AnimNodes/FAnimNode_SequenceEvaluatorBase?application_version=5.6)
and the installed 5.6.1 `AnimNode_SequenceEvaluator.h`.

The branch requires both Enabled and a non-null reflected sequence reference.
The initial IsValid graph call failed the strict compile because it was not
marked thread-safe. The accepted graph uses KismetMathLibrary's pointer
comparison, so threaded animation remains enabled. Runtime must still validate
the loaded clip, skeleton and binding before enabling playback.

The checker matches duplicated node subobject names and classes, not GUIDs.
UE's `PostDuplicateBlueprint` assigns fresh node GUIDs. It compares all existing
CSS default values and original pin defaults, then checks the new inactive
defaults and matching target skeleton. This is a structural check, not evidence
of equivalent evaluated physics.

Evidence under `work/cine-animation-port/shared-v1/`:

- `performance-layers.json`, `performance-layers-verify.json`
- `performance-stage4-console.log`, `performance-stage4-exit.json`
- `performance-verify2-console.log`, `performance-verify2-exit.json`
- `authoring-source-links.json` records the read-only commandlet source locations.

The authoring source graphs and all five outputs retain their recorded hashes.
Genessa and Commander White sources live in their own authoring projects and
must be mounted read-only when running the shared project's commandlets.
Do not copy a replacement module over those projects or save through their
source mounts.

The temporary directory symlinks were removed after the initial checks.
`run_performance_layers.py` now binds the originals onto three empty authoring
directories only inside bwrap. It also makes the original locations read-only.
Outside the commandlet, the shared project has no writable links into those
source trees. Reproduce a fresh readback with a new report label:

```sh
python3 tools/authoring-probes/animations/run_performance_layers.py \
  work/cine-animation-port/shared-v1 --mode verify --label performance-next-check
```

Before deployment:

1. Compare active, held, disabled and null-clip evaluation against the original
   graphs on all nine current meshes. Check physics, footwear and Genessa wings.
2. Bind the runtime only to verified source graph versions. A matching skeleton
   or source asset path alone cannot establish that an adapter preserves a newer
   outfit's physics graph.
3. Add owned playback and restoration through the shared CSS/CINE service.
   Preserve original override class and CSS controls; handle reinit, unload,
   death, travel, appearance changes and gameplay interruption.
4. Cook adapters with their declared original outfit dependencies. Existing
   outfit containers must remain untouched.
5. Verify packaged load, measure cold/warm costs, and obtain live acceptance.
