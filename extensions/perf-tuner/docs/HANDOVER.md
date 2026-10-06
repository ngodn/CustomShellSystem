# Perf Tuner: handover

Repository: `/home/eins0fx/development/mods/msII/CustomShellSystem`, branch `feature/cine-scene`.
The original work is committed in `30aab48` (CSSX) and `bbcf58f` (Perf Tuner).
The owner requested a separate commit for the Codex takeover changes on 2026-10-06.
CSSX 1.3.1 and Cheat Menu 1.1.2 are already committed and tagged locally.
Another agent works in the same checkout
(animation library port: `native/src/*`, `tests/animation_library_tests.cpp`,
`tools/authoring-probes/animations/*`). Commit only the paths listed in section 7. No git
worktrees, no `git stash`, no `git add -A`.

## 1. What it is

A CSSX extension for Mortal Shell II (Unreal Engine 5.6.1, DX12, modded through UE4SS).
Source: `extensions/perf-tuner/`. It loads as `perf_tuner.dll` through the CSSX core and
appears as a tab page "Perf Tuner" in the game's Player Menu.

CSSX is the extension framework in `extensions/core/` (loader `dlls/main.dll`, hot-swappable
core `core/cssx_core-<ver>.dll`, extensions under `Mods/CSSX/extensions/<id>/`). Extensions
talk to the engine only through a JSON bridge (`extensions/core/include/cssx/client.hpp`,
contract in `extensions/core/docs/abi.md`). Performance rules for anything inside CSSX:
`extensions/core/docs/extension-performance-standard.md`.

## 2. Goal

One goal: more frames per second with no visible loss of picture quality.

Hard constraints from the owner:
- Never change resolution, screen percentage, dynamic resolution, scalability groups
  (`sg.*`), the upscaler, frame caps, v-sync, mip/LOD biases, quality levels, Lumen/VSM
  feature switches, the VSM page pool, or Nanite detail. The catalog parser refuses these
  (`forbidden_reason()` in `src/catalog.cpp`); tests cover it.
- Owner clarification during takeover: temporary resolution changes are allowed only in
  a separately requested diagnostic. Automatic Tune must never run that diagnostic.
- Never write the player's `Engine.ini` or `GameUserSettings.ini`. Read only.
- Measure before trusting. Every tweak is read first, verified after writing, restored on
  stop or when turned off. A claim of gain needs the off/on/off/on trial to agree.
- Support Windows, Proton/Linux (NVIDIA, AMD, Intel) and Steam Deck: detect the machine,
  offer only what fits, scale memory-dependent values from VRAM.
- Take over any Engine.ini tweak pack the player already installed: judge every line,
  override what the tuner manages, flag picture-changing and dead lines, never edit the file.
- C++23, UE 5.6.1 best practices, no lag, no bugs, no fps drop, high performance, every case
  handled, no regression. The tuner must never cost frames itself: no per-frame work, no
  per-tick reflected calls unless an audit or a trial is running, cached handles.

## 3. Layout

```
extensions/perf-tuner/
  extension.json          manifest (id eins0fx.perf-tuner, api 3, entry perf_tuner.dll)
  tweaks.json             the catalog, shipped, player-editable
  README.txt              player-facing
  THIRD_PARTY_NOTICES.txt
  src/catalog.hpp/.cpp    catalog parsing + forbidden rules + Machine/fit rules + console_number
  src/trial.hpp/.cpp      pure off/on/off/on state machine and verdict maths
  src/inifile.hpp/.cpp    Engine.ini scanner, game-owned and picture-cvar lists
  src/machine.hpp/.cpp    detect_machine(): Wine via ntdll export, DXGI adapter/VRAM, SteamDeck env
  src/audit.hpp/.cpp      world audit over reflection + the one world fix
  src/tuner.hpp/.cpp      the extension: apply/restore, trials, probe, tune queue, findings, menu model
  src/main.cpp            ABI table only
  tests/tuner_tests.cpp   121 checks against a mock host (catalog, trial, bridge, ini, audit, tune, fit, cleanup)
  docs/DESIGN.md          evidence and rejected candidates
  docs/findings.md        numbers measured in the game
  docs/HANDOVER.md        this file
  docs/work-queue.md      takeover status and remaining live checks
  work/live/check.py      dev-channel driver: model, event, library, report, frame
  work/live/stat.py       runs a console command; `stat unit` + engine screenshot
  work/research/          game inis exported from pakchunk0, UE 5.6.1 cvar defaults, the UET pack
                          (ini/, uet/, GameDump-tool/bin|obj are git-ignored)
```

