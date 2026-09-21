# CSSX FPS regression investigation

Started 2026-09-20. The historical checkpoints below retain their original scope.

2026-09-21 update: the user still reports an FPS drop in the builds they tried
("its still has fps drop but not worse"). Performance remains unresolved. The
HUD demand fix is not full performance acceptance. A separate agent is being
assigned standalone CSSX v1 through
[the execution handoff](../cssx/standalone-v1-agent-prompt.md). CSSX remains
disabled in the working CSS alpha; this feedback does not identify a newly
loaded module or a controlled residual frame-time delta.

Current checkpoint: the reproduced idle HUD preparation defect is fixed and installed, with both extensions restored. See the final validation section below. The older ABI-1 report and animation reload crash are not closed by this result.

## Report and scope

The user reports a serious FPS regression possibly caused by CSSX. A player reports about 22 FPS and 35% GPU utilization after installing the download, with performance restored by removing it and cheats still usable. The affected download/version and reproduction state are not yet known. This report is evidence of a regression to reproduce, not proof of a particular CPU/GPU bottleneck or guilty module.

The user's machine currently has `cssx_core-dev-6cbebbbb517b90e9.dll` selected and Cheat Menu manifest version 0.3.1, entry `cheat_menu-77da617e62fc7d71.dll`. The CSS loader status selects `css_core-215cf8c690596258-1789835809917158647.dll`. These are local installed files, not the affected player's confirmed versions. This inventory was captured before testing; later live results and deployment changes are recorded below.

## Feedback loop under construction

Measure actual engine frame intervals and time attributable to the shared CSS tick, CSSX tick, HUD preparation and extension render. Then compare identical idle scenes with only the suspected component changed. Keep menu state, camera, cheats, graphics settings and frame cap fixed. A frame-time regression must improve in this loop before calling the issue fixed.

The existing `tests/live_material_recovery_perf.py` measures cosmetic recovery latency. It does not catch sustained FPS loss and is not sufficient for this report. The loader already passes real `EngineTickPost` delta to the core each frame, offering a bounded instrumentation point without screen capture or new global hooks. Existing dev commands can export measurements through the runtime request protocol. The initial instrumented baseline failed; the restarted original-core counter measurements below now provide a live baseline.

## Prepared frame probe

Developer-only `frame_profile` now records up to 4096 frames over a requested 3 to 30 seconds after one second of warmup. It measures engine delta, wall-clock interval between EngineTickPost callbacks, total CSS core time, and separate recovery/CSSX tick/HUD preparation/CSSX render/Inventory phases. It allocates its buffer before the warmup and writes the result after measurement ends. There are no new global hooks. A production build without `CSS_INVENTORY_DEV` excludes this instrumentation.

`tools/css_frame_profile.py --seconds 10 --output work/cssx-fps-baseline.json` verifies that the loaded DLL matches `build/windows/css_core.dll` before sending a request. It does not deploy or change input/focus. It retains raw results and computes mean, median, p95 and maximum timings, checks phase sums, detects game/core replacement and reports whether saved CSS state changed. `--review <saved-report>` performs offline analysis without contacting the game. Engine tick cadence is not GPU/display presentation timing; CSSX hooks executing elsewhere in the frame are outside the nested phase timers.

Verification: Windows developer build exits zero (`work/cssx-frame-profile-build.log`). `build/host/css_frame_profile_tests` passes warmup exclusion, slow-frame timing, exception recording, overlap rejection, reset and capacity bounds. The summary correctly computes 22 Hz from synthetic 45.45 ms intervals and rejects impossible phase totals. These are instrumentation tests, **not a live reproduction or a fix**. The first host build attempt required regenerating CMake before the new target was available; configuration and retry pass.

The user has been asked for the affected CSSX version/reproduction state and a new ten-minute live window to reload the prepared timing build, measure a safe idle scene and restore the current core. The old 30-minute window has expired. While that request is pending, no installation changes, live requests or gameplay input are authorized by that new window.

Next: capture a baseline, establish the smallest comparison that reproduces the FPS loss, then rank and test causes. Code inspection for measurement placement does not establish a cause. Preserve current core/config and verify restoration around any live trial. Next-Gen physics remains paused.

## Release comparison before live testing

Read-only process mappings confirm that the game maps the selected development CSSX DLL and the Cheat Menu DLL named above. This is stronger evidence than a selector file alone. `work/cssx-fps-release-inventory.json` records hashes from all six saved CSSX-related ZIPs and the installed entry files without extracting or changing the game installation.

