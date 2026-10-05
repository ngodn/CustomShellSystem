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
   sockets, outline, depth, base colour, normals, roughness, with wipes. Works on the player and
   on enemies in view. Ships ALL AT ONCE, including CINE's own small asset pak for wireframe,
   G-buffer passes and wipes (user's decision; no two-step release).
4. Presets for the new kinds, menu controls, README, Nexus description and changelog updated.
5. Host tests (mock host) cover enter/restore paths, interrupted takes, world change, missing
   pieces; full CSSX suite green; Windows build 0 warnings.

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
- [ ] 1. Live probe session with the user (script `extensions/cine/work/probe/`), results written
      into `SCENE-RESEARCH.md`
- [ ] 2. Asset pak: materials authored and cooked, pak built, renders in game
      - 5 Oct: pipeline proven offline. `extensions/cine/assets/`: `author.sh STAGE` (Linux editor,
        stage on /mnt/eins0fxE/CSS-work/cine-assets/), `cook.sh STAGE COOK` (Wine container; COOK
        must be under /home, e.g. `extensions/cine/work/export-cook/<run>`; about 8 min),
        `pack.py COOK OUT` (retoc trio `CSSX_CINE_eins0fx_P`). Run a1: cook exit 0, 7 packages,
        decoded: flags and parameters as authored. NOT yet seen in game.
      - Still to add before the final cook: an `Invert` scalar on every material (outgoing layers
        show where the wipe has NOT passed), then gamma/gain defaults from the live look.
- [ ] 3. Clean scene: code, host tests
      - 5 Oct: `src/scene.{hpp,cpp}` written and compiling (not wired into the director, no
        tests yet). Probe script ready: `extensions/cine/work/probe/probe.py all`.
- [ ] 4. Breakdown: code, host tests
- [ ] 5. Action takes: code, host tests, presets
- [ ] 6. Menu, settings, README, presets wired; regression (CSSX suite, Windows build)
- [ ] 7. Installed and tested in game with the user; fixes
- [ ] 8. Nexus description, changelog, release notes, STATUS; version 1.1.0
- [ ] 9. Release only when the user says so
