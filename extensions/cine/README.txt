CINE 2.0.0 for CSSX
===================

Cinematic camera for showcases. CINE moves the camera and the character; you record with
your own tool (OBS, ShadowPlay, Steam recording, gpu-screen-recorder).

Requirements: CSSX 1.3.1 or newer. The look track and the music scenes also need Custom
Shell System 1.0.5 or newer; without CSS the camera works and the look track is off.

Install
1. Close the game.
2. Put the eins0fx.cine folder into ue4ss/Mods/CSSX/extensions/.
3. Put the CSSX_CINE_eins0fx_P folder into MortalShell2/Content/Paks/~mods/.
   It holds the materials the breakdown draws with. Without it CINE still works, and a
   breakdown can only show physics bodies and the movement capsule.

Updating from 1.0.0: replace the eins0fx.cine folder and add the CSSX_CINE_eins0fx_P folder.
Presets you made yourself: copy them back into the new presets folder. When you update CINE
later, keep the presets and audio folders, or extract your scene packs again.

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
are locked on to in the scene; a fight with it is real, it can hurt you.

Breakdown
Breakdown > Breakdown track shows the take in layers like a VFX breakdown reel: clay,
wireframe, skeleton, virtual bones, IK bones, spring bones, sockets, physics bodies, the
movement capsule and the frame passes (base colour, normals, depth and more), wiped in one
after another. "Built-in reel" works on any preset. "From the preset" uses the steps in
the preset file (see Breakdown Turntable). Labels and arrows name what is on screen.
The look track is off while a breakdown runs.

Music scenes
Scene packs are separate downloads, one per song. Each one unzips into this folder: its
presets go into presets/ and its music into audio/. A scene plays its music from the roll
and cuts the camera on the beat while CSS changes outfits, skins and palettes with the song.
Leave Expression on "As set in CSS" (the default), and set Breakdown track to "From the
preset" for the scenes that stage the rig. Each scene puts your own look back when it ends. Scenes need the outfits
they show and CSS Shared Assets for the dances; if an outfit is missing, the look changes
turn off (the status line says why) and the camera and music still run.

Presets
Presets live in the presets folder as .cine.json files. Copy one, change it, then
Shot > Reload presets. A broken file is skipped and named in the CSSX log.

Any preset can carry a breakdown track:
  "breakdown": {"steps": [{"at": 0.1, "layers": ["clay"], "wipe": 1.2}, ...]}
"at" is a fraction of the whole take. Layers: clay, wireframe, skeleton, virtual_bones,
ik_bones, springs, sockets, collision, capsule, base_color, normals, roughness, metallic,
depth, occlusion. An empty list is the final image.

A music scene has "kind": "sequence", a "tempo" (bpm, offset, beats_per_bar), an "end", an
"audio" file in the audio folder, and lists of "shots" and "looks" timed in bars from 1.
A shot has "from" and "to" poses (azimuth, distance, height, aim, fov, roll), an "ease", and
may "track" a part of her (head, neck, chest, pelvis, knees, body) with "fit" centimetres of
her in frame; "fixed" keeps a blocked shot on its side, "view" holds the camera you framed.
A look step changes "outfit" and "variant", "palette" or "controls" on its beat, can switch
the "idle", show a "title", and (with a preset "performance") start the dance again.
