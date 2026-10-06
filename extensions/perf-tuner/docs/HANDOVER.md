# Perf Tuner: handover

Repository: `/home/eins0fx/development/mods/msII/CustomShellSystem`, branch `feature/cine-scene`.
Everything below is uncommitted as of 2026-10-06. Another agent works in the same checkout
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
  tests/tuner_tests.cpp   89 checks against a mock host (catalog, trial, bridge, ini, audit, tune, fit)
  docs/DESIGN.md          evidence and rejected candidates
  docs/findings.md        numbers measured in the game
  docs/HANDOVER.md        this file
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
- Probe: 100 % screen percentage for one window; verdict "scales with pixels" / "does not
  scale with pixels" / mixed. (First wording said CPU-bound; wrong, corrected.)
- Tune button: queue = audit, probe, every off-by-default switch that fits, the world fix
  (if the audit found 2+ fixable meshes), then the kept set as a whole. Keeps what wins,
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
See `docs/findings.md`. Summary: tavern area is GPU-limited (`stat unit`: GPU 30.7 ms, RHI
thread 17.6 ms, game 8.6 ms) with work that does not scale with pixels (22 shadow lights on a
512-page VSM pool, Lumen scene lighting). Hardware RT Lumen measured faster than software
(28.6 vs 31 ms). Always-on set: no measurable fps change, no hitch change in that scene.
Nanite without mesh shaders: +3.4 % consistent. Reflex off and Nanite shadow async: no
change. The game already animates NPCs only when seen. CSSX's own frame share: 0.1 ms.

## 6. Known gaps, risks, next steps

- Not yet run: the Tune button end to end in the game (unit-tested on the mock world only).
  Run it once in the world and check the summary row and that `Perf Tuner off` restores all.
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
- The probe sets `r.ScreenPercentage` for 5 s. With DLSS the game keeps 66.6 there; the
  tuner restores exactly what it read.
- Hitch attribution is frame-level only (shipping build: `stat gpu`, `ProfileGPU`, CSV
  profiler compiled out; `stat unit` draws on screen and can be read from an engine
  screenshot, see `work/live/stat.py`).
- Release packaging: `extensions/core/tools/cssx_release.py` has no Perf Tuner section yet
  (follow the Traverse/CINE pattern). Version 1.0.0 is unreleased; keep the number until it
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
extensions/core/tools/cssx.py
extensions/cheat-menu/README.txt
extensions/perf-tuner/            (whole tree; .gitignore excludes work/research/ini, uet, tool binaries, docs/*.zip, docs/txt)
```

Commit message ends with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`. Two
commits are natural: "CSSX: remove the controller shortcut; frame.brief window and percentiles"
and "Perf Tuner 1.0.0". Commit only when the owner asks.

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
