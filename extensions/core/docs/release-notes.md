# CSSX 1.0.0 and CSSX Cheat Menu 1.0.0, release notes

Draft, updated with every release candidate. Player-facing wording; the
evidence behind each line is in `integration-tests.md` and `performance.md`.

## CINE 1.0.1

- The look track starts on Off; Cycle palettes turns it on. The shipped presets have no
  look steps of their own, so "From the preset" says so on screen instead of doing nothing.
- Your own look goes back even when a take is cut off by a loading screen, travel or
  death: CINE retries once a second for up to two minutes until CSS takes it, and a new
  take waits for that first.

## CSSX 1.3.0 and CINE 1.0.0

- New extension, CINE: a cinematic camera for showcase videos. Enter Cine World
  from the CSSX tab, record the walk you want, frame the last shot and press F8.
  CINE flies the camera along a preset (fashion walk 360, pose glides of 3, 5, 7
  and 10 seconds, turntable, photo orbit), walks her at a real walking pace, and
  can cycle CSS palettes on the way. Record it with your own recorder.
- Presets are plain `.cine.json` files in the CINE folder; drop in your own or
  share them.
- Mods can offer services to CSSX extensions without depending on CSSX: a native
  mod exports one function, and extensions find and call it with `service.list` and
  `service.call`. CINE uses CSS's look service this way; CSSX itself knows no mod.

## CSSX 1.2.1 and Traverse 1.0.1

- Traverse passes the destination's zone to the game's streaming teleport, the way
  the game's own beacons and gates do. A traverse used to arrive with the world
  still in the old zone, and a beacon trip taken from there could leave the save
  in a state that crashed on every load (28 September, the Red Keep gate area).
- Traverse refuses to fire while a previous traverse is still arriving, inside a
  dungeon (those leave through the game's own exit path), and for a point whose
  zone cannot be read. Each case says why in the CSSX tab.
- The CSSX tab keeps its place when CCS is installed as well: the Player Menu
  can hold Inventory, Tarstones, Map, CSS, CCS and CSSX, and CSSX stays last.
- A Player Menu that stops taking input recovers on its own: when the game's
  UI.Input.Block.All tag has sat on the player for two seconds with the menu
  open, CSSX strips it and logs a warning (a Nexus report of a menu that needed
  Alt+F4 on first use; cause not yet known, the log will say).
- The CSSX page always has a way out: if Enhanced Input never answers, the
  game's default menu keys apply after 1.5 s (Escape or B closes).

## CSSX 1.2.0

- The CSSX page is built from Mortal Shell II's own menu widgets, like the
  CSS tab: the Change Shade list rows and category headers, the options
  menu's selector and slider rows, the Inventory details window with its
  prompt list, the Inventory's tab strip with edge fades, and the game's
  confirmation dialog. Fonts, frames, glyphs and highlights are the game's.
- Section tabs scroll instead of truncating; the selected one slides into view.
- Every control's value shows under its name in the list; the game's E badge
  marks toggles that are on and extensions that report themselves active.
- No hitches while navigating: widgets are pooled and only what changed is
  written. Moving the selection costs a fraction of a millisecond instead of
  rebuilding the page (12 to 30 ms before). The page warms up while the
  Player Menu is open on another tab, so the CSSX tab opens ready.
- Menu scale (Settings) now scales the whole page like the game's menus do.
- Extensions need no change: the extension ABI, the menu schema and every
  request operation are unchanged. CSSX Cheat Menu 1.1.1 and Traverse 1.0.0
  run as shipped.

## CSSX 1.0.0

- CSSX is its own UE4SS mod at `ue4ss/Mods/CSSX/`. It does not need CSS and
  works next to CSS v1.0.0-alpha.1 without touching it.
- The menu is a tab inside the game's Player Menu (after CSS, before
  TARSTONES). Open it with F6, both thumbsticks, or by switching tabs. Pause,
  cursor, HUD and key prompts are the game's own.
- Extensions describe controls; CSSX draws them: library, sections, detail
  pane, confirmation for anything that touches the save, a searchable picker,
  a notice strip for pending edits, and a settings page with a live
  performance readout (CSSX's own milliseconds per frame).
- Performance: no per-frame object scans; no disk access on the game thread
  (a background thread writes status and logs); the cost is measured and
  shown, not promised. Frame statistics with hitch attribution are available
  in developer mode.
- Extension ABI 3 (JSON request bridge, hooks, HUD) loads ABI 1 and 2
  extensions unchanged; Lua extensions run in a sandbox.
- Migration tool for installs that used the old CSSX inside
  CustomShellSystem, with a verified backup and a one-command restore.

## CSSX Cheat Menu 1.0.0

- Every cheat is a draft until you press Apply settings; Discard drops edits;
  Turn off all cheats restores everything the menu changed.
- Apply is per feature: one failing cheat reports why and the rest go live.
- Perfect parry, perfect guard and perfect harden stay armed until the
  matching seal (Infinite, Untarnished, Vatra's) is equipped, then act.
- No ability cooldown works on every ability, including those without a
  stone-form cooldown field.
- God, auto heal, infinite resolve, movement multiplier, shell points, shell
  powers (Genessa clones, Smert stance, Lazlo shockwaves), resources, unlocks,
  pickups, Tarstones, shell switch, shortcuts and intro-lock recovery.
- Anything that changes the save asks for confirmation and says so.

## CSSX Performance 1.0.0

- Live graphics settings inside the CSSX tab for A/B testing: the ten
  scalability groups, screen percentage, motion blur, depth of field,
  volumetric fog, Lumen async compute and a frame cap. Each change applies
  at once through the engine console; the status line shows the frame rate
  and CSSX's own cost; Restore puts the game's values back; "Re-apply at
  launch" keeps your choices.
- Lua extension, no native code. Works on CSSX 1.0.0.

## Known limits

- Developer core hot reload is a developer path; players restart the game.
- ABI-2 minimap operations are not provided (the Compass/Minimap extension
  needs its own port).
