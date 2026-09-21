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

## D6. Menu UI is a tab inside the game's Player Menu (user decision, 2026-09-21)

First implementation was a separate full-screen overlay opened through the
game's `EnableUserInterfaceInput`; it worked (screens in `work/screens/01-03`)
but the user wants CSSX where CSS is: a tab in the Player Menu, after CSS and
before TARSTONES (after INVENTORY when CSS is absent). Implemented like CSS's
own integration: a `WBP_NB_Menu_C` tab styled from the Inventory tab and a
`WBP_Menu_Game_Tab_C` page whose root is a CanvasPanel, added to
`BP_HBC_Menu_Game` / `BP_WS_Menu_Game` and re-ordered with the original
slot padding scaled (0.6 with five tabs, 0.75 with four).

Coexistence with the unchanged CSS alpha: CSS attaches when it sees three
pages and orders four, both in one tick, so CSSX waits until the page count
is 4 when CSS is installed and enabled (`Mods/CustomShellSystem/enabled.txt`
plus `dlls/main.dll`), and attaches at once with 3 pages when CSS is absent.
If CSS is installed but never attaches, CSSX stops waiting after two seconds.
A Player Menu with five or more pages before CSSX attaches is refused.

Input: the game's own menu actions read from its mapping context
(`InputMapping.Mappings`, keys declared in the context, overridden by the
player's applied mappings when Enhanced Input answers), plus the left stick
for navigation because the page has no character preview. Back on the
library closes the Player Menu through `HandleGameMenu(0, true)` exactly as
CSS does; Back inside an extension returns to the library. The hotkey (F6 or
both sticks) opens the Player Menu through `HandleGameMenu(0, false)` and
selects the CSSX tab, or closes it when the CSSX page is showing.

Rendering rules unchanged: retained widgets, rebuilt on change only, nothing
built while the tab is not showing.

## D7. Version numbers

Product 1.0.0 (`VERSION`), extension ABI 3 (`CSSX_ABI`), manifest schema 2,
menu schema 2 (accepts 1), loader/core ABI 1, settings schema 1. Release
metadata records all five.

## D8. Loader-only measurement row

The CSS alpha production core does not answer the old `css_probe` counter, so
a "CSSX absent" row cannot be measured in-process. Two substitutes: MangoHud
frame-time logs (external, identical for every row) and a loader-only row
where `core.json` names a missing core, so only `dlls/main.dll` with its one
tick hook is loaded and the loader itself dumps `runtime/frames.json` every
ten seconds in developer mode. Loader-only differs from absent by one
QueryPerformanceCounter read and one mutex per frame.

## D9. What is deliberately not promised

- No sandboxing of native DLLs and no recovery from memory corruption.
- No hot reload for players.
- No ABI-2 minimap operations.
- No claim of zero cost; the `frame.stats` numbers are the claim.

## D10. The game thread never waits on the disk for informational files (2026-09-22)

Status, frame dumps, logs and dev responses are written by one background
thread (`src/runtime/writer.{hpp,cpp}`), queued as bytes; the game thread
returns at once. Only explicit user actions (settings save, extension state
save) keep the synchronous fsynced path. Reason: `FlushFileBuffers` plus a
write-through rename every 5 s on the game thread is an fsync under Proton
and showed up to the user as periodic drops that a median cannot reveal. The
writer is owned by `Core`, so it is joined before the core DLL is unloaded.

## D11. Cheat Menu Apply is per feature, never all-or-nothing (2026-09-22)

Each feature (God, movement, shell points, heal/resolve, combat group, each
shell power) applies inside its own try. A feature that fails reverts to its
previous value, is named in the status line in red, and the rest goes live.
A failed preference save is reported and never undoes a live cheat. Reason:
the user's log showed one refused toggle ("Equip the matching seal") throwing
away every other edit, which read as "nothing works".

## D12. Seal cheats arm; they do not fail (2026-09-22)

Perfect parry/block/harden keep their toggle on while the matching seal is
not equipped; no hook exists until it is, and the status says "Perfect parry
waits for the Infinite seal." Equipping the seal installs the hooks on the
next sync (1 s); unequipping removes them and keeps the toggle armed. The
loader hook rule still checks the seal at call time, so the cheat can never
act through another seal.

## D13. CSSX has a face (2026-09-22)

`assets/logo.png` and `assets/banner.png` (drawn as SVG in `assets/`,
rasterised with rsvg-convert) ship in the framework ZIP and are read by the
menu through the same `ImportFileAsTexture2D` path as extension banners.
Framework pages (library, settings) carry the emblem; extension pages carry
the extension's own title and banner.
