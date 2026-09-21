# CSSX standalone work queue

Newest first. Each entry says what is done, what evidence exists and what is next.

2026-09-22 (later): Controlled MangoHud pair done: CSS alone 22.24 ms median
/ 42.0 fps; with CSSX + Cheat Menu + UI Kit 22.53 ms / 44.4 fps. No
measurable cost from loading CSSX. Per-op accounting then removed the real
costs inside the Cheat Menu (readiness probe, catalog hitch, class-default
scan, combat decode). Live cheat check 6/6, 3-cycle menu stress clean,
migrated Lua UI Kit loads. Still open: physical hotkey/controller confirmation
from the user, 30-cycle stress in an idle window, travel re-attach, cheats-on
FPS row, release build from a real tag.

2026-09-22: Menu moved into the Player Menu as a tab after CSS (user
decision); library, sections, confirm, picker and settings verified by
screenshot (`work/screens/05-13`). Hotkey path verified through the dev
channel; a second raw-keyboard path through UE4SS input added in the loader.
Migration run with backup. Controlled FPS pair in progress with MangoHud
(Steam restarted with `MANGOHUD=1`): row B (CSS alone) median 22.2 ms /
42 fps while playing; row D (CSSX on) recording next. User reports the
hotkey and controller "not working"; physical confirmation still pending.

2026-09-21 (later): Loader, core, bridge, HUD service, menu, extension runtime,
settings, dev channel, tools and tests exist and build: Linux host tests pass
(runtime, cheat menu, lifecycle with ABI 1/2/3 fixtures, tools), Windows
cross-build produces main.dll, cssx_core.dll and cheat_menu.dll with zero
warnings. Nothing is installed in the game yet. The pinned UE 5.6 source
confirms a plain UserWidget creates its own WidgetTree on Initialize, and the
game header dump matches every UI handler, prompt and UMG library signature
the menu calls. Next: user authorises staging `Mods/CSSX` (dev build + Cheat
Menu) and a restart; then the first live checks in this order: loader starts
with CSS present, status.json, menu open/close with input restore, library and
Cheat Menu pages by eye, frame.stats rows C and D, then the coexistence row G.

2026-09-21: Evidence pass complete (legacy runtime, bridge, hooks, HUD, UI,
Cheat Menu, tooling, UE4SS pinned source, game header dump). Decisions recorded
in `decisions.md`; performance evidence indexed in `performance.md`; migration
contract in `migration.md`. User fixed the install root to `ue4ss/Mods/CSSX/`.
Next: build the loader + core skeleton that starts with CSS absent, ships frame
instrumentation from day one, and exposes the dev request channel; then the
host bridge, runtime, UI, Cheat Menu port, tools, tests, packaging.

## Blocked on the user

- Live test windows (each row of the performance table needs a restart and the
  same save/camera; the game is currently running with the CSS alpha).
- Authorisation before staging anything into `ue4ss/Mods/CSSX/` on this machine.
- Public upload of the two release ZIPs.

## Deferred (documented, not silently dropped)

- Developer core hot reload safety proof for this implementation.
- ABI-2 `hud.minimap.*` operations (CSS-specific; the Compass/Minimap extension
  needs its own port to the standalone HUD service).
- H4: the ABI-1 era slowdown cannot be reproduced without replacing the user's
  CSS alpha with the archived 0.3.1 pair.
