# Performance investigation: evidence index, method, status

Status line (update this first): **residual FPS loss is unresolved.** The one
proven defect is fixed in the legacy host; the new framework is built to avoid
it by construction. First live numbers (2026-09-21, below) show the new core's
per-frame work is 0.27 ms mean and that switching every per-frame function off
in-process does not change the frame rate, so whatever the user sees is not
work inside CSSX's callbacks. The controlled restart pair (CSSX absent versus
present, same beacon, MangoHud frame-time log) is the next step and the only
thing that can attribute the remaining gap.

## Raw evidence index (all under `CustomShellSystem/work/`)

| Artifact | What it is | Scope |
| --- | --- | --- |
| `cssx-fps-counter-baseline-01/02.json` | 27.7 / 28.5 Hz engine cadence, legacy CSSX dev DLL + Cheat Menu loaded, menus closed, cheats off | Same save, uncontrolled camera |
| `cssx-disabled-comparison/counter-01/02.json` | 77.3 / 70.1 Hz, same CSS core, CSSX absent (no cssx.json) | Same save, camera not pinned |
| `cssx-disabled-comparison/perf.txt` | perf userspace sample, CSSX absent: top GameThread object-scan hotspot gone | 199 Hz sampling |
| `cssx-fps-perf-01.txt` / `.data` | perf sample with CSSX loaded: 12.8% GameThread in one UE4SS.dll address | Caller attribution missing |
| `cssx-fps-hotspot-disassembly.txt` | UE4SS RVA 0x3e2d40 loop: class/name compare over superclass chain (object iteration). The nearest export label `UnregisterHook` is not the function | Disassembly only |
| `cssx-framework-only/counter-01.json` | 26.4 Hz with legacy CSSX loaded and **zero extensions** | Minimal reproduction |
| `cssx-frame-demand-live/profile-01.json` | 523 frames after the needs_frame fix: 52.2 Hz, CSS core 0.515 ms mean, CSSX tick 0.052 ms mean, HUD 0 ms | Camera changed vs the disabled run |
| `cssx-fps-baseline/crash/` | Minidump + lldb unwind: null call in `FAnimSync::TickAssetPlayerInstances` after a CSS core hot reload while profiling | Unresolved, CSS animation lifetime |
| `cssx-fps-release-inventory.json` | Hashes of archived 0.3.0/0.3.1 CSSX and Cheat Menu ZIP binaries and the mapped dev DLLs | Which binaries were tested |

Source for the numbers: `docs/development/cssx-fps-investigation.md`.

## Proven cause P1 (legacy host, fixed there)

`Core::tick` called `HudService::update` every frame whenever the CSSX runtime
existed; `resolve_live_hud` called `UObjectGlobals::FindFirstOf("WBP_Player_HUD_C")`
per frame, a linear scan of the global object array with class-chain compares.
Zero-extension reproduction (26 Hz) plus the matching perf hotspot plus its
disappearance when CSSX is absent establish it. 1/26 s − 1/75 s ≈ 25 ms per
frame, consistent with a full object-array walk under Proton.

The new framework never resolves the HUD by class search per frame. The HUD
service resolves once per world generation through the player controller's
`User Interface Handler Component -> WBP_Player_HUD` (a property read), caches a
weak handle, and only re-resolves when the handle dies.

## Open hypotheses (not causes until measured)

- **H1, residual after the fix.** 49–52 Hz after the fix vs 70–77 Hz without
  CSSX was not a controlled pair (camera changed). Unknown residual size.
- **H2, global script-function hook.** The legacy hook service installs one
  `RegisterProcessLocalScriptFunctionPostCallback` as soon as any rule targets a
  Blueprint function (No cooldown, Perfect parry/block/harden, Genessa clones).
  Every Blueprint call in the game then takes a mutex and a map lookup. Cheats
  off ⇒ no hook, so this cannot explain the cheats-off report; it can explain
  drops with combat cheats on. The new service counts dispatches and time.
- **H3, JSON bridge volume with cheats on.** `combat_sync` decodes the player's
  full ability list (110+ instances, nested structs) each sync, and the legacy
  `handle()` did a linear scan of up to 8192 tracked objects per handle. O(n²)
  per sync. Only with combat cheats applied. The new bridge keeps a reverse
  index (pointer → id) so handle creation is O(log n).
