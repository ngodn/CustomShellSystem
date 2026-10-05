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

Offline evaluation, 2026-10-06: `performance-all-eval1-result.json` and its
exit receipt confirm seven cases on each of the nine audited meshes (63 total).
Inactive, active start, explicit seek, held endpoint, stop, null sequence and
restoration all match the original graph on the 19 checked body bones within
0.01 cm / 0.001 radians. Every raw bone remains finite and normalized. The
comparison runs compressed poses and post-process graphs at 60 Hz, with the
last 30 of 180 frames used after settling. It does not exercise the world's
cloth solver, live movement, gameplay interruptions or a moving clip's full
duration. Those remain acceptance work.

`CSSPerformanceEvaluation.cpp` and `evaluate_performance_layers.py` reproduce
this with `run_performance_layers.py --mode evaluate`. Input meshes, source
graphs, layer graphs and both clips are hash-checked after evaluation; none
are saved. `--mesh` limits a diagnostic run to one audited mesh.

The portable `PerformanceClock` now supplies explicit loop, once and pose-hold
timing, pause, seek, rate and stop behavior. It consumes game time, retains the
last pose during a 0.18-second exit blend and rejects nonfinite/out-of-range
requests before changing state. Its tests include partitioned ticks, two-key
photo poses, endpoint seeks, repeated stop, pause cleanup and rate changes.
The engine service still needs to provide ownership, a world-time delta,
binding validation and restoration; the clock alone does not play a clip.

Before deployment:

1. Extend the passing held-pose checks to full moving clips. Check live cloth,
   footwear and Genessa wings.
2. Bind the runtime only to verified source graph versions. A matching skeleton
   or source asset path alone cannot establish that an adapter preserves a newer
   outfit's physics graph.
3. Add owned playback and restoration through the shared CSS/CINE service.
   Preserve original override class and CSS controls; handle reinit, unload,
   death, travel, appearance changes and gameplay interruption.
4. Cook adapters with their declared original outfit dependencies. Existing
   outfit containers must remain untouched.
5. Verify packaged load, measure cold/warm costs, and obtain live acceptance.
