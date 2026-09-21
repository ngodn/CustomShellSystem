# CSSX standalone work queue

Newest first. Each entry says what is done, what evidence exists and what is next.

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