- **H4, ABI-1 era slowdown reported by the user.** No archived measurement
  exists. Candidates: H2/H3 with cheats on, or something in CSS 0.3.x itself
  that was attributed to CSSX. Reproducing needs the archived CSS 0.3.1 + CSSX
  0.3.1 pair on a separate install; that replaces the user's alpha and is not
  authorised. Stays open unless the user asks for that test.
- **H5, per-frame framework work in the new design.** Must be shown ≈ 0 with
  the menu closed: one atomic flag read, one ring-buffer store, one zero-wait
  change-notification check, and a 30 Hz two-key chord poll only when a player
  controller exists.

## Method for the release comparison

Same save, same beacon, same camera (the driver pins control rotation through
`SetControlRotation` after load), same graphics settings, VSync off, no input
during capture, 10 s warm-up, 20 s capture, three pairs per row. Each row is a
normal restart. Numbers come from the loader's frame-interval ring
(`frame.stats`), which measures the interval between consecutive engine ticks
on the game thread, not GPU presentation.

Noise floor: computed from three back-to-back captures of the *same*
configuration in one process (median-of-medians spread). Acceptance budget for
"closed menu, no extensions requesting frames": median frame time within the
noise floor of the baseline, p99 within 1.5x baseline p99, zero framework-
attributed hitches. Enabled features declare their own budget per feature
(Cheat Menu: each periodic cheat ≤ 0.1 ms/tick at 10 Hz; combat hooks reported
per call).

| Row | Configuration | Isolates |
| --- | --- | --- |
| A | Game + pinned UE4SS, CSS and CSSX disabled | loader baseline |
| B | A + CSS alpha + Eve, CSSX absent | coexistence baseline |
| C | A + CSSX, no extensions, menu never opened | core and global hook cost |
| D | C + Cheat Menu, cheats off, menu closed | registration/polling cost |
| E | D, menu opened, navigated, closed | widget/input/cleanup cost |
| F | D + selected cheats enabled | feature cost and restoration |
| G | B + CSSX + Cheat Menu | combined contention |

Results table lives below and is empty until the live window happens.

## Results

### 2026-09-21 first live session (standalone CSSX dev core, CSS alpha present)

Same save, player standing at a beacon, no input, Player Menu closed, Cheat
Menu loaded with every cheat off. Source: `frame.stats` (loader ring) through
`tools/fps_probe.py`; raw files in `work/perf/D-*.json`.

| Capture | Frames | Median ms | p95 ms | p99 ms | Hz | Hitches |
| --- | --- | --- | --- | --- | --- | --- |
| D-closed-1 (right after load) | 812 | 24.59 | 29.08 | 34.58 | 40.6 | 0 |
| D-closed-2 | 926 | 21.24 | 26.68 | 31.18 | 46.3 | 0 |
| D-idle-1 (core tick disabled) | 930 | 21.49 | 25.03 | 29.60 | 46.5 | 0 |
| D-closed-3 | 959 | 20.35 | 24.97 | 30.85 | 47.9 | 1 |
| D-idle-2 (core tick disabled) | 990 | 19.86 | 23.78 | 29.40 | 49.5 | 0 |

Per-phase cost inside the core (869 frames): core tick mean 0.273 ms, p99
5.3 ms, max 13.1 ms; extension ticks mean 0.037 ms, max 0.73 ms; HUD and menu
0 (not active). The p99/max spikes were the once-per-second status write
(now every 5 s without read-back). Hook service: 0 rules, no script hook.

Reading: active versus idle differ by 0.6 ms with a 4.2 ms spread across
repeats (`fps_probe.py compare D-closed D-idle` → inconclusive/within noise),
and the scene itself warmed from 40.6 to 49.5 Hz over the five captures. So
H5 is measured and small. H1 remains: 46–50 Hz here versus 70–77 Hz in the
Sept 20 CSSX-absent run is not a controlled pair (different session, camera
not pinned, no MangoHud). The CSS alpha production core does not answer the
old `css_probe` counter, so rows A/B need an external logger: MangoHud
(installed, 0.8.4) with `work/perf/mangohud.conf`, started and stopped from
outside through `mangohudctl`, identical for every row.
