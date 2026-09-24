# CSSX Teleport: build status

Built and installed the whole extension tonight. It is loaded and healthy in your
running game right now (hot-swapped in on the released CSSX 1.1.0 core), and it
persists across a restart because the files are staged in
`Mods/CSSX/extensions/eins0fx.teleport/`.

## What it is

A native CSSX extension, `eins0fx.teleport`. On the world map you hover a point and
a Teleport prompt appears; press `T` (keyboard) or `R3` (right stick) to travel
there with the game's own streaming teleport, so there is no loading screen. Four
settings under the CSSX tab: Map teleport on/off, Confirmation (ask / right away),
Allow locked points, Departure camera (face the character / keep the game camera).

## Verified (safe, non-interactive, on the live game)

- The extension loads with no error, sits in the library alongside cheat-menu /
  performance / ui-kit.
- Settings model renders correctly, an event round-trips, and the four settings
  persist to `CSSX/state/eins0fx.teleport.json`.
- `render()` runs every frame at about 1.5 microseconds on the no-menu path (the
  early return). No cost during gameplay, matches the performance standard.
- The teleport call exists with the exact args I encode:
  `BPFL_WorldStreaming_C::TeleportPlayerWithStreaming(Location, Rotation,
  ControlRotation, ScreenTransitionClass, TransitionZOrder, FadeInDuration,
  FadeOutDuration, OptionalZoneData, __WorldContext)`.
- `GetTeleportManager`, the camera manager's `SetDesiredViewFromDirection`, the
  pawn's `GetActorForwardVector`, `K2_GetActorLocation/Rotation`, and `input.keys`
  for `T` / `Gamepad_RightThumbstick` all resolve and encode/decode correctly.
- The map structure: controller has `Teleport Manager` and `World Map Handler`;
  `World Map Handler.MapActorWidgets` holds the `BPC_MAW_LandingAreaSelector_C`
  components; each has a `MapActorWidget` (the WBP selector with `HasFocus` and
  `GetSelectedLandingArea`). The scan reads exactly these.

## The one thing to check when you are at the keyboard (about a minute)

I could not exercise the actual map hover, keypress, or the teleport fire, because
the game throttles its tick while its window is unfocused (you were away), so the
map's world-space selector widgets never populated or took focus, and I would not
fire a real teleport on your sleeping session. Everything the fire depends on is
verified; the fire itself and the on-map UI need a focused window.

To check: open the world map, hover a beacon/gate/dungeon, and confirm the Teleport
panel appears at the bottom. Press `T` (or `R3`), confirm, and watch the jump. If
anything is off, `CSSX.log` has a one-line `world map open: N map actor widgets`
debug entry each time the map opens, which pinpoints whether the scan sees the map.

Known follow-ups that genuinely need that focused pass (flagged in DESIGN.md):
- The exact "meteor-flash" transition: v1 uses the game's default streaming
  transition (clean, no loading screen). Passing a specific `ScreenTransitionClass`
  to reproduce the Marrow Keep meteor overlay is a live-tuning item.
- The departure camera feel (blend timing). It is best-effort and never blocks the
  teleport; if you dislike it, set Departure camera to "keep the game camera".
- Overlay placement/polish: positioned bottom-centre from the viewport size; exact
  spacing may want a nudge once seen on screen.

## How to iterate

- Edit sources under `extensions/teleport/src/`.
- Rebuild + hot-swap into the running game:
  `python3 extensions/core/tools/cssx.py stage --core-only --teleport`
  then force a rescan by flipping `Mods/CSSX/core.json` between the two staged
  1.1.0 cores (the loader reloads the core and re-scans extensions on that change).
- Live reflection for probing: `cssx.py request '{"op":"engine","request":{...}}'`
  (dev channel is on; `dev/enabled.txt` present).

## Layout

Single source tree at `extensions/teleport/` (not duplicated into
`extensions/core/`, to avoid the divergence the cheat-menu copies have). The CSSX
CMake and `cssx.py` point at it directly. Distribution (`cssx_release.py`
`teleport_files` + a zip) is not wired yet; say the word and I will add it for a
Nexus release.