Build targets are declared in `extensions/core/CMakeLists.txt` (`cssx_perf_tuner_logic`,
`perf_tuner`, test `cssx_perf_tuner`). Staging flag added to `extensions/core/tools/cssx.py`:
`--perf-tuner`.

## 4. Commands

```
# host build + all tests (11 suites)
cmake -S extensions/core -B extensions/core/build/host && cmake --build extensions/core/build/host -j8
ctest --test-dir extensions/core/build/host

# Windows cross build (clang-cl + xwin), release and dev
cmake --build extensions/core/build/windows --target perf_tuner cssx_core -j8
python3 extensions/core/tools/cssx.py build --dev

# install into the game (ASK THE OWNER FIRST; the game may run for --core-only + extension)
# This stages a newly built core too. For an extension-only update, copy only the extension
# files and manifest, then reload through an identical copy of the installed core.
python3 extensions/core/tools/cssx.py stage --dev --core-only --perf-tuner

# make the running game reload extensions: copy the core to a -rNNNN name and point core.json at it
# (see the inline python in this session or extensions/cine/work/live/reload.py), then settle back.

# live checks (dev channel must be on: Mods/CSSX/dev/enabled.txt)
python3 extensions/perf-tuner/work/live/check.py model
python3 extensions/perf-tuner/work/live/check.py event tune
python3 extensions/perf-tuner/work/live/check.py report world-audit-report.txt
python3 extensions/core/tools/cssx.py request '{"op":"frame.stats","seconds":10}'
```

Game: `/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta`, GE-Proton 11-7, prefix
`steamapps/compatdata/2584270`. Player config:
`<prefix>/pfx/drive_c/users/steamuser/AppData/Local/MortalShell2/Saved/Config/Windows/`.
Reflected class dump: `MortalShell2/Binaries/Win64/ue4ss/CXXHeaderDump/`. UE 5.6.1 source:
`/home/eins0fx/development/mods/msII/CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine/Source`.

## 5. What has been done

### 5.1 Core change: controller shortcut removed (CSSX 1.3.1)
L3+R3 opened the menu mid-play and could leave it stuck. Removed `open_gamepad` from
`Settings` (`src/runtime/settings.*`), the 30 Hz pad poll in `Core::tick`, the menu texts
(`menu_page.cpp`), READMEs and docs. An `open_gamepad` key in an old `settings.json` is
dropped on the next save (test in `tests/runtime_tests.cpp`). Keyboard F6 unchanged.

### 5.2 Core change: `frame.brief` extended
`frame.brief {seconds?}` (1 to 60, default 10) now also returns `mean_ms, p95_ms, p99_ms,
max_ms, hitches`. Documented in `docs/abi.md`. The tuner samples frame time through this,
so it has no per-frame work of its own.

### 5.3 Perf Tuner, all host-tested and run live once
- Catalog (`tweaks.json`): 7 entries. Always on: texture pool (`r.Streaming.PoolSize`
  scaled from VRAM, 800..3000), Nanite streaming pool (512..1024), `s.ForceGCAfterLevelStreamedOut 0`,
  `gc.TimeBetweenPurgingPendingKillObjects 120`. Off until measured: `r.Nanite.AsyncRasterization.ShadowDepths 1`,
  `r.Nanite.MeshShaderRasterization 0` (Proton only), `t.Streamline.Reflex.Enable 0` (NVIDIA only).
  Fields: `platform`, `gpu`, `min_vram_mb`, `from_vram {fraction,min,max}`, `picture none|check`.
- Apply path: read `GetConsoleVariableFloatValue`, `ExecuteConsoleCommand "<cvar> <value>"`
  via the KismetSystemLibrary CDO, read back; a value that does not take rolls the whole
  tweak back and marks it unavailable. Restore on stop/off. Console variables are process-wide,
  so originals are kept across world changes (a bug here was fixed; test covers it).
- Trial: off/on/off/on, 15 s windows (5 s settle + 10 s measured), verdict gain/same/loss/unclear
  with a 1.5 % threshold and the consistency rule (both on windows beat both off windows).
- Manual resolution diagnostic: 100%, or 50% when already native, for one window.
  Requested values are verified. Old unverified verdicts are ignored. It reports pixel
  sensitivity, not a CPU/GPU bottleneck, and Tune never runs it.
- Tune button: queue = audit, every off-by-default switch that fits, then the kept set
  as a whole. Resolution diagnostics and experimental animation changes are manual only. Keeps what wins,
  turns the rest off, writes the summary to the row and to state.
