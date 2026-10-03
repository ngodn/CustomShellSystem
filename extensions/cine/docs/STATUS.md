# CINE status

Read this first after any break. Design: `DESIGN.md` (same folder).

## Where the work is

- Worktree `CustomShellSystem/work/cine`, branch `feature/cine` (from `0089a27`).
- The main checkout `CustomShellSystem/` (branch `main`) is never edited for CINE.
- Nothing is pushed or merged. The user decides when.

## Baseline (before any change)

- CSS host tests 14/14 pass; CSSX host tests 4/4 pass.
- Windows builds: CSS has 2 pre-existing warnings (extension_data.cpp unused `row`,
  UE4SS USMapGenerator switch); CSSX has 0. New code must add none.

## Done

| Commit | What |
| --- | --- |
| `a3392ba` | CSS: `apply_customize` / `restore_customization` shared by the request path; exports `css_customize_v1` / `css_customize_abi`; 25 new unit checks; CSS 14/14 |
| `3381ef0` | CSSX: `css.customize` host op (per-call module lookup, hot-swap safe), ABI doc; CSSX 4/4, 0 warnings |

## In progress

- CINE extension sources under `extensions/cine/src`: `shot` (route, orbit curve, wall profile),
  `preset` (format, validation, timeline) written; still to write: rig, director, look,
  guides, extension/menu, main, presets, CMake wiring, tests.

## Next

1. CINE rig/director/look/guides/extension + `menu.json` + `extension.json` + presets.
2. CMake target `cine` + logic library + `cine_tests` (mock host, Cheat Menu pattern);
   `cssx.py` build/stage flag `--cine`; `cssx_release.py` packaging.
3. Full regression: CSS 14 + CSSX suites + new tests; Windows builds, no new warnings.
4. Version notes: CSSX 1.3.0, CSS 1.0.0-beta.9 (unreleased, in release notes).
5. Live test with the user (ask before installing any DLL).
