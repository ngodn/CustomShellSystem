CINE 1.1.0 for CSSX
===================

Cinematic camera for showcases. CINE moves the camera and the character; you record with
your own tool (OBS, ShadowPlay, Steam recording, gpu-screen-recorder).

Requirements: CSSX 1.3.1 or newer. The look track also needs Custom Shell System
1.0.0-beta.9 or newer; without it the camera works and the look track is off.

Install
1. Close the game.
2. Put the eins0fx.cine folder into ue4ss/Mods/CSSX/extensions/.
3. Put the CSSX_CINE_eins0fx_P folder into MortalShell2/Content/Paks/~mods/.
   It holds the materials the breakdown draws with. Without it CINE still works, and a
   breakdown can only show physics bodies and the movement capsule.

Quick start
1. Open CSSX (F6) and go to CINE.
2. Route > Record route. Walk the path you want, then stand still for 3 seconds.
3. Stand her where the walk should end, frame her with the game camera, then use
   Route > "Use the current view as final shot" (or F7 in Cine World).
4. Cine World > Enter Cine World. Pick a preset, start your recorder, press F8.

Keys in Cine World: F8 roll, F9 cancel or leave, F10 frame guide, F7 save final shot.
The D-pad is not used: it uses items in Mortal Shell II.

Clean scene
Clean scene > Hide takes enemies, or enemies and NPCs, out of the picture while you are in
Cine World. The world is not paused. Hidden characters are frozen and cannot be hit, new
ones do not spawn, and everyone comes back as they were when you leave. Bosses are left
alone, and it stays off during a boss fight. "Keep my lock-on target" leaves the enemy you
are locked on to in the scene; that fight is real, it can hurt you.

Fight takes
The "Fight" presets film a fight you play yourself. Lock on to an enemy, roll the take and
fight. The camera frames both of you, a kill plays in slow motion and a parry or a hit
pushes the camera in. The take ends when the target is down, at F9, or at the preset's
time limit. "Fight: Hero Orbit" also works with nobody locked.

Breakdown
Breakdown > Breakdown track shows the take in layers like a VFX breakdown reel: clay,
wireframe, skeleton, virtual bones, IK bones, spring bones, sockets, physics bodies, the
movement capsule and the frame passes (base colour, normals, depth and more), wiped in one
after another. "Built-in reel" works on any preset. "From the preset" uses the steps in
the preset file (see Breakdown Turntable). Labels and arrows name what is on screen.
The look track is off while a breakdown runs.

Presets
Presets live in the presets folder as .cine.json files. Copy one, change it, then
Shot > Reload presets. A broken file is skipped and named in the CSSX log.

A fight preset has "kind": "action" and an "action" block: framing (duel, follow or
shoulder), azimuth, orbit_speed, distance, distance_max, margin, height, aim, fov, smooth,
shake, fstop, slowmo, slowmo_hold, tail, game_cameras (respect or override) and beats
(on: hit, hurt, parry or kill; do: slowmo or punch; cooldown).

Any preset can carry a breakdown track:
  "breakdown": {"steps": [{"at": 0.1, "layers": ["clay"], "wipe": 1.2}, ...]}
"at" is a fraction of the whole take. Layers: clay, wireframe, skeleton, virtual_bones,
ik_bones, springs, sockets, collision, capsule, base_color, normals, roughness, metallic,
depth, occlusion. An empty list is the final image.
