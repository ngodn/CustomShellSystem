# Integration tests and live checks

Each row names the check, how it is run, and the last evidence. "Not yet"
means exactly that. Evidence paths are under `extensions/core/work/` unless
stated. Release is gated on every row being a pass.

## Framework

| Check | How | Last result |
| --- | --- | --- |
| Loader starts as its own UE4SS mod beside CSS | `UE4SS.log` shows `Mod 'CSSX' has enabled.txt` and `Mod 'CustomShellSystem'`; `CSSX.log` shows core activated | Pass 2026-09-21 15:35 (dev build) |
| Core starts with no player yet, then finds the player | `runtime/status.json` player flag after loading | Pass 2026-09-21 |
| Legacy detection | `status.legacy` fields; activation absent on this machine, stale cheat-menu folder reported as shared id | Pass (detection); migration run not yet |
| Dev channel round trip | `tools/cssx.py request '{"op":"status"}'` | Pass |
| Live core switch while the game runs | stage `--core-only`, watch `CSSX.log` for "Core activated" | Pass ×8 in one session; first attempt needed the watch-path fix (loader now watches the mod root) |
| Extension load: ABI 3 Cheat Menu | `library` shows available, cost counters advance | Pass |
| Legacy ABI 1/2 extension loads | host lifecycle test with fixtures | Pass (Linux fixtures); in-game legacy DLL not yet |
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
| Hotkey F6 / L3+R3 opens on the CSSX tab, closes from it | dev `menu.hotkey` path pass; real key press | Code path pass; physical key press not yet confirmed |
| Back on library closes the Player Menu; Back inside an extension returns | dev `menu.key close` | Pass |
| Menu instance replaced by travel: tab dropped and re-attached | beacon travel with the menu closed and open | Not yet |
| Repeated open/close (50×) without leaks | script | Not yet |
| Mouse: rows, buttons, slider drag, picker rows | user test | Not yet |
| Text entry (search field) | user test | Not yet |

## Cheat Menu

| Check | How | Last result |
| --- | --- | --- |
| Model validates against menu.json schema 2 | host test + live `model` | Pass |
| Draft does not touch gameplay; Apply enables God; Disable all restores | port of `tools/cssx_cheat_check.py` to the dev channel | Not yet |
| Confirmation before persistent grants | screenshot 10, cancelled | Pass |
| Shell picker lists the live catalog | screenshot 11 | Pass |
| Death, respawn, beacon travel, shell change, load/save with cheats on | user play | Not yet |
| Enemies hit through an apparently invulnerable character with all cheats off | user play | Not yet |
| Idle tick cost | `library.cost`: 107 µs mean after warm-up before the lazy readiness change; re-measure | Re-measure |

## Performance

See `performance.md`. Rows A/B need MangoHud or the loader-only row; rows C, D, E, F, G through `fps_probe.py`.
