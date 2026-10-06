# Explicit-time performance layers

2026-10-06. These adapters are included in the installed private trial with
CSS/CINE controls. The initial offline evidence below does not establish live
acceptance. See `performance-service.md` and `performance-facial-curves.md` for
the runtime and current installation.

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
- `CSSPerformanceWeight`: explicit blend alpha, zero by default.

A SequenceEvaluator samples explicit time without advancing its own clock,
extracting root motion or running notifies. Runtime must supply the clock,
looping/once/hold policy and interruption rules. This avoids editing a live
node's internal accumulator or evaluated bone buffers. See Epic's
[SequenceEvaluator API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/AnimGraphRuntime/AnimNodes/FAnimNode_SequenceEvaluatorBase?application_version=5.6)
and the installed 5.6.1 `AnimNode_SequenceEvaluator.h`.

The branch requires both Enabled and a non-null reflected sequence reference.
The explicit weight drives a Two Way Blend. A thread-safe SelectFloat chooses
zero weight when the branch is disabled or its sequence is null. The original
idle output connects once to the base pose input; it is not fanned out to
multiple pose consumers. An earlier draft with two consumers failed the
zero-weight body comparison and was rejected before cooking or installation.
The smooth entry/exit weight comes from the same clock as sequence time, so
a second Unreal blend timer cannot outlive the retained sequence.
The initial IsValid graph call failed the strict compile because it was not
marked thread-safe. The accepted graph uses KismetMathLibrary's pointer
comparison, so threaded animation remains enabled. Runtime must still validate
the loaded clip, skeleton and binding before enabling playback.

The checker matches duplicated node subobject names and classes, not GUIDs.
UE's `PostDuplicateBlueprint` assigns fresh node GUIDs. It compares all existing
CSS default values and original pin defaults, then checks the new inactive
defaults and matching target skeleton. This is a structural check, not evidence
of equivalent evaluated physics.

Current evidence under `work/cine-animation-port/shared-v1/`:

- `performance-weight-stage2-result.json`: five warning-free graph compiles.
- `performance-motion-eval1-result.json` and `performance-motion-eval1-exit.json`:
  81 evaluated cases and successful commandlet exit.
- `performance-weight-cook1-exit.json`: successful Windows-target adapter cook.
- `performance-adapter-bindings.json`: independent cooked/source comparison.
- `pack-with-adapters-v2/verification.json`: 62 exact export round trips,
  consisting of 57 sequences and five adapters, with verified binding metadata.
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

Offline evaluation now covers nine cases on each of nine meshes (81 total):
inactive, active start, explicit seek, held endpoint, stop, null sequence,
restoration, zero weight and the full Black Hole clip. Static comparisons use
the last 30 of 180 frames. Full motion compares all 1,201 poses at 60 Hz on each
mesh, 10,809 poses total. The 19 checked body bones remain within 0.01 cm /
0.001 radians; every raw bone remains finite and normalized. This does not
exercise the world's cloth solver, gameplay interruptions, live frame costs,
or every clip's visual quality. Those remain acceptance work.

`verify_cooked_performance_layers.py` compares independent CUE4Parse readback
of the packaged adapters against the installed source graphs. Existing CSS
defaults and original node settings match, including rig classes, transferred
bones and input defaults. It allows only the inserted pose connection and
compiler-generated input-name GUID changes. Runtime field types, inactive
defaults, skeletons and blend connections are also checked. Runtime binding
metadata records each source outfit's verified container-table SHA-256 and
bulk size. This is compatibility evidence, not a cryptographic guarantee that
another installed pak cannot override the same asset path.

`CSSPerformanceEvaluation.cpp` and `evaluate_performance_layers.py` reproduce
this with `run_performance_layers.py --mode evaluate`. Input meshes, source
graphs, layer graphs and both clips are hash-checked after evaluation; none
are saved. `--mesh` limits a diagnostic run to one audited mesh.

The portable `PerformanceClock` now supplies explicit loop, once and pose-hold
timing, pause, seek, rate and stop behavior. It consumes game time, retains the
last pose during a 0.18-second exit blend and rejects nonfinite/out-of-range
requests before changing state. Its tests include partitioned ticks, two-key
photo poses, endpoint seeks, repeated stop, pause cleanup and rate changes.
The native service candidate now provides ownership, a world-time delta,
verified source-graph binding and transient restoration. CSS/CINE controls and
live acceptance are not complete.
See `performance-service.md` for the current boundary.

Before deployment:

1. Check live cloth, footwear, hands and Genessa wings throughout moving clips.
2. Accept source-graph binding/restoration in game, including native aiming
   layers and the selected idle's phase when the original graph is recreated.
3. Finish CSS/CINE controls and interruption tests for unload, death, travel,
   appearance changes and gameplay input.
4. Package and catalog checks now pass with each outfit alone; see
   `shared-animation-cook.md`. Validate live behavior with the same isolation.
5. Measure cold/warm costs and obtain live acceptance.
