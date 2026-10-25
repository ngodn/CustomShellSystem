# CINE status

Read this first after any break. Design: `DESIGN.md` (same folder).

## CINE 2.0.0 (built 25 Oct 2026, not yet on Nexus)

`cine-v2.0.0` is a local tag (not pushed). Release files: `extensions/core/dist/cine-v2.0.0/`
(zip with the extension, camera presets and the breakdown pak; changelog, description, short
description, images from the `docs/cine-2.0.0-media` branch). Music scenes ship as one pack per
song, built by `tools/scene_packs.py` into `extensions/core/dist/cine-scene-<slug>-v1.0.0/`:
Iris Out, Something Ain't Right, They Know Not What They Do. The tracks live in the git-ignored
`audio/` folder. Needs CSSX 1.3.1 and CSS 1.0.5 (palette changes keep the idle, takes keep saved
looks). Scenes need Expression on "As set in CSS"; their look changes run whatever the Look track says.

The three fight presets are held back from 2.0.0 (the code stays). In the live demo on 25 Oct
the user found player-driven fight takes awkward, and two bugs showed: a death mid-take ends
with "the target is down" (the target's handle expires on the world change), and the duel
camera opened behind tree branches.

### Next: Fight Replay (queued)
The user's design: the player fights normally with the game camera while CINE records; then
CINE replays the fight and the cinematic camera films it, any preset, any number of times.
- Unreal's replay system is a dead end here. `demorec` works in shipping (UWorld::Exec, not
  guarded) and recorded a 678 KB replay, but `demoplay` rebuilt an empty world with no pawn:
  MS2 is single player (characters and animation are not replicated) and its World Partition
  world is not rebuilt. `demostop` then loads the main menu. Do not retry it.
- The way forward is CINE's own recorder: sample the player and the target each frame
  (transform and bone pose, natively, not through per-bone bridge calls), then hide them and
  drive posable copies (same mesh and materials) in the live world. Hit effects, blood and
  sound need recording and re-firing separately. Research the game's character and animation
  setup first (CXXHeaderDump, Sparta.hpp).

## CINE 1.1.0 (in progress, 5 Oct 2026)

Branch `feature/cine-scene` (from main `1832bf8`). Clean scene, fight takes and breakdown
are written, wired and host-tested; versions are CINE 1.1.0 and CSSX 1.3.1. The goal,
decisions and checklist are in `SCENE-GOAL.md`, the verified game facts in
`SCENE-RESEARCH.md`. Open: the live test with the user (checklist item 7), then the final
Nexus texts (drafts are in `docs/nexus/cssx-cine/` and `docs/nexus/cssx/`).

- Installed in the game for that test (5 Oct, 19:13): pak a2, `cssx_core-1.3.1-dev`,
  CINE 1.1.0, and a CSS dev core from this branch. The previous CSS core pointer
  (`css_core-1.0.0-beta.11.dll`) is backed up in `extensions/cine/work/export-backup/`;
  to go back, write that name into `Mods/CustomShellSystem/core.json` with the game closed.
- Live drivers: `extensions/cine/work/probe/probe.py` (engine facts) and
  `extensions/cine/work/live/drive.py` (roll takes, screenshots, cost).

## Where the work was (1.0.0)

- The main checkout `CustomShellSystem/`, branch `feature/cine` (from `0089a27`).
- No git worktrees for this project (the user's rule). Check the branch before editing.
- Nothing is pushed or merged. The user decides when.
- Build dirs: `build/cine-host`, `build/cine-win`, `build/cine-cssx-host`, `build/cine-cssx-win`.

## Baseline (before any change)

- CSS host tests 14/14 pass; CSSX host tests 4/4 pass.
- Windows builds: CSS has 2 pre-existing warnings (extension_data.cpp unused `row`,
  UE4SS USMapGenerator switch); CSSX has 0. New code must add none.

## Done

| Commit | What |
| --- | --- |
| `a3392ba` | CSS: `apply_customize` / `restore_customization` shared by the request path; exports `css_customize_v1` / `css_customize_abi`; 25 new unit checks; CSS 14/14 |
| `3381ef0` | CSSX: `css.customize` host op (per-call module lookup, hot-swap safe), ABI doc; CSSX 4/4, 0 warnings |
| `93289ef` | CINE: `shot` (route, orbit curve, wall profile) and `preset` (format, validation, timeline) sources |
| `218de14` | CINE: shipped presets, head/capsule reference, CMake wiring, 392 logic checks |
| `d954a1c` | CINE: rig, director, guides, menu, ABI 3 entry, 42 mock-host director checks (caught and fixed the record-from-outside bug) |
| `c3ad159` | `cssx.py --cine` staging, `cssx_release.py` CINE package, notes |
| next | **Redesign (user rejected CSSX knowing CSS):** neutral service bus. CSSX `service.list` / `service.call`, `include/cssx/service.h`, `src/runtime/services.cpp` + 49 bus checks + header-copy check; `css.customize` op removed from CSSX core. CSS publishes `css.customize` via its own `cssx_services()` export (`native/src/cssx_service.h`, `docs/services.md`). CINE calls `service.call`. Decision D15 rewritten. |

| `242b592` | Live test 1: Enter failed ("player is not fully loaded"). `class_default` takes a class name, not a path; fixed, mock hardened to match the bridge. Live-probed every read-only call CINE makes at resolve: all good. Traverse "No zone data on The Marrow Keep" is older (since 1.0.1, 28 Sept), not this branch. |

| fixes before release | Look track defaults to Off (shipped presets have no look steps; "From the preset" now says so). The look is restored after a world change or a refused end-of-take restore: retried 1/s for 120 s, new takes wait for it. 51 director checks. Still CINE 1.0.0 (never released before); released as tag `cine-v1.0.0`. |

## Rule learned

CSSX core must never know any mod by name. Mods talk through the service bus (D15).
Earlier rows mentioning `css_customize_v1` and the `css.customize` op are superseded.

## Regression (2026-10-03, after the redesign)

- CSS host 14/14 (`build/cine-host`); Windows build no new warnings; `css_core.dll`
  exports `css_get_api`, `css_get_api2`, `cssx_services`.
- CSSX host 7/7 incl. `cssx_services`, `cssx_cine`, `cssx_cine_director`
  (`build/cine-cssx-host`); Windows build 0 warnings (`build/cine-cssx-win`).

## Game install state

- CSS: the beta.9 build with the OLD exports is installed (backup of the previous CSS
  folder: `backups/1791026653460406353`). Needs reinstall with the service build.
- CSSX: rolled back to 1.2.1 (loader `cc71868b`, core `9221b2d8`), CINE removed.

## Next

1. Live test with the user. Ask before installing anything, game closed: `tools/css.py
   install` (service build), then `extensions/core/tools/cssx.py stage --cine`.
2. Live checks: Enter/leave restores HUD and camera; record a route; capture a final
   shot; fashion walk with the palette track; each glide; menu-open cancel; loading
   screen mid-take; frame stats in Cine World idle vs. mid-take.
3. VERSION bumps (CSS 1.0.0-beta.9, CSSX 1.3.0) only in a release-prep commit when the
   user asks to release.
