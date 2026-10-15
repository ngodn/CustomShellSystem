# Body simulation cost in the running game

Peony, Unholy Genessa and Commander White each complete four enabled/bypassed
pairs. The verified set contains 24 ten-second captures and 12,380 frames. Original appearance, saved settings, CINE, resources, equipment, camera
and body-enable state are restored. No production code or assets changed.

Evidence: `work/three-outfit-rig-physics/live-body-cost-20261015/`.
The installed core is
`dc0a609e74750815084130278354de76f754643326c8ec3829393d7439cead67`.

## What was measured

The existing live diagnostic changes only the transient post-process
`CSSBodyEnabled` input. The regional Off controls are not used as a bypass.
The body rig counter must stop while bypassed and advance while enabled.
Other public body inputs remain exact, and the other active rig counters keep
advancing. The native Eve idle, camera, equipment and graphics settings are
matched. Pair order alternates Off/On and On/Off, with two seconds of settling
before each capture.

These are whole-game engine-tick intervals, not display presentation times or
exclusive body-worker/GPU timings. The frame recorder's `engine_ms` column is
the engine-supplied delta, not engine CPU duration. Secondary-rig inspection
occurs before and after the timed capture. Background world and desktop work
remain present. Body movement itself can also affect rendering cost.

The retained GameUserSettings file matches each run's hash. The common context
uses `t.MaxFPS=999`, `r.VSync=0`, `r.ScreenPercentage=58.328`, and dynamic
resolution mode 0. No graphics setting or console priority is changed.

## Results

Values are the median within-pair change when enabling the body solver, in
milliseconds. A negative value does not mean simulation is free or speeds up
the game; it shows the variation in this whole-frame comparison.

| Appearance | Complete pairs | Frames | Median interval change | p95 change | p99 change |
| --- | ---: | ---: | ---: | ---: | ---: |
| Peony | 4 | 4,283 | +0.061 | +0.355 | +1.261 |
| Unholy Genessa | 4 | 4,267 | -0.082 | -0.475 | -0.320 |
| Commander White | 4 | 3,830 | +0.025 | +0.325 | +0.977 |

No body or other sampled secondary-rig reset occurs within these verified
capture boundaries. Peony and Genessa run around 53 engine ticks per second;
Commander White runs around 48 in this scene. These different outfits are not
an isolated hair, belt, cloth or GPU comparison. CW's strands and Belt20 remain
enabled in both conditions. Genessa's saved hair setting is disabled throughout.

Peony's individual median pair differences range from -0.123 to +0.200 ms;
its p99 differences range from -0.533 to +4.215 ms. Genessa's median differences
range from -0.137 to +0.075 ms. The small medians and variable tails do not
establish an exclusive solver cost or complete the overall performance gate.
Movement, transitions, facial variants, doubles and cloth/GPU cost remain outside
this measurement. Accepted solver limits and collision settings are unchanged.

## Interrupted runs and diagnostic corrections

`genessa-cw/result.json` remains failed. Its first three Genessa pairs pass;
the fourth On capture reaches the final guard after unchanged inputs, context
and released keys, then fails the focus check. Its unpaired capture is retained
and excluded from the comparison. Full restoration passes. `genessa-final-pair/`
repeats only pair 3, completes it and restores everything. The contexts,
GameUserSettings hashes and saved states match exactly across both runs.

`cw/result.json` retains a pre-measurement rig-name failure. CW uses
`CR_BodyB3`, while the old diagnostic selected only `CR_Body`. The public enable
value is restored to true, but that same lookup prevents automatic appearance
cleanup. `rig-failure-readback.json` confirms all three CW rigs enabled;
`recover.py` then restores the original appearance, idle, saved settings, CINE
and exact geometry. `recovery.json` passes. The source diagnostic now recognizes
both accepted body rigs and attempts guarded appearance restoration separately
from body-readback validation, so a failed measurement assertion cannot by itself
skip that cleanup.

`cw-bodyb3/result.json` retains the subsequent focus interruption, including the
actual VS Code window identity and traceback. Two pairs complete before the
third On capture fails its final focus check. Every restoration field passes.
The unpaired capture is retained and excluded; no successful pairs need rerunning.
After the user renewed the foreground testing window, `cw-final-pairs/`
completes only pairs 2 and 3, with 1,903 frames and full restoration. All runs
share the same context, graphics-file hash and original saved state.

## Verification

`tools/diagnostics/verify_live_body_cost.py` recomputes raw timing statistics,
checks actual enabled/bypassed counter behavior, preserved inputs, other rig
progress and resets, recorded writes, hashes and restoration. Its normal mode
requires the requested pair set to be complete. The explicit
`--completed-pairs-only` mode verifies only complete pairs from an interrupted
run, retains the failed parent identity in its output and never claims overall
performance acceptance.

Verified receipts are `peony-verified.json`, `genessa-prefix-verified.json`,
`genessa-final-verified.json`, `cw-prefix-verified.json` and
`cw-final-verified.json`. The original failed receipts remain intact.
`verify-combined.py` passes and writes `combined-verified.json`: all evidence
hashes match, each appearance has exactly pairs 0 through 3, and all restoration
checks pass. This closes the requested stationary comparison only.