| Artifact | SHA-256 |
| --- | --- |
| Mapped development CSSX DLL | `6cbebbbb517b90e923009cd12fce471938c6c8f634ab704c6d4d9148b2fe2290` |
| CSSX DLL inside saved 0.3.1 ZIP | `357003518afcbc0289e8cb06600ed9df026f5607da07df143a9479a36f7a57aa` |
| CSSX DLL inside saved 0.3.0 ZIP | `b4ae595b4286f124eb4f107123e571b10ad1f8eb6fbe51bfbc643a64cd3281f5` |
| Mapped Cheat Menu and saved 0.3.1 Cheat Menu DLL | `77da617e62fc7d7145f0f4820df36d63790ac5b602f102caa731d15f6cd24bff` |

The saved framework ZIP's `cssx-release.json` declares version 0.3.1, ABI 1 and source commit `8ab223a626468a3d070b7f5b101b6023d6457435`. That commit's header defines ABI 1. The current header defines ABI 2, introduced in `7cb9737`, while the current local 0.3.1 changelog describes ABI 2 and CSS 0.4.2. This establishes a local archive/documentation mismatch, not what the reporting player downloaded or the binary ABI of an unexamined remote release. An attempt to open the GitHub release page failed, so its current contents remain unverified.

The local changelog's absolute zero-frame-rate-overhead and zero-allocation claims had no matching performance evidence. They are removed from the local draft pending measurement; nothing has been published. Current runtime behavior remains unchanged.

A future comparison must identify the reporting player's actual package and pair it with a compatible CSS host. Do not replace the running ABI-2 runtime with the old archive solely because both use the version label 0.3.1. The prepared probe measures the currently installed development setup first; it cannot by itself close a report against another build.

## Waiting for a current live window

Superseded by the authorization and crash checkpoint below.

The live-window request remains unanswered across three goal turns, including the original FPS-priority turn. Offline instrumentation, its checks and the installed/release inventory are complete. A fresh read-only readiness check confirms the original core is still selected and mapped, the prepared probe is not installed, and the rollback DLL exists. `work/cssx-fps-readiness.json` records the hashes.

The goal is temporarily blocked on that live window, not complete. Resume by applying the user's answer, rechecking the current installation, preserving the current core selector, capturing a ten-second safe-idle profile and restoring the original core. Do not launch, hot-reload or issue runtime requests based only on an automatic continuation. If the user chooses offline-only work, agree on a reproduction artifact or revised priority before resuming Next-Gen physics.

## Authorized profiling attempt and animation crash

The user authorized the prepared ten-minute test window at 17:25:28 UTC on 2026-09-19 (01:25:28 local, September 20), with the game already open. Preflight live requests confirmed an active player, closed Inventory/CSSX menu, both extensions available, and no unapplied Cheat Menu edits. The sampled cheat toggles were off. The existing Feminine locomotion preference was retained.

The original core selector and all CSS state files were copied into `work/cssx-fps-baseline/`. The prepared DLL was loaded as `css_core-profile-c57179c52d94170a-1789838792759485841.dll`. The game then crashed during the capture, before `runtime/frame-profile.json` was produced. The player also confirmed the crash. No valid FPS result exists, and no FPS cause or fix is established.

Crash `UECC-Windows-F86B312A4EFE8EAF8CB4CEB6D9AA6F08_0000` is timestamped 17:26:55 UTC. Its error is a null-address access violation, not the GPU-crash classification found in the two older local reports. There is no Linux coredump for PID 2267051 and no matching OOM/driver error in the checked kernel-log interval. The UE crash folder, including its minidump and bundled save backups, is preserved under `work/cssx-fps-baseline/crash/`.

LLDB loads the Windows minidump directly. Adding the exact game executable at its recorded load address permits symbolized unwinding. `lldb-game-unwind.txt` shows:

- Crashed thread: Windows TID 476, Foreground Worker #1.
- Instruction pointer zero, with a return address in `UE::Anim::FAnimSync::TickAssetPlayerInstances`, `AnimSync.cpp:379`.
- The caller is `FAnimInstanceProxy::UpdateAnimation`, followed by skeletal-mesh parallel animation evaluation and task execution.
- The game thread is waiting for animation/tick tasks in `UWorld::Tick`, rather than executing the frame-profile callback at the captured instant.

The disassembly at return RVA `0x3a92e72` shows an indirect call through a virtual table at offset `0x2c8`. Pinned UE source line 379 calls `AssetPlayerToTick.SourceAsset->GetUniqueMarkerNames()`. This localizes the fault to animation-asset access; it does not identify which asset was invalid or prove whether the reload, another mod or another lifetime defect caused it. Optimized debugger local values are not reliable object identities. The small dump does not contain the pointed-to asset memory. Do not claim the profiler is exonerated merely because it is absent from the worker stack.

