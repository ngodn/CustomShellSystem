# CINE status

Read this first after any break. Design: `DESIGN.md` (same folder).

## Where the work is

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
| next | `cssx.py --cine` staging, `cssx_release.py` CINE package, unreleased notes, decision D15 |

## Regression (2026-10-03, on `feature/cine`)

- CSS host 14/14 (`build/cine-host`). CSS Windows build: only the 2 baseline warnings;
  `css_core.dll` exports `css_customize_abi` and `css_customize_v1`.
- CSSX host 6/6 incl. `cssx_cine` and `cssx_cine_director` (`build/cine-cssx-host`).
  CSSX Windows build 0 warnings, `cine.dll` built (`build/cine-cssx-win`).
- Release packaging dry run: the CINE zip verifies (manifest, hashes, PE check).

## Next

1. Live test with the user. Ask before installing anything: CSS core (beta.9 build),
   CSSX core + loader, and CINE via `cssx.py stage --cine`.
2. Live checks: Enter/leave restores HUD and camera; record a route; capture a final
   shot; fashion walk with the palette track; each glide; menu-open cancel; loading
   screen mid-take; frame stats in Cine World idle vs. mid-take.
3. VERSION bumps (CSS 1.0.0-beta.9, CSSX 1.3.0) only in a release-prep commit when the
   user asks to release.
