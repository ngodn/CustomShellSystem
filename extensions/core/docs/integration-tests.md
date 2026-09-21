# Integration tests and live checks

Each row names the check, how it is run, and the last evidence. "Not yet"
means exactly that. Evidence paths are under `extensions/core/work/` unless
stated. Release is gated on every row being a pass.

## Framework

| Check | How | Last result |
| --- | --- | --- |
| Loader starts as its own UE4SS mod beside CSS | `UE4SS.log` shows `Mod 'CSSX' has enabled.txt` and `Mod 'CustomShellSystem'`; `CSSX.log` shows core activated | Pass 2026-09-21 15:35 (dev build) |
| Core starts with no player yet, then finds the player | `runtime/status.json` player flag after loading | Pass 2026-09-21 |
| Legacy detection | `status.legacy` fields; activation absent on this machine, stale cheat-menu folder reported as shared id | Pass |
| Migration (game closed) | `tools/cssx_migrate.py` dry run then perform; backup `Mods/CSSX/backup/20260921T160844Z` | Pass 2026-09-22: 6 moves, 16 stale cssx_core DLLs removed, CSS files untouched, saved Cheat Menu preferences carried over |
| Dev channel round trip | `tools/cssx.py request '{"op":"status"}'` | Pass |
| Exact rc1 release DLLs load in game | `dist/v1.0.0-rc1/*.built` staged at exit; loader.json, library, `cheat_check.py` after relaunch | Pass 2026-09-22 (core 1.0.0, both extensions available, cheat check 6/6) |
| Live core switch while the game runs | stage `--core-only`, watch `CSSX.log` for "Core activated" | Pass ×8 in one session; first attempt needed the watch-path fix (loader now watches the mod root) |
| Extension load: ABI 3 Cheat Menu | `library` shows available, cost counters advance | Pass |
| Legacy ABI 1/2 extension loads | host lifecycle test with fixtures; in game the migrated `cssx.ui-kit` (schema 1, Lua) loads beside the ABI 3 Cheat Menu | Pass (fixtures + live Lua 2026-09-22); legacy native DLL in game not yet |
| Malformed / mismatched extension is rejected | host lifecycle test (`unknown-api`, `abi-mismatch`, broken Lua) | Pass |
| Settings default, round-trip, backup recovery | host runtime test | Pass |
| CSS alone still works with CSSX disabled | `enabled.txt` removed, restart, CSS log and Inventory tab | Pass 2026-09-21 (row B launch) |

## Player Menu tab

| Check | How | Last result |
| --- | --- | --- |
| Tab appears after CSS, before TARSTONES | screenshot `work/screens/13-library-keys.jpg` | Pass |
| CSS's own tab still attaches and orders with CSSX present | CSS tab visible and functional in the same menu | Pass (visible; CSS page interaction not yet exercised) |
| Attach without CSS installed | needs a launch with `CustomShellSystem/enabled.txt` removed | Not yet |
| Library, extension page, sections, confirm, picker, settings | screenshots 05–13 | Pass (visual review) |
| Keyboard/controller glyphs follow the game's bindings | diagnostics `bindings`, screenshot 13 | Pass for keyboard glyphs; controller glyph rendering not yet seen |
| Controller navigation in the page (D-pad, sticks, A/B, triggers) | user test | Not yet confirmed |
| Hotkey F6 / L3+R3 opens on the CSSX tab, closes from it | dev `menu.hotkey` path pass; real key press | Pass for opening: single "Hotkey: opening" log lines at 16:36 and 19:56 outside any tool run are real presses. Closing from another tab switches back instead (by design) |
| Logo in library header, banner + performance line on the CSSX entry | screenshots `20-21` | Pass 2026-09-22 |
| Notice strip shows unapplied edits and applies from any section | screenshot `23` | Pass 2026-09-22 (strip visible; Apply through the strip not yet clicked by hand) |
| Live core switch does not crash the game | four switches in one session (rc1, diagnostic, cache-free, glyph build) | Pass 2026-09-22 |
| Back on library closes the Player Menu; Back inside an extension returns | dev `menu.key close` | Pass |
| Menu instance replaced by travel: tab dropped and re-attached | beacon travel with the menu closed and open | Not yet |
| Repeated open/close without leaks | `tools/menu_stress.py --cycles 10` | Pass 10/10 (2026-09-22), 51 rebuilds at 3 ms, 34 widgets, menu and game menu closed afterwards |
| Mouse: rows, buttons, slider drag, picker rows | user test | Not yet |
| Text entry (search field) | user test | Not yet |

## Cheat Menu

| Check | How | Last result |
| --- | --- | --- |
| Model validates against menu.json schema 2 | host test + live `model` | Pass |
| Draft does not touch gameplay; Apply enables God; Disable all restores | `tools/cheat_check.py` | Pass 2026-09-22 (6/6, pawn damageable afterwards) |
| Every reversible control audited live (God, movement, damage target, auto heal, heal, infinite resolve, resolve, shell points, no cooldown, seal cheats armed, powers, lists, revive, shortcuts, confirmations) | `tools/cheat_audit.py` (`work/live/audit/cheat-audit-1.json`) | 31/33 on 2026-09-22; the two fails were audit bugs (measured body instead of shell health; counted an amount field as an action), both fixed. Movement off had crashed the game before the bridge fix (in-place struct writes); verified clean afterwards with repeated struct reads |
| Grants and unlocks run right after changing an amount; the game applies them (gates, weapons, map) | user test after the D14 switch | Pass 2026-09-22 ("seems working"); unlock bodies confirmed from the exported controller Blueprint (UnlockGate per gate actor, UnlockWeapon per tag, map reveal broadcasts) |
| Controller navigation in the CSSX tab | user test | Pass 2026-09-22 ("that works") |
| Damage to target acts on shell health first | manual read of GetShellHealth before/after | Pass (130.9 → 65.4 → healed 130.9) |
| No cooldown with abilities lacking a cooldown field; Apply with one failing feature | host tests (`cssx_cheat_menu`) | Pass 2026-09-22 (missing-field text fixed; per-feature apply; armed seal cheats) |
| Confirmation before persistent grants | screenshot 10, cancelled | Pass |
| Shell picker lists the live catalog | screenshot 11 | Pass |
| Death, respawn, beacon travel, shell change, load/save with cheats on | user play | Not yet |
| Enemies hit through an apparently invulnerable character with all cheats off | user play | Not yet |
| Idle tick cost | `frame.stats` per-op accounting | 12 µs per frame extension phase, 121 µs core tick, cheats off (2026-09-22) |

## Performance

See `performance.md`. Rows A/B need MangoHud or the loader-only row; rows C, D, E, F, G through `fps_probe.py`.

| Check | How | Last result |
| --- | --- | --- |
| No disk write on the game thread for status/logs | code: `Writer` thread; host test `cssx_runtime` (replace/append/rotate/drain) | Pass 2026-09-22 (host) |
| Cheats-on cost row (five cheats, 231 hook rules) | `tools/hitches.py` 60 s (`hitches-cheats-on-2.json`) | Pass 2026-09-22: CSSX mean 135 µs, worst 2.7 ms, no frame where CSSX exceeds a quarter |
| Beacon travel keeps the tab working | user test | Pass 2026-09-22 ("seems good") |
| Hitch attribution: core share at hitch frames | `tools/hitches.py` (`work/live/audit/hitches-steady-1.json`) | Pass 2026-09-22: 60 s of play, 2642 frames, 0 hitches, max frame 37 ms, p99 29.7 ms, 44 fps, core 129 µs mean, largest core frame 2.1 ms |
