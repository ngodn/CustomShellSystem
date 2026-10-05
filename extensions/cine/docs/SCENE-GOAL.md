# CINE 1.1.0 goal: clean scene, action takes, breakdown takes

READ THIS FIRST after any break or context reset. Then `SCENE-RESEARCH.md` (verified game and
engine names, live tests still open). Update the checklist at the bottom as work lands.

## Where and how

- Main checkout `/home/eins0fx/development/mods/msII/CustomShellSystem`, branch
  `feature/cine-scene` (from `main` 1832bf8). NO git worktrees. Never `git stash`. Check
  `git branch --show-current` before editing; another agent sometimes works in this checkout.
- Commit small and often on this branch. Do not push, merge or release without the user asking.
- Build dirs: `build/cine-cssx-host` (tests), `build/cine-cssx-win` (DLLs). CSS: `build/cine-host`,
  `build/cine-win`.
- Ask before installing anything into the game; the game must be closed for a DLL swap.
  Live probes through the dev channel (`extensions/core/tools/cssx.py request`) need the game
  running and the user told first.
- The user wants ONE goal worked straight through, not phases with approval stops. Ask only
  when the user is needed at the game.
- Standards the user repeats every time: C++ and UE 5.6.1 best practice, no lag, no fps drop,
  every case handled, no regression. CSSX core stays mod-neutral (no mod names in it).
- Writing for the user and players: plain words, no em dashes, changelog leads with the fixed or
  new state, never claim what was not tested.

## What "done" means

All of this in CINE 1.1.0, tested in game by the user, with docs and changelog, ready to release
when the user says so:

1. **Clean scene.** In Cine World, a setting hides enemies (or enemies and NPCs) and stops new
   ones appearing, while the world keeps running. Option to keep the lock-on target. The player
   is safe while it is on. Everything is restored exactly on leaving, on a cancelled take, on
   stop, and nothing is written to the save.
2. **Action takes.** A take kind where the player plays and the camera films the fight: follow,
   duel (player and lock-on target both in frame, orbiting) and shoulder framing; beats (hit,
   parry, kill) can trigger slow motion, a punch-in or a cut; depth of field; shake scale; enemy
   health bars and lock-on marker hidden; the game's own finisher cameras respected.
