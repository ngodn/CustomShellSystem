# CSSX standalone work queue

Newest first. Each entry says what is done, what evidence exists and what is next.

2026-10-06 (local Nexus release): The owner requests CSSX 1.3.1 and Cheat Menu
1.1.2. CSSX stays at 1.3.1; Cheat Menu's manifest and README now say 1.1.2.
Player release notes are in docs/releases/cssx-1.3.1.md and
docs/releases/cheat-menu-1.1.2.md. Release main/cssx_core/cheat_menu builds pass.
Two uncommitted candidate ZIPs in ignored
work/release-candidates-20261006T145021Z pass PE, file-set and hash verification;
they exclude the dev channel, state and logs. These are review candidates,
not tagged releases. The owner confirms typing, Enter and Escape work in the
installed build and requests local ZIPs for Nexus with the usual BBCode notes.
The owner clarifies that both ZIPs and their BBCode files must be in the same
release folder, and rejects the older verbose changelog template. Notes now
contain only a short change list and Cheat Menu's CSSX requirement. The pair
command copies both notes next to the ZIPs in dist/cssx-v1.3.1/ automatically;
the matching-version/missing-note cases are tested.
The release pair command builds just CSSX and Cheat Menu from an annotated
tag exported with git archive, without a worktree or other unfinished changes.
Its source-isolation test and all 11 host suites pass. BBCode changelogs and
updated file-upload instructions are under docs/nexus/cssx/ and
docs/nexus/cssx-cheat-menu/. Final source is ad6c573, tagged locally as
cssx-v1.3.1 and v1.1.2-cheat-menu. The two ZIPs, their checksums and both short
BBCode notes are together in extensions/core/dist/cssx-v1.3.1/. Release builds,
archive/PE/hash checks, tag provenance and exact tagged note bytes pass. The
superseded candidate directory was removed. The report is in ignored
work/cssx-1.3.1-release-verification.json. No remote publication occurred.
The installed test build remains Cheat
Menu 1.1.1 with the same number-field implementation; its manifest has not
been relabelled 1.1.2 during release preparation.

2026-10-06 (number entry, awaiting live check): The owner wants typed quantities in
Cheat Menu instead of numeric sliders or repeated arrow presses. CSSX's
number control now uses its native text-field recipe with range/step validation,
Enter and Set value commit, Escape cancel, and controller arrows. Cheat Menu
uses number fields for all numeric settings. Quantity descriptions no longer
send the player to Apply settings. All 11 host suites pass, including number
parsing and the quantity event path with pending cheat edits preserved. Release
and dev Windows builds of cssx_core and cheat_menu pass; git diff --check passes.
Installed with the owner's approval at 2026-10-06 13:51 UTC while the game was
closed: core cssx_core-1.3.1-dev-d5fe2c248dfe.dll and Cheat Menu
cheat_menu-7b7721f71372.dll, plus menu.json and README. Copy hashes verified.
Ten protected files, including the loader, saved settings and player graphics
configs, stayed unchanged. Rollback files and install report are in ignored
work/cheat-number-entry-20261006T135100Z. No resource or progression action ran.
Next: check native input and rendered layout after the owner launches the game.

UI gate review: existing game assets, typography, layout and accent retained;
no new assets, decoration or animation. Clear action labels and validation errors
are implemented. Enter, Escape, focus changes, mouse Set value, controller
arrows, disabled state and viewport/scale behavior need a rendered click-through
after installation. This is an offline-tested candidate, not a completed live
UI acceptance pass. Native palette contrast and physical input remain unverified.

2026-09-27 (v1.2.0, native page): The CSSX page is rebuilt on the game's own
widget blueprints with a pooled widget model (`src/core/menu_page.cpp`, the
CSS beta.5 recipe, no CSS dependency). Verified live through the dev channel
with screenshots `work/screens/50-76`: library, extension sections, toggles,
numbers, sliders (drag and arrows), the inline picker, the pooled confirmation
dialog, settings and the 95% rescale. Costs from `menu.diagnostics`: selection
move 0.1-0.6 ms, new pooled kinds 1.5-5 ms once, dialog 4 / 2.4 ms, skeleton
30 ms once (warm-up path when the Player Menu opens on another tab), idle
with the page open ~0.06 ms per frame, closed ~3 us. Extension DLLs
untouched (ABI 3, menu schema 2). Driver scripts: `work/ui-overhaul/drive*.py`.
Open: the user's own look at it in game (controller glyphs, mouse clicks on
the game's transparent buttons), then a release build and tag.

2026-09-22 (v1.0.0, re-cut): Added the CSSX Performance extension (Lua,
`performance/`) and the `frame.brief` host op it uses; tag `v1.0.0-cssx`
moved to this commit and the three ZIPs rebuilt (nothing had been uploaded).

2026-09-22 (v1.0.0): Tag `v1.0.0-cssx` on the rc2 commit; ZIPs in
`dist/v1.0.0/` (MSII-CSSX-v1.0.0.zip febd278a…, CSSX-Cheat-Menu-v1.0.0.zip
acc0e4c7…), verified. The exact release DLLs are installed in Mods/CSSX for
the user's next launch; if that run differs from the dev build the tag is
replaced before upload. Public upload is the user's.

2026-09-22 (rc2): User accepted unlocks, grants, controller navigation and
beacon travel ("seems good, we can release"). Last fixes before the tag:
action parameters no longer gate actions (D14), disabled reasons, the CSSX
artwork, ability scan only on change with cooldown hooks a few per second,
smaller images. Tag `v1.0.0-cssx-rc2`, ZIPs in `dist/v1.0.0-rc2/`; the exact
release DLLs are installed at the next game exit for the release-build run.

2026-09-22 (live session, later): Three failed launches traced by bisect
to the new path-lookup cache (removed) and then a crash in Movement off
traced through the dev channel's last request to the bridge's struct write
(now in place). Live results: 10/10 menu stress, Cheat Menu audit 31/33
(both fails were audit bugs), Movement on/off clean, 0 hitches in 60 s of
play with core 129 µs mean, logo/banner/notice strip on screen. Four live
core switches in one session without a crash. Open: physical controller
navigation report from the user, beacon travel re-attach, cheats-on row,
then rc2 tag and ZIPs.

2026-09-22 (hitches, UX, Cheat Menu): User still sees random drops after the
controlled pair, plus Cheat Menu failures and no logo. Found and fixed: the
5 s status write did an fsync on the game thread (now a background writer,
D10); `find()` uncached on the per-frame input path; global script hook
took a mutex on every Blueprint call (lock-free pre-filter). Cheat Menu:
"Property is missing" case bug broke No cooldown; one refused toggle threw
away the whole Apply (now per feature, D11); seal cheats arm instead of
failing (D12). UI: CSSX logo/banner (D13), performance row and line, notice
strip for unapplied edits, no mouse-glyph placeholder before bindings load.
Host tests 4/4 incl. the writer. Dev build staged; `work/live/on_launch.sh`
runs hitch attribution, 10-cycle stress, the full `cheat_audit.py` and
screenshots at the next launch. Next: read those results, then re-tag and
rebuild the release ZIPs.

2026-09-22 (rc1): Tag `v1.0.0-cssx-rc1` built from a clean worktree into
`extensions/core/dist/v1.0.0-rc1/` (both ZIPs verified, SHA-256 sidecars). The
exact release DLLs are staged at the next game exit for the in-game check.

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
