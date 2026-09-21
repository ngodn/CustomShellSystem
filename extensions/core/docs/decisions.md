# CSSX v1.0.0 standalone: decisions and rejected alternatives

Started 2026-09-21. Execution handoff: `docs/cssx/standalone-v1-agent-prompt.md`.
Each entry states the decision, the evidence it rests on, and what was rejected.
Hypotheses are marked as such; they are not proven causes.

## D1. Install root is its own UE4SS mod: `ue4ss/Mods/CSSX/`

Decided by the user on 2026-09-21. UE4SS starts every `Mods/<name>/dlls/main.dll`
that has an `enabled.txt` (pinned source `UE4SS/src/UE4SSProgram.cpp`, "Part #2",
no defined order; `mods.txt` entries start first). CSSX is therefore a sibling
of `CustomShellSystem`, not a file inside it.

Layout on disk:

```text
ue4ss/Mods/CSSX/
  enabled.txt            activation marker (UE4SS reads it)
  dlls/main.dll          permanent loader: UE4SS hooks, key events, core selection
  core/cssx_core-<v>.dll runtime: host bridge, UI, extension lifecycle
  core.json              {"file": "cssx_core-1.0.0.dll"}
  settings.json          CSSX settings (hotkeys, ui)
  extensions/<id>/       extension folders (extension.json + entry)
  state/<id>.json        extension settings (+ .bak)
  logs/cssx.jsonl        framework log, rotated
  logs/<id>/current.jsonl
  output/<id>/
  runtime/               live acknowledgements (loader.json, status.json)
  dev/enabled.txt        optional: enables the file request channel
  backup/<stamp>/        migration backups
  README.txt, THIRD_PARTY_NOTICES.txt, release.json
```

Rejected: keeping `CustomShellSystem/cssx.json` + `cores/cssx_core.dll`. That is
the legacy activation the unchanged CSS alpha still honours, so it can never be
standalone and would start a second host. See `migration.md`.

## D2. Loader + reloadable core, single C ABI between them

`dlls/main.dll` stays small and permanent: it registers the one UE4SS engine
tick callback, the keyboard hotkey, and owns the hook service (the engine-facing
per-function callback registry) so the core can be replaced without leaving
callbacks pointing into freed code. `core/cssx_core-<v>.dll` holds everything
else. The loader/core ABI (`src/loader/core_abi.h`) is versioned separately from
the extension ABI.

Why: the legacy CSS loader/core split is the one part of the old design that
survived every incident (the September 20 crash was in CSS's animation
restoration during `Core::stop`, not in the loader mechanism). Live UI iteration
without restarts is worth the small ABI. Core replacement is a developer path;
players use restarts. Reload is not advertised as safe until demonstrated for
this implementation (work-queue item).

Selector watching uses a Windows directory change notification on `core/` with a
zero-timeout wait on UE4SS's update thread. No periodic `stat` of `core.json`.

Rejected: one DLL (every UI tweak needs a game restart the user must perform);
UE4SS's own C++ hot reload (it re-runs `start_mod` without unloading, not a
tested path for this build).

## D3. Engine access only on the game thread, through `EngineTickPost`

`RegisterEngineTickPostCallback` is the proven dispatch point (CSS uses it). All
UObject reads/writes, widget construction and extension callbacks run inside it.
UE4SS key events arrive on UE4SS's input thread and only set an atomic flag the
tick consumes. No `ProcessEvent` global interception is installed by the
framework. Per-function hooks are installed only when an extension adds a rule
and removed when the last rule goes.

Known cost to measure, not assume (hypothesis H2 in `performance.md`): the
legacy hook service registered a *global* `ProcessLocalScriptFunction` post
callback the moment any Blueprint-function rule existed; every Blueprint call in
the game then paid a mutex plus map lookup. The new service keeps that design
but records call counts and time so the cost is visible, and the Cheat Menu
documents which cheats install script-function rules.

## D4. Frame instrumentation is always on, bounded, and measured

