# CSSX 1.0.0 and CSSX Cheat Menu 1.0.0, release notes

Draft, updated with every release candidate. Player-facing wording; the
evidence behind each line is in `integration-tests.md` and `performance.md`.

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