3. **Breakdown takes.** A breakdown track usable on any take that steps through layers:
   final, clay, wireframe, skeleton, virtual bones, IK bones, collision bodies, capsule,
   sockets, outline, depth, base colour, normals, roughness, with wipes. Plus a layer for CSS
   customization physics and motion (spring bones, chest and glutes first) when the outfit is a
   CSS one (user request, 5 Oct). Works on the player and
   on enemies in view. Ships ALL AT ONCE, including CINE's own small asset pak for wireframe,
   G-buffer passes and wipes (user's decision; no two-step release).
4. Presets for the new kinds, menu controls, README, Nexus description and changelog updated.
5. Host tests (mock host) cover enter/restore paths, interrupted takes, world change, missing
   pieces; full CSSX suite green; Windows build 0 warnings.

## Added by the user on 5 Oct (during the probe)

- Breakdown layer for CSS customization physics / motion (spring bones; chest and glutes
  first) when the outfit is a CSS one.
- Callouts: an arrow and a text label naming each component shown. Plan: CSSX HUD layers
  (text + thin rotated image lines via `set_angle` / `set_pivot`), label slots on the screen
  margins, leader to the component's projected point
  (`WidgetLayoutLibrary.ProjectWorldLocationToWidgetPosition`, a few calls per frame only while
  labels show, at most 6 callouts), plus a caption naming the current layer.

## CSSX core changes on this branch (neutral, no mod names)

- Bridge: `TScriptInterface` arguments from an object handle (needed for
  `AddOrUpdateBlendable`), interface values decode to the object.
- Bridge: `set` takes `"echo": false` (a struct of more than 256 fields failed its echo after
  the write). CINE tolerates the old failure text on CSSX 1.3.0.
- These need a CSSX release (1.3.1) alongside CINE 1.1.0; without it CINE skips buffer passes
  and says why.

## Decisions already made by the user

- Opponent in a clean scene: keep the lock-on target (no spawn list in 1.1.0).
- Breakdown ships with every layer at once, asset pak included.
- The old bug the user remembers showed wire capsules and boxes: confirms shape components
  render in this shipping build.
- Work in a branch; everything as one goal.

## Design in short

- `scene.{hpp,cpp}`: clean scene. Per enemy: the game's own `Pause()`, hit processing off,
  hidden (with attached actors), collision off, `GE_AI_Invisible_C`, `GE_HealthDamageImmunity_C`,
  audio off, cloth suspended; spawn triggers' actor collision off; spawner proxies hidden;
  player `GE_Invulnerability_C` + fall immunity + `GE_Undetectable_C`. Every prior value saved
  and restored in reverse. Work spread over ticks; 0.5 s keep-clean poll; refuse while a boss is
  aggroed or in a cutscene.
- `action` take kind in the director: subject = player, optional target = lock-on; camera from
  CINE's own CameraActor; framing maths in `shot.cpp` (pure, tested); beats by 10 Hz polling;
  slow motion through `WorldSettings.CinematicTimeDilation` (fallback: guarded global dilation).
- `breakdown.{hpp,cpp}`: layers as attach-once engine objects (shape components on bones for
  physics bodies and capsule, meshes on bones for skeleton / VB / IK / sockets), material swaps
  for clay and wireframe, post-process blendables on CINE's camera for passes and wipes. Zero
  per-frame work except one parameter write while a wipe animates.
- Asset pak `CSSX_CINE_eins0fx_P` (`~mods/`), assets under `/Game/CSSX/CINE/` (user's rule:
  CSSX extension assets go under `/Game/CSSX/<EXTENSION>/`, never `/Game/CSS/`):
  wireframe overlay material, flat clay material (skeletal + cloth + morph usage), G-buffer
  viewer post-process with mode and wipe parameters. CINE works without the pak: those layers
  are skipped and the status line says why.

## Checklist (update as you go; newest notes at the bottom of each item)

- [x] 0. Goal file, research file, branch committed
- [x] 1. Live probe session with the user (script `extensions/cine/work/probe/`), results written
      into `SCENE-RESEARCH.md` ("Live probe results"). Only the buffer passes are untested
      (they need the bridge's new interface-argument support, built on this branch).
- [x] 2. Asset pak: materials authored and cooked, pak built, renders in game
      - Pipeline in `extensions/cine/assets/`: `author.sh STAGE` (Linux editor, stage on
        /mnt/eins0fxE/CSS-work/cine-assets/), `cook.sh STAGE COOK` (Wine container; COOK must be
        under /home; about 8 min), `pack.py COOK OUT` (retoc trio `CSSX_CINE_eins0fx_P`),
        `install.py PACK [--remove]`.
      - Run a2 is the current build (Invert/Hold/Show/Ambient switches). Wire, clay, x-ray and
        solid were seen in game with a1. The released trio sits in `assets/pak/` beside
        `pack.json` (hashes committed, binaries git-ignored); the release tool ships it.
- [x] 3. Clean scene: code, host tests
      - `src/scene.{hpp,cpp}`. Player protection only when no target is kept (a kept target
        is a real fight). Lock-on is dropped before its target is hidden
        (`TargetSpecificCharacter(null)`, verified live 5 Oct).
- [x] 4. Breakdown: code, host tests
      - `src/breakdown.{hpp,cpp}`, `src/layers.hpp`, `src/callouts.{hpp,cpp}`. Labels: sockets
        sampled at 10 Hz relative to her, the lens measured against the engine's projection at
        10 Hz, and ONE host call a frame (`GetPlayerViewPoint`); own projection is within about
        1 px of the engine's (verified live 5 Oct, 2560x1440, scale 1.33).
- [x] 5. Action takes: code, host tests, presets (`fight-duel`, `fight-hero`, `fight-shoulder`)
- [x] 6. Menu, settings, README, presets wired; regression (CSSX suite 8/8, CSS suite 18/18,
      Windows build without warnings in CINE and CSSX)
      - Director tests use a modelled world (`tests/director_tests.cpp`, `Host::populate`) and
        compare every value before and after: 131 checks. Pure tests: 680 checks.
      - CSS: `css.customize describe` lists `nodes` for physics controls (additive; lands in
        the CSS release after 1.0.0-beta.11, CSS VERSION not touched here).
      - CSSX 1.3.1: bridge interface arguments and `"echo": false` (decision D16).
- [ ] 7. Installed and tested in game with the user; fixes
      - To check live: frame passes (first time on screen; Post vs Pre, Gain/Gamma), wipes with
        the a2 switches, label positions on the HUD, health bar and lock marker hidden in a
        fight take, slow motion feel, clean scene with real spawners.
- [ ] 8. Nexus description, changelog, STATUS; version 1.1.0 (manifest, README and release
      notes are done)
- [ ] 9. Release only when the user says so