Rollback audit: `recovery-audit.json` verifies that the original core selector is restored and every saved CSS state JSON is byte-identical to the preflight copy. The game exited before a live rollback acknowledgment could arrive, so the scope is **original core selected for the next launch**, not a completed live reload. The profiling wrapper now preserves its lifecycle/error report even if the game exits during rollback. It must not mask the primary failure with an acknowledgment timeout.

Next: investigate the animation asset lifetime across restoration/reapplication and the retained Feminine locomotion override. Avoid another reload of the profiling build until that failure is understood. A later controlled cold-start comparison may separate startup from reload behavior; it is not yet performed. The user's ten-minute window ends at 17:35:28 UTC. No further game launch or state change has been made after the crash.

## FPS fallback through the existing core

`tools/css_frame_counter.py --seconds 10 --output work/cssx-fps-counter-baseline.json` now provides a baseline path that requires no profiling DLL or hot reload. The game’s existing CXX dump declares `UKismetSystemLibrary::GetFrameCount()` with an int64 return and no input parameters. Pinned engine source confirms it returns `GFrameCounter`. The driver uses the existing CSS `css_probe` reflection bridge, so CSSX itself need not be loaded for this measurement.

The driver samples the engine counter about once per second, records request send/receive times and calculates frame cadence with bounds for request latency. It detects process/core replacement, retains partial evidence on failure and performs no gameplay input, asset changes or preference writes. This does not attribute CPU time to individual CSSX phases, measure GPU utilization or prove display presentation rate.

Synthetic 22 Hz arithmetic and counter-reset/overlapping-request rejection pass. The restarted-game samples below now validate the fallback against the live engine. On a later authorized run, begin with the original core and this reader; do not repeat the failed profiling reload merely to get an FPS number. The animation crash remains an open issue with a captured, symbolized stack.

## Restarted session and CSSX removal comparison

The user restarted the game and confirmed they were in gameplay. Two ten-second frame-counter captures using the original core measured **27.657 Hz** (latency bounds 27.154 to 28.179) and **28.456 Hz** (27.931 to 29.000). Artifacts: `work/cssx-fps-counter-baseline-01.json` and `-02.json`. This is engine cadence, not a GPU presentation measurement. The restarted process maps the original CSS core, the development CSSX DLL and Cheat Menu DLL. Menus are closed, sampled cheat toggles are off and CSSX hook status has no rules. Saved settings specify 999 FPS foreground/menu limits, 60 FPS background and VSync off. Settings on disk alone do not prove effective live limits.

The user clarified that they personally observed good performance before CSSX existed and FPS loss already with ABI 1, before the HUD feature. Treat CSSX as the primary regression suspect. ABI 2 HUD work cannot explain the origin of that earlier regression. The player's exact downloaded package is still useful for release verification but is not a reason to delay isolation of this machine's CSSX regression.

Ranked initial predictions presented during the live investigation: global interception cost would concentrate in dispatch hooks; host/reflection work would concentrate in engine lookups called by CSSX; extension polling would concentrate in tick callbacks and their host calls. The first ten-second userspace CPU sample succeeded without stopping the game or replacing modules. `work/cssx-fps-perf-01.data` and `.txt` show the largest individual sampled address in UE4SS on GameThread, plus class-super traversal. This requires caller attribution and does not by itself blame UE4SS independently of CSSX. The perf binary and its libpfm dependency were extracted into `work/perf-tool/` from the configured Arch mirror, without system installation or settings changes. Build-ID cache writes were disabled.

At the user's explicit request, proceed with the direct CSSX-disabled comparison. The runtime has no independent live-unload command, and another core hot reload is inappropriate after the animation crash. A reflected, signature-verified Kismet `QuitGame` request closed the game normally. `cssx.json` was backed up into `work/cssx-disabled-comparison/` and removed only after the process exited. The fallback `cores/cssx_core.dll` is absent, so the next start skips CSSX and all its extensions. Core selection and appearance state were not edited. Steam was asked to restart app 2584270. Await the same save/view, verify no CSSX/extension DLLs are mapped, then repeat the counter capture. Do not compare title-screen cadence with gameplay. The selector backup is retained for an exact restoration or subsequent fixed-build trial.


CSSX-disabled result: process 2587739 maps the original CSS core and no CSSX/Cheat Menu DLL. The player probe confirms the same Corrupted Genessa character and persistent world. Two ten-second samples report **77.315 Hz** (75.484 to 79.238) and **70.128 Hz** (68.377 to 71.970). The second sample overlaps an external 199 Hz userspace CPU capture. The earlier large GameThread lookup hotspot disappears from that capture. The scene/view was not mechanically pinned, so do not advertise a precise speedup percentage as a general benchmark. The large repeatable improvement does validate the user's CSSX regression observation on this installation.