- World audit: `GetAllActorsOfClass` per family (Character, Pawn, SkeletalMeshActor, Light,
  NiagaraActor; the bridge caps arrays at 4096 so "all actors" is not possible), class from the
  handle's `class` field, ~300 reflected calls per tick. Counts ticking actors, skeletal
  meshes animating while unseen, URO off, shadow lights (movable), live Niagara. One fix:
  `VisibilityBasedAnimTickOption` 0/1 -> 2 on non-player meshes, reversible, re-applied per world.
- Engine.ini: path from `GetProjectSavedDirectory()` + `Config/Windows/Engine.ini`, read once
  per session when a world is up. Each cvar line read back with `GetConsoleVariableStringValue`
  (empty string = the engine has no such variable = "dead"). Kinds: managed, game, picture,
  dead, setting, kept. Report written through `output.write` (lands in
  `Mods/CSSX/output/eins0fx.perf-tuner/`). Pack detection: UET (P40L0), KOALA stutter fix.
- Findings ("Check" section): machine line, Proton note, Engine.ini summary, GPU crash
  debugging, DLSS-G on an RTX 30, Reflex, HWRT (now says it measured faster here), texture
  pool when not live, VSM page pool vs shadow light count (after an audit), v-sync, render
  resolution, DXVK/VKD3D frame-rate env caps.
- Menu is a dynamic model (no menu.json). Ids may only contain `[A-Za-z0-9._-]`
  (`valid_id` in `core/src/runtime/common.cpp`). The core caches the model until the
  extension calls `invalidate()`; the tuner invalidates on world change, every 2 s (1 s during
  audit/trial) for the frame-rate line, and on every event.

### 5.4 Measured in the game (RTX 3060 12 GB, i5-12400F, GE-Proton 11-7, DLSS Quality 66.6 %)
See `docs/findings.md`. Historical tavern measurements: `stat unit` GPU 30.7 ms, RHI
thread 17.6 ms, game 8.6 ms. The old resolution probe lacked read-back verification;
discard its pixel-sensitivity conclusions. Hardware RT Lumen measured faster than software
(28.6 vs 31 ms). Always-on set: no measurable fps change, no hitch change in that scene.
Nanite without mesh shaders: 3.4% lower frame time in that earlier scene. Reflex off and
Nanite shadow async: no change. The game already limits unseen animation on NPCs.
The Codex live pass found no optional gain and rejected the default set on p99 regression.

### 5.5 Codex takeover, 6 Oct 2026

- Main tab: Tune, Enabled, Frame rate, and an available Stop action during work.
  Manual measurements are under Tests. Shortened tweak descriptions, findings, status
  messages, manifest copy and README. Native CSSX widgets and input bindings are retained.
- Tune never changes resolution. The separate Resolution test asks for confirmation,
  verifies the requested value, and produces no verdict if the engine refuses it.
- Experimental offscreen animation is no longer enabled by Tune. The previous claim
  that option 2 was the engine default was wrong; UE 5.6.1 constructors initialize option 0.
- Cleanup preserves originals across failed writes, world loss and refused restores.
  Read-back verifies restoration. Failed cleanup blocks DLL unload for a later retry.
  Kismet's CDO is cached, and cvar cleanup can use its null-world console path.
- Optional gains with more than 1.5% p99 regression are rejected. A combined loss or
  p99 regression disables the tuner and restores originals. Whole-set measurements
  include a manually enabled animation option rather than leaving it outside the comparison.
- Scene scans yield between actors after 500 microseconds when the host clock is available,
  as well as respecting the call cap. One actor can still exceed that time budget.
- Release packaging now includes a separate independently versioned Perf Tuner archive.
  The file set and ZIP verification are tested. No release command, tag or upload was run.
- Catalog validation reuses the Engine.ini picture classifier, closing gaps for fog detail,
  Nanite tessellation, ray tracing, NIS, secondary screen percentage and DLSS-G.
- Native CSSX ignores value fields on buttons and labels. Results now use button hints;
  frame rate and findings use label descriptions. Single-line descriptions avoid exposing
  the game's rich-text tags. Stored resolution verdicts without verified writes are ignored.
- Verified: all 11 host suites pass, including 121 Perf Tuner checks; Windows release and
  dev x64 DLL builds succeed. With the owner's approval, only Perf Tuner was installed.
  The original core and loader binaries were retained, and the original core selector
  was restored after reloading. No commit was made during that live pass.
- Live Tune completed all five steps. No optional gain in this scene. The combined set
  worsened p99 by 5.0%, so Tune disabled itself and restored the originals. Stop tuning
  and Stop test also restored all 26 watched values independently. Resolution stayed
  at 66.6%; no diagnostic or experimental animation ran. See findings.md for the numbers.
