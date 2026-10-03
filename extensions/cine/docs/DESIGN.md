# CINE design (eins0fx.cine)

CINE is a CSSX extension that turns any character into a cinematic showcase that a modder
records with their own tool (OBS, ShadowPlay, Steam). It ships camera presets as data files,
records a walking route, captures the modder's own final framing, and can animate CSS looks
(palettes and outfit pieces) during the shot while keeping every other customization value.

Branch: `feature/cine`. Targets CSSX 1.3.0 (extension ABI 3, menu schema 2) and CSS
1.0.0-beta.9 (new optional customize export). Everything in this document was proven live on
2026-10-02/03 with the external rig in `work/cinematic-20261002/` before it became a design.

## Goals and rules

- No cost outside Cine World: `tick` and `render` return at once.
- In Cine World, coarse work only: a few cached bridge calls per 10 Hz tick, no per-frame
  bridge traffic, no per-frame allocation (CSSX performance standard rules 1-6).
- Everything CINE changes is owned and restored: view target, HUD visibility, force walk,
  speed limit, movement block, path following, spawned cameras, CSS customization.
  Restore runs on Exit, Cancel, `stop()`, world change, pawn change and errors.
- Never touch objects CINE did not create (no "find all cameras and destroy").
- Fail visibly: every refusal reaches the menu as an `error` or a `notice`.

## Components

### CSS: customize export (native/)

- `css::apply_customize(options, custom, command)` in `controls.cpp` (css_data, unit-tested):
  the `palette`, `control`, `reset_control`, `color`, `reset_color` branch of
  `Core::request`, moved verbatim so the request path and the export share one
  implementation. `css::restore_customization(options, json)` parses a snapshot and keeps
  only values compatible with the outfit (`compatible_values`).
- `Core::customize_api(json)`: actions `describe` (shell, outfit, variant, palette,
  customize, palettes, controls) and `apply` (`commands` list: palette, control,
  reset_control, restore; applied to a copy, validated, then committed the way
  `Core::request` commits a control change, so CSS's normal apply pass picks it up).
- Export `css_customize_v1(json, size, sink, ctx)` and `css_customize_abi()` from the core
  DLL. Refuses when no core is live, when called during CSS's own tick (re-entrancy from a
  CSSX hook), or off the game thread. Never writes `message` (read by the ImGui thread).

### CSSX: `css.customize` host op (extensions/core/)

- In `Core::service` beside `menu.status`. Finds the CSS core module per call
  (`EnumProcessModules`, path under `Mods/CustomShellSystem/cores/`), re-resolves the export
  every call because the CSS core hot-swaps, forwards `request`, returns its JSON.
- Errors: "CSS is not installed", "CSS is too old for CINE looks (needs 1.0.0-beta.9)",
  plus CSS's own refusal text. Documented in `docs/abi.md`; CSSX 1.3.0.

### CINE extension (extensions/cine/)

| File | Role |
| --- | --- |
| `src/main.cpp` | ABI 3 table, `noexcept` trampolines (Traverse pattern) |
| `src/cine.hpp/.cpp` | Extension: settings, menu model/events, status, stop |
| `src/director.cpp` | State machine: Idle, Preparing, Countdown, Running, Restoring |
| `src/rig.cpp` | Engine side: cameras, view target, HUD, walk, steering, restore |
| `src/shot.hpp/.cpp` | Pure maths: route, orbit keys, Catmull-Rom, wall profile, lead |
| `src/look.cpp` | CSS look track through `css.customize` |
| `src/preset.cpp` | Preset files (`presets/*.cine.json`), validation |
| `src/guides.cpp` | HUD layers: frame guide (16:9, 9:16, 1:1, 2.39:1), countdown |

Pure maths and presets live in a static library tested off-game with a mock host, like the
Cheat Menu.

## Shot model (what the presets describe)

- **Route**: recorded at 10 Hz from the player's own walk ("Record route"), decimated to
  ~1.2 m, longest walking run kept, branched into the final spot with a cubic curve so she
  arrives already facing the final direction (no turn on the spot).
- **Final shot**: the modder's game camera captured relative to her ("Use current view as
  final shot"); played on a camera attached to her (she is still by then).
- **Orbit keys**: (time, azimuth, distance, height, aim height, FOV), interpolated in orbit
  terms with one Catmull-Rom curve so the camera never stops between shots and arcs around
  her. Azimuth only increases (full 360 for Fashion Walk).
- **Wall profile**: traced once while Preparing, 20 traces per tick so preparing never
  hitches; the camera eases in where a wall is close and never flips to the other side.
- **Camera rig**: one unattached CameraActor moved by `KismetSystemLibrary.MoveComponentTo`
  with a fixed latent UUID, retargeted every tick to the pose 0.8 s ahead. The base point is
  her real position plus the route displacement over the lead (follows curves), the height
  is the route's own height (smooth through stairs), the frame turns with her smoothed
  facing. Filters clamp dt to 0.12 s so a slow frame never makes the camera leap.
- **Walk**: `BPFL_Player.SetForceWalk` before the take (the game picks the walk gait from
  its MovementGait tag, not from speed), `SimpleMoveToLocation` chasing a goal two
  waypoints ahead, control rotation steered to the velocity, start facing the path, ease
  in from 70 cm/s, slow over the last 2 m.
- **Look track**: steps of (time, palette, control overrides) applied as one atomic
  `css.customize` apply each. The modder's snapshot is taken on Enter and restored on every
  exit path. The Original palette is never used mid-shot (it clears every value); the
  modder's own look is rebuilt from the snapshot instead.

## Presets shipped

`fashion-walk-360`, `pose-glide-3`, `pose-glide-5`, `pose-glide-7`, `pose-glide-10`,
`turntable`, `photo-orbit`. Each is a JSON file a modder can copy and edit.

## Menu (CSSX tab "CINE")

- **Cine**: status, Enter Cine World, Exit, countdown on/off and seconds.
- **Shot**: preset, duration (for glide and turntable), zoom, height, FOV scale, frame guide,
  pull in at walls.
- **Route**: Record route / Stop, route summary, Use current view as final shot, Clear.
- **Look** (shown when CSS answers): animate look on/off, look sequence (from the preset or
  "cycle palettes"), Restore my look now.
- In Cine World: F8 / D-pad up starts a take, F9 / D-pad down cancels or exits,
  F10 / D-pad right toggles the guide.

## Exit and failure paths

| Event | Response |
| --- | --- |
| Exit / Cancel / `stop()` | Restore all, view back to the player |
| World generation changes (loading) | Drop handles, restore what is still valid |
| Pawn changes or dies | Abort take, restore |
| Game menu opens during a take | Abort take, restore |
| CSS missing or old | Look track off with a notice; camera still works |
| Route missing | Fashion Walk disabled with the reason |
| Bridge call fails | Abort take, restore, show the error |

## Tests

- css_controls: `apply_customize` and `restore_customization` against the existing recipe.
- CINE logic: route decimation and branching, Catmull-Rom continuity, azimuth unwrapping,
  wall profile hold and smoothing, lead along the route, preset validation and limits,
  director transitions and the restore list with a mock host.
- Regression: full CSS (14) and CSSX (4) host suites, Windows builds with no new warnings.
