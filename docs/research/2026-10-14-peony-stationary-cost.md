# Peony stationary timing reference, October 14

Three 15-second developer-core captures completed with the same Peony appearance
and view at their boundaries. All 3,133 records pass an independent archive
check. This is a reference for the current installed build, not a baseline versus
candidate comparison or a performance acceptance result.

## Capture and preservation

Command from `CustomShellSystem`:

```sh
python3 tools/diagnostics/capture_stationary_cost.py --output work/three-outfit-rig-physics/peony-stationary-cost-20261014
```

The wrapper invokes `tools/css_frame_profile.py` three times. Each request has
one second of warmup followed by 15 seconds of recording, with a 4,096-record
capacity. All three stopped for duration, not capacity, and reported no CSS
exception. The installed development core was
`css_core-development-54387cf5e27911f8-performance-end.dll`, SHA-256
`54387cf5e27911f83c64df97d8f7fb3d59c37d97e04024ee20cb0dcb09f5c8d1`.
PID 1944353, core selection and core hash remained unchanged.

The appearance was Peony on the Genessa shell, with the saved black/white floral
palette, existing body settings and trim glow. The wrapper recorded the full
customization values, idle override, player/controller/world identities,
location, actor and control rotations, and view target. These matched before
and after every capture. These are boundary checks, not per-frame movement
detection. No gameplay input, screen recording, appearance changes or settings
writes were sent by this capture. Full saved CSS JSON remained equal.

Captured console values also matched at every boundary:

| Setting | Value |
| --- | --- |
| `t.MaxFPS` | 999 |
| `r.VSync` | 0 |
| `r.ScreenPercentage` | 58.328 |
| `r.DynamicRes.OperationMode` | 0 |

## Measurements

All durations are milliseconds. Percentiles 95 and 99 use nearest rank;
the median uses the middle value or mean of the middle pair.

| Capture | Records | Tick interval median | p95 | p99 | Maximum |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 939 | 13.1189 | 35.5984 | 55.2954 | 81.1355 |
| 2 | 1,154 | 12.22515 | 18.8200 | 33.7227 | 56.0327 |
| 3 | 1,040 | 12.3839 | 26.4058 | 33.6786 | 51.5337 |

| Capture | CSS tick mean | Median | p95 | p99 | Maximum |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 0.281532 | 0.17350 | 0.6847 | 2.5927 | 5.8158 |
| 2 | 0.224672 | 0.15305 | 0.4909 | 1.5142 | 4.6700 |
| 3 | 0.225926 | 0.15740 | 0.4941 | 1.6216 | 3.3750 |

The spread between the three interval medians is 0.89375 ms. Tail timings vary
substantially. Desktop and background workloads were not isolated. Do not claim
an improvement over the October 12 sample, which used a different context.

`native/src/frame_profile.hpp` defines the measurements:

- `interval_ms` is wall time between successive CSS callback beginnings.
- `engine_ms` is the supplied engine delta multiplied by 1,000, not measured
  engine CPU time.
- `core_ms` and phase values are elapsed wall durations inside the CSS callback.
  They can include thread preemption and are not exclusive CPU measurements.
- A row's interval precedes its core execution. A same-row core spike does not
  explain the interval preceding it.
- Animation graph/cloth worker execution, other CSSX hooks, GPU work and display
  presentation are not isolated by these timers. The results cannot establish
  the cost of the accepted body, belt or skirt rigs.

## Trace capability check

Epic's [Unreal Insights reference for 5.6](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-reference-in-unreal-engine-5?application_version=5.6)
documents trace channels and runtime trace controls. The locally installed engine
source is 5.6.1 (CL 44394996, compatible CL 43139311). Its
`Runtime/Core/Private/ProfilingDebugging/TraceAuxiliary.cpp` registers the trace
console commands under `UE_TRACE_ENABLED`. The
`Plugins/TraceUtilities/Source/TraceUtilities/Private/TraceUtilLibrary.cpp`
implementation exposes channel enumeration and trace connection state.

Read-only calls on the game's reflected `TraceUtilLibrary` returned:

| Call | Result |
| --- | --- |
| `IsTracing` | false |
| `GetAllChannels` | empty array |
| `GetEnabledChannels` | empty array |

The object exists, but this interface exposes no channels in the running game.
This is consistent with tracing being compiled out, but does not prove the
game's compile flags or rule out other profiling methods. No trace start, stop,
channel toggle or trace-file write was attempted. Detailed rig/cloth timers
through this interface are therefore not available for this capture.

## Evidence and next comparison

Evidence is under
`work/three-outfit-rig-physics/peony-stationary-cost-20261014/`, through the work
symlink to `/run/media/eins0fx/theark/CSS/CustomShellSystem-work/`:

- `capture-1.json` through `capture-3.json`: raw rows, summaries and sampled
  runtime context, with command and stdout files.
- `result.json`, `state-before.json`: boundary comparisons and preservation.
- `trace-availability.json`: exact read-only requests and responses.
- `recipe.py`, `frame-profile-recipe.py`: the versions actually executed.
- `verify.py`, `verified.json`, `SHA256SUMS`: independent archive verification
  and checksums. Re-run `verify.py` offline to check all metrics and contexts.

The reusable scripts were subsequently clarified to name tick intervals
accurately, explain timing scope and fail the wrapper's process exit status if
final preservation checks fail. The executed recipes remain unmodified.
The profiler's four unit tests pass, including a separate p99/maximum spike
fixture. No new core or cooked assets were installed for this check.

The directory occupies about 1.5 MiB. Root and theark report 91 GiB and 66 GiB
available respectively; no large intermediate or duplicate build was created.

The release performance gate stays open. It still needs matched comparisons,
movement and transitions, and 30/60/higher frame-rate coverage. Any temporary
cap must restore both value and console priority. Local `UEngine::SetMaxFPS`
preserves the existing priority except constructor-to-scalability promotion;
`UGameUserSettings::ApplyNonResolutionSettings` also changes unrelated graphics
and audio settings. Neither path was invoked here. Do not blindly use a console
assignment or apply all settings to obtain one benchmark cap.