The loader records the wall-clock interval between consecutive tick callbacks
into a fixed 8192-entry ring (one `QueryPerformanceCounter` read and one store
per frame). The core adds phase timers (framework tick, extension ticks, UI,
HUD) with the same primitive. `frame.stats` reports count, mean, median, p95,
p99, max, hitches (>2x median) and per-phase totals for the last N seconds. Its
own overhead is measured by the host test (`frame_stats_tests`) and reported.

Rejected: Unreal Insights / Slate Insights. The shipped build has no
`-trace` channel exposure we can reach without the editor; the external fallback
is `perf` userspace sampling of the Proton process (already used on Sept 20).

## D5. Extension ABI 3 keeps the JSON bridge; legacy ABI 1/2 tables load through a prefix-compatible adapter

The reflection request protocol (`player`, `get`, `set`, `call`, `describe`,
`map.update`, `hooks.*`, `state.*`, `log`) is what the Cheat Menu's validated
gameplay logic is written against, and it is checked at every call against live
reflection. It stays. What changes: the host struct gains a direct `log` and
`invalidate`, the frame/HUD tables are versioned, and the manifest schema is 2.

`CssxHost`, `CssxExtension` and `CssxHudApi` ABI-3 structs are strict supersets
of the ABI-1/2 layouts. An extension declaring ABI 1 or 2 is given a host stamped
with that ABI and the matching struct prefix, exactly as the legacy runtime did.
Unknown ABIs and manifests with an unknown `api` are rejected before `create`.
`hud.minimap.*` and `minimap_update` (a CSS-specific hack in ABI 2) are refused
with a clear error; they never existed outside CSS's engine TU.

Lua extensions stay supported (same sandbox: base/table/string/math/utf8, memory
and instruction budgets). The UI Kit gallery is Lua and is the reference menu.

## D6. Menu UI is UMG built by reflection, opened through the game's own UI handler

The menu is a `UserWidget` whose root is a `CanvasPanel`, added to the viewport
with a high Z order. Opening calls
`BPC_UserInterfaceHandler_C::EnableUserInterfaceInput(widget, lock, addMapping,
showCursor, pause, hideHUD)` and `UpdateActiveMenu(widget)`; closing calls
`ResetActiveMenu` and `DisableUserInterfaceInput`. This is the same path the
game's own Inventory/Map/Options use (header dump `BPC_UserInterfaceHandler`),
so pause, cursor, HUD hiding, the menu input mapping context and the game's
"a menu is active" state behave like a native menu. Navigation input is read
from the game's Enhanced Input menu actions (`IA_Menu_*`) so remapped keys and
controller glyphs are honoured, as CSS does.

Every widget is retained; rebuilds happen on model revision, selection or
layout change, never per frame. Per-frame work while open: key polling and one
mouse-position read; hover tests only when the cursor moved. Closed menu: no
widgets exist and no per-frame UI work runs.

Coexistence rule: CSSX ignores its hotkey while `bIsInGameMenu` is true or
`ActiveMenu` is set (CSS's tab lives inside the game menu). While CSSX is open,
if the game reports another active menu, CSSX closes itself and restores input.
All close paths (Back, hotkey, travel, pawn loss, handler loss, core stop) go
through one `close()` that restores the prior mode exactly once.

Rejected: injecting a tab into `WBP_MGT_Main` (that is CSS's integration and
would couple to CSS's page count checks); ImGui through UE4SS's GUI (external
render thread, no controller support, not the game's look).

## D7. Version numbers

Product 1.0.0 (`VERSION`), extension ABI 3 (`CSSX_ABI`), manifest schema 2,
menu schema 2 (accepts 1), loader/core ABI 1, settings schema 1. Release
metadata records all five.

## D8. What is deliberately not promised

- No sandboxing of native DLLs and no recovery from memory corruption.
- No hot reload for players.
- No ABI-2 minimap operations.
- No claim of zero cost; the `frame.stats` numbers are the claim.