Next isolation stage started with a normal game quit, preserving the same CSS core: restored the exact CSSX selector, moved the entire `extensions` folder to `work/cssx-framework-only/extensions`, then requested a Steam start. This loads the framework with zero extensions. Extension files and preferences are preserved. On restoration, close the game, remove only the empty auto-created extensions directory, and move the preserved folder back. Do not leave extensions silently missing at the end of testing.

Framework-only preflight confirms both original CSS and CSSX mapped, zero extension entries, zero discovery errors. Player and controller remain null at the main menu at the preflight check. The user has been asked to load the same save; no title-screen FPS is being substituted for gameplay.

The user subsequently loaded the save and observed FPS below 30 with the framework alone. Capture started through the existing CSS frame counter.


Framework-only gameplay measured **26.408 Hz** (25.927 to 26.906), agreeing with the user's below-30 observation. Artifact `work/cssx-framework-only/counter-01.json`. This is the minimized reproduction: neither Cheat Menu nor UI Kit is necessary for the slowdown.

A concrete idle-work defect is present in the current host: `Core::tick` calls `HudService::update` every frame whenever the CSSX runtime exists, even with zero extensions. `resolve_live_hud` calls `FindFirstOf` on each such frame. The CPU hotspot disassembly at UE4SS RVA 0x3e2d40 traverses object classes/names and superclass chains, with `IsValidObjectForFindXOf` calls, consistent with that search. The disassembler's nearest exported label happens to be `UnregisterHook`; it is not a trustworthy function name for this internal callback. Do not label this a hook-unregistration hotspot.

The candidate introduces an optional size-checked ABI-2 runtime tail query, `needs_frame`. The query directly checks live render consumers without JSON or allocation. The CSS host prepares and dispatches a HUD frame only when needed. Older ABI-2 runtime tables remain accepted and use their previous unconditional behavior. ABI-1 extensions remain supported and do not request HUD frames; this is distinct from supporting an ABI-1 runtime in the ABI-2 host. Runtime tests cover zero extensions, Lua-only entries, native ABI 1, native ABI 2 without a render callback, a live renderer, render-failure suspension and stop. Existing cheat and extension tests pass. The old host test library fails the new query-presence check; the rebuilt library passes. Windows core/runtime compilation succeeds. Live candidate validation remains pending.

This candidate targets the reproduced current-build defect. The user's observation that an earlier ABI-1 setup was also slow remains recorded and must not be explained retroactively by this newer HUD path without testing that older host/runtime combination.


## Candidate validation and current deployment

Installed through a normal quit/start, with both original extension folders restored:

- CSS core: `css_core-frame-demand-8183ded19bd89cdc.dll`, SHA-256 `8183ded19bd89cdc2f0f4a1a5c4d917e98c4ec984db9edb60954d15dcfb1c9b4`.
- CSSX: `cssx_core-frame-demand-1a38a86ea28d42a8.dll`, SHA-256 `1a38a86ea28d42a8860d2fce5bc4f7fdb2e9cdce7381fc3371428bc7e7c5780d`.
- Cheat Menu entry remains the original `cheat_menu-77da617e62fc7d71.dll`; UI Kit is also restored and available.

The first startup request timed out during loading. A later fresh player request succeeded, and the candidate was verified in gameplay. No crash occurred in this cold-start capture. `work/cssx-frame-demand-live/profile-01.json` contains 523 measured frames: 52.189 engine Hz, mean total CSS core 0.515 ms, mean CSSX tick 0.0518 ms, **HUD preparation and render both zero for every measured frame**. No failed core frames were reported. `counter-01.json` independently measures 48.983 Hz with bounds 48.103 to 49.896.

The user confirmed they had moved or changed the camera view since the CSSX-disabled run. Therefore 49 to 52 versus 70 to 77 is not a controlled residual-penalty measurement, and neither a precise speedup nor complete parity can be inferred from those rates. The current defect is established by the zero-extension reproduction, the object-search hotspot and the now-skipped idle work. Broader renderer-enabled and historical ABI-1 performance remain unverified.

`tools/cssx_cheat_check.py` passes live: a draft does not change gameplay; Discard works; Apply enables God; Disable all restores the original damage flag. Final audit confirms the player is damageable, God is off, there are no unapplied edits or active hook rules, and every saved CSS/cheat JSON hash matches its pre-deployment value. Module mappings confirm CSSX and Cheat Menu really are loaded. Evidence: `cheat-check.log`, `verified-state.json`, `loaded.json`, `final-audit.json` in the candidate evidence folder. No Blender assets, mesh proportions or outfit packages were changed.

The user also requested verification of DLL hot reload to avoid unnecessary restarts. [The reload guide](dll-reload.md) distinguishes existing core hot reload, menu-only reload and startup-only loader replacement. A CSSX-only lifecycle path is a follow-up; it is not yet implemented. The earlier animation-asset crash remains captured and unresolved.