- Tune, Tests, confirmation, readings and both Stop actions were inspected in rendered
  screenshots, using dev-channel calls to the native key/action handlers. Physical
  controller and mouse input remain untested. The tuner is left off after acceptance.
- Both player config files stayed byte-identical. Own screenshots, intermediate DLLs,
  temporary core copy and test reports were removed; pre-existing reports were restored.

## 6. Known gaps, risks, next steps

- Tune and restoration passed live on this machine. Physical controller/mouse checks,
  combat/shadow testing of experimental animation and other-platform measurements remain.
- Not measured on Windows, AMD, Intel or Steam Deck. The rules exist; the numbers do not.
- SetBy priority: a cvar written from the console (`SetByConsole`) outranks the game's
  scalability until restart, so while `r.Streaming.PoolSize` is touched the Textures quality
  menu no longer moves that one variable. Documented in the tweak text. Reflection has no
  `Unset`; a native `IConsoleVariable::Unset(ECVF_SetByConsole)` would fix it but needs a core
  service, not an extension call.
- Cheat-flagged cvars cannot be set from the console in a shipping build; the read-back
  catches it. `ECVF_ReadOnly` ones are the same.
- Do not resize `r.Shadow.Virtual.MaxPhysicalPages` live: a resize (512 -> 4096) ended in a
  GPU crash on this machine once (CINE, 6 Oct). The tuner only reports it.
- Toggling `r.Lumen.HardwareRayTracing`, `r.RayTracing.Enable`, `r.Shadow.Virtual.Enable`,
  `r.Nanite.Tessellation` recreates render state (multi-frame hitch); never inside a trial window.
- The separately requested diagnostic sets `r.ScreenPercentage` for one window. If DLSS
  or the engine keeps its own value, the diagnostic stops without a new verdict.
- Hitch attribution is frame-level only (shipping build: `stat gpu`, `ProfileGPU`, CSV
  profiler compiled out; `stat unit` draws on screen and can be read from an engine
  screenshot, see `work/live/stat.py`).
- The live combined-set transition/cleanup capture included a 19.08 ms extension peak.
  Idle tuner work averaged 0.013 ms per tick. This is not a zero-cost guarantee for cvar
  changes; settling windows exclude transitions. See findings.md for scope and scan cost.
- Release packaging is implemented in `extensions/core/tools/cssx_release.py`, but the
  tagged release flow has not been run. Version 1.0.0 is unreleased; keep the number until it
  is on Nexus (owner's rule: unreleased stays the same version).
- Nexus-facing text: changelog entries must lead with the fixed state and claim only what was
  tested (`extensions/core/docs/release-notes.md`, "CSSX 1.3.1 and CINE 1.1.0").

## 7. Paths to commit (and nothing else)

```
extensions/core/CMakeLists.txt
extensions/core/docs/abi.md
extensions/core/docs/performance.md
extensions/core/docs/release-notes.md
extensions/core/docs/ui-standard.md
extensions/core/packaging/README.txt
extensions/core/src/core/core.cpp
extensions/core/src/core/core.hpp
extensions/core/src/core/menu_page.cpp
extensions/core/src/runtime/settings.cpp
extensions/core/src/runtime/settings.hpp
extensions/core/tests/runtime_tests.cpp
extensions/core/tests/test_tools.py
extensions/core/tools/cssx.py
extensions/core/tools/cssx_release.py
extensions/cheat-menu/README.txt
extensions/perf-tuner/            (whole tree; .gitignore excludes work/research/ini, uet, tool binaries, docs/*.zip, docs/txt)
```

The original two commits already exist. The owner has now requested a separate commit
for the remaining Perf Tuner takeover changes. Do not add an AI co-author line; the owner's later instruction
in the previous session rejected it.

## 8. Working rules of this repository (owner's standing instructions)

- `pkexec`, never `sudo`. Plain, direct writing; no em dashes; no AI phrasing.
- Research online before relying on memory, especially for third-party software.
- Ask before installing any DLL or pak into the game. The game must be closed to swap the
  loader (`dlls/main.dll`) or a pak; the core and extensions hot-swap while it runs.
- Never start anything in the owner's game without telling them; scripts that drive the game
  need a `main()` guard and must not act on import.
- After live work: verify the game state is restored and delete your own leftovers
  (screenshots, `-rNNNN` core copies, output files you made).
- No MangoHud or env injection into Steam. Measure with CSSX's own frame ring.
- Never push, merge, tag or release unless asked. The reference music files under
  `extensions/cine/` are personal; never commit them.
- CSSX core must never know a mod by name (mod-neutral); extensions may.
