# Diagnostic frame profiling

`CCS_FRAME_PROFILE` is OFF by default. Normal builds do not instantiate the profiler, sample timestamps or start its worker. `ccs.py build` explicitly resets the option to OFF so a cached diagnostic setting cannot silently enable it.

Build a separate diagnostic candidate from the repository root:

```sh
python3 CCS/tools/ccs.py build-profile --menu
```

This creates `CCS/build/windows-menu-profile`, with both the experimental menu and profiling enabled. `build-profile` without `--menu` creates a passive profiling candidate; `--probe` or `--registry` selects the corresponding discovery candidate. Menu and probe variants remain separate. These commands build only. They do not install or reload anything.

In an installed diagnostic build, F7 starts a 30-second capture in both loader and core. Its existing menu/probe action also runs, so cold opening/initial capture costs are included. Pressing F7 during a capture does not restart the timer. A full capture requires actual tick callbacks; it does not finish just because the game is paused or no longer ticking. Each capture stops after 16,384 frames or 30 seconds observed by callbacks. The limit and stop reason appear in the report.

The loader records the elapsed callback time, gate wait, start-to-start tick interval and supplied engine delta. The core records its elapsed tick and context resolution, menu, loaded discovery or explicit probe phases. Unused phases have zero samples, rather than a misleading zero-duration performance result. Measured phases with zero duration remain valid samples. Exception paths retain elapsed phase costs and mark the frame failed. The first callback has no interval sample, and invalid engine deltas are excluded from delta statistics.

Sampling uses `std::chrono::steady_clock`, fixed preallocated frame storage and two capture buffers. Active frame sampling makes no allocations or disk calls. Arming and completed-buffer handoff use `try_lock`; a busy handoff retries on a later callback. A refused arm leaves the existing capture intact. Producer access is limited to the loader's serialized game-thread callback; the worker only owns completed buffers. Shutdown joins the worker and drains completed or partially collected captures without engine calls.

The worker sorts samples, calculates arithmetic mean and nearest-rank p95/p99, records minimum/maximum, selects the 16 largest tick intervals and writes raw CSV plus JSON under `Mods/CCS/runtime/timing`. Mean is the arithmetic mean; nearest rank applies to percentiles only. Each report has source and monotonic start/finish timestamps. The JSON file is written after its CSV, so use JSON presence and matching CSV as the completed-artifact check. Output failures are counted; core status exposes active/completed/failed counts. The diagnostics are compiled separately and still require measurement of their own overhead.

A start-to-start interval includes the previous callback's CCS work. The report associates each large interval with both the preceding and current callback costs and phase values. Raw rows preserve their start timestamps, failure state and measured-phase mask. This supports correlating loader/core reports, rather than assigning a long interval to whichever phase happened in the current callback. Elapsed wall time includes descheduling and waiting; it is not a CPU-cycle count or GPU presentation measurement.

## Live comparison still required

Record the actual installed DLL hashes, game/content identity, UE4SS pin, graphics/FPS settings and enabled CSS/CSSX variants with each run. Keep route, combat actions, weapon/shell, enemies and menu scenario comparable. Separate cold opening, warm navigation, ordinary combat, weapon/shell changes, death and travel. Repeat each scenario with CCS absent/disabled, passive, and the enabled candidate while CSS/CSSX coexist. Use the same sampling method for compared runs, measure its overhead separately, and retain raw samples. A diagnostic passive capture measures loader/core dispatch overhead, not a full CCS-absent baseline.

Report sample counts, mean, p95, p99, maximum, failed frames and long-interval phase costs. Engine-tick cadence alone cannot establish displayed FPS or GPU behavior. If supported by the shipped build, a separate Unreal Insights or presentation capture can provide that evidence. No live CCS profiling result has been obtained yet, and no no-lag/FPS claim is justified by these tests.

## Verification

Host tests cover known distributions and percentile boundaries, skipped/zero-duration phases, first-interval exclusion, invalid engine delta, preceding-frame hitch association, frame capacity, duration, partial shutdown drain and background output failure. Build-option tests verify normal builds explicitly reset diagnostic options and reject combined menu/probe builds. Windows cross-compilation verifies the diagnostic loader/core integration; it does not execute engine callbacks.

References: [CSS phase captures](../../native/src/frame_profile.hpp), [CSSX cadence statistics](../../extensions/core/src/runtime/frame_stats.hpp), [Microsoft interval timing](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter), and [Epic Timing Insights](https://dev.epicgames.com/documentation/unreal-engine/timing-insights-in-unreal-engine). The Microsoft page describes the platform timer rather than this C++ implementation. The Epic page describes profiling concepts, not proof that this shipped UE 5.6.1 build exposes every trace channel.

## Existing CSSX live reference

A fresh `frame.stats` request returned 577 current samples from the installed CSSX core. Its measured core mean was 75.44 us, p99 566.8 us and maximum 1,260.9 us. The menu phase mean was 2.95 us. The reported engine-tick rate was 57.69 Hz, with zero intervals above CSSX's twice-median hitch threshold in that window. These are observations of the installed reference, not CCS results or presentation FPS.

Raw response, fresh request ID, installed CSSX loader/core hashes, game PID and CCS selector hash are retained in [the reference artifact](../work/frame-profile/cssx-live-reference.json). Scene/settings were not controlled, and a diagnostic compilation completed shortly beforehand. This reading cannot establish combat, travel or cold-menu performance. The new CCS candidates remain uninstalled and unprofiled. Nine host suites pass normally and under ASan/UBSan; 17 Python tests pass. Four normal/probe/menu variants and both menu/registry diagnostic variants cross-compile.
