# Performance investigation: evidence index, method, status

Status line (update this first): **no measurable average cost; periodic
hitch source removed 2026-09-22.** The one proven legacy defect (P1) is
designed out. The controlled MangoHud pair (below) shows CSSX loaded versus
absent within noise on the median and better on p95/p99. The user still
reported "random frame drops" after that pair, and a code audit found a real
periodic stall that a median cannot show: the core wrote `runtime/status.json`
every 5 s **on the game thread with FlushFileBuffers plus a write-through
rename**, which under Proton is an fsync on the game thread while the game
streams from the same disk. That, and every log append, now goes through a
background writer thread; the game thread never touches the disk for
informational files. `frame.stats` now attributes every hitch frame to the
core's own time inside it, so the next live session can say per hitch whether
CSSX was involved (`tools/hitches.py`).

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

### 2026-09-22 controlled pair, MangoHud frame-time log (identical instrument for both rows)

Steam restarted with `MANGOHUD=1` and `MANGOHUD_CONFIGFILE=work/perf/mangohud.conf`
so every launch gets the same overlay and per-frame log (`log_interval=0`,
automatic 120 s window starting 120 s after launch). Player free-roaming
near the same beacon. Raw CSVs: `work/perf/B-css-only-1.csv`, `D-cssx-1.csv`.
The MangoHud control socket does not reach the game inside Steam's container
(`mangohudctl` returns 0 but nothing happens), so each launch yields exactly
one automatic window; further rows are one launch each.

| Row | Configuration | Frames | Median ms | p95 ms | p99 ms | Mean fps |
| --- | --- | --- | --- | --- | --- | --- |
| B | CSS alpha alone, CSSX `enabled.txt` absent | 5042 (4202 after 20 s) | 22.24 (22.19) | 30.20 | 39.04 | 42.0 |
| D | B + CSSX dev core + Cheat Menu + UI Kit (Lua), menu closed, cheats off | 5331 (4443 after 20 s) | 22.53 (22.77) | 27.82 | 38.11 | 44.4 (42.7) |

**Reading of the pair.** Median +0.3 ms (about 1%, inside the 4 ms spread
seen between repeats of one configuration), p95 and p99 slightly better with
CSSX, mean fps equal or higher. With this instrument, loading CSSX, the
Cheat Menu and the Lua UI Kit costs nothing measurable at this scene. The
earlier 70–77 Hz CSSX-absent figure (Sept 20) was a different scene/camera;
today's baseline without CSSX is 42 fps in the same play area.

**Where CSSX's own time went, and what changed (per-op request accounting,
`frame.stats.requests`):**

| Source | Before | After | Change |
| --- | --- | --- | --- |
| Cheat Menu readiness probe (`gameplay_ready`, ~13 requests every 250 ms) | 0.8–1.7 ms per tick, always | only when a cheat, power or reapply needs it | lazy |
| Shell catalog (`GetShellItemDefinition`, 10–13 ms per shell) | 10 calls in one tick on every pawn change (100+ ms hitch) | names at once; one token per tick, once per process, on-demand for matching | spread + cached |
| `class_default` scan (whole object array, 1–36 ms) | every call (weak pointer reported the CDO dead) | once per name, validated by object-array slot | cache fixed |
| Combat sync ability decode (`ActivatableAbilities`, 4.6 ms) with combat cheats on | every second | only when the list length changed or every 10 s; seal checks still every second | shallow `count` op |
| Status file write | every second with read-back (5 ms p99 spikes) | every 5 s, no read-back | |

Idle after the changes (cheats off, menu closed, 40 s): core tick 121 µs
mean / 384 µs p99 per frame, extension ticks 12 µs mean, menu 2 µs. Loader
ring in the same window: median 23.4 ms, p99 38.7 ms, 41.9 Hz.

### 2026-09-22 live results after the disk I/O change

Steady state while the user played (60 s, cheats off, menu closed, writer
thread on): 2642 frames, median 23.24 ms, p99 29.69 ms, max 37.4 ms, 44.0
fps, **0 hitch frames** (none above twice the median). Core share: 129 µs
mean, 2.1 ms worst frame. Raw: `work/live/audit/hitches-steady-1.json`.
Compare the earlier idle figure (core 121 µs) and the D row (p99 38 ms):
same average, and the periodic stalls are gone from the window.

Cheats on (God off; auto heal, infinite resolve, max shell points, no
cooldown, perfect parry on; 231 hook rules), 60 s of play: before the
ability-scan and image fixes, CSSX made six frames of 22 to 64 ms in the
window (forced 10 s ability re-scan, first menu build decoding two 1 MB
banners). After: 3016 frames, median 18.3 ms, p99 39.8 ms, 50 fps, CSSX
mean 135 µs, **worst CSSX frame 2.7 ms**, zero frames where CSSX exceeded a
quarter of the frame; the 44 hitches in that window (up to 179 ms) carry
under 0.4 ms of CSSX each. Raw: `work/live/audit/hitches-cheats-on-{1,2}.json`.

Two crashes found and fixed the same day by live bisect (loader alone ran;
core switched live between builds): a weak-reference cache for reflected
path lookups killed the game within a second (removed), and the bridge's
whole-struct write through a temporary copy freed the Movement struct's map
storage so the next read crashed (non-plain-data properties are now written
in place). Neither was a frame-rate issue; both were correctness.

### 2026-09-22 game-thread disk I/O audit (code evidence)

What ran on the game thread and could stall it, found by reading every write
path (`grep ofstream|FlushFileBuffers|MoveFileEx`):

| Path | Frequency | Stall mechanism | Fix |
| --- | --- | --- | --- |
| `Core::publish_status` → `atomic_json` | every 5 s | temp write, `FlushFileBuffers` (fsync), `MoveFileEx(WRITE_THROUGH)` | queued to `Writer` (background thread), no fsync |
| `Storage::log` (every log line, incl. "Hotkey:") | on events | two `stat` calls, append, flush | queued to `Writer` |
| loader `runtime/frames.json`, `loader.json` | 10 s (dev), on switch | fsync + write-through on the UE4SS update thread | `durable=false` |
| dev channel `response.json` | per request (dev only) | fsync | `durable=false` |
| settings save, extension `state.save` | explicit user action only | fsync, backup copy | kept: durability matters there and it is one hitch per Apply |

Also removed from the per-frame path:

- `find()` (StaticFindObject by path) was called for `/Script/InputCore.Key`
  and `Default__GameplayStatics` on every input poll and every player lookup.
  It now caches by path with weak validation (`engine.cpp`).
- The global Blueprint script hook (installed while a combat cheat is on)
  took a recursive mutex on **every Blueprint call in the game**. It now
  checks a lock-free snapshot of the hooked functions first; the mutex is
  taken only for calls that are ours (`hook_service.cpp`).

New instrument: `frame.stats.hitches` lists every frame above twice the
median in the window with the core's own microseconds inside that frame
(`worst`, `core_share_max_us`, `frames_where_core_exceeds_quarter`). The
Settings page shows the same numbers to the player ("Performance" row) so
the cost is visible without tools.

Cheat Menu correctness found in the same pass (both from the user's logs):
`no_cooldown` aborted on "Property is missing: StoneFormCooldown" because the
code compared lowercase text; and one failing toggle (perfect parry without
its seal) rejected the whole Apply. Apply is now per feature (D11) and seal
cheats arm instead of failing (D12).

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
