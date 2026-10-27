CINE 2.0.0 for CSSX
===================

Cinematic camera for showcases, inside the game's own Photo Mode. CINE moves the camera and
the character, and can render a take straight to a video file.

Requirements: CSSX 1.3.2 or newer. The look track and the music scenes also need Custom
Shell System 1.0.5 or newer; without CSS the camera works and the look track is off.

Install
1. Close the game.
2. Put the eins0fx.cine folder into ue4ss/Mods/CSSX/extensions/.
3. Put the CSSX_CINE_eins0fx_P folder into MortalShell2/Content/Paks/~mods/.
   It holds what the breakdown draws with and the mixer a render records music through.
4. Scenes come in packs, one folder each (CINE_Walks_eins0fx_P, CINE_IrisOut_eins0fx_P and
   so on). Put the packs you want into MortalShell2/Content/Paks/~mods/ as well.
   CINE without any pack shows no scenes.

Updating from 1.x: replace the eins0fx.cine folder and the CSSX_CINE_eins0fx_P folder, and
install the scene packs. The scenes that came in the presets and audio folders now come from
the packs; presets you made yourself still work from the presets folder.

Quick start
1. Pause the game, go to System and choose Activate CINE Mode. The game's Photo Mode opens
   on CINE's tabs: Take, Route, Animation, Scene, Look and Render.
2. Take: pick a scene, frame her with the Photo Mode camera, then Roll Take (or F8).
   Photo Mode steps aside while the take runs and comes back when it ends.
3. Walk scenes (Fashion Walk and the like) need a route and a final shot first:
   Route > Record Route closes Photo Mode; walk the path, then stand still for 3 seconds.
   CINE Mode comes back on the Route tab. Frame the end of the walk with the Photo Mode
   camera and choose Use Current View as Final Shot.
4. Close Photo Mode the usual way to leave CINE Mode.

Keys in CINE Mode: F8 roll, F9 cancel, F10 frame guide, F7 save final shot.

Render to video
Render > Render Take to Video rolls the chosen scene and saves it as video.mp4 in
renders/<scene>-<date>/ inside this folder. Every frame is captured at the frame rate you
pick (24, 30 or 60), the scene's music is recorded through CINE's own mixer, and the bundled
ffmpeg encodes them with your graphics card's encoder when it has one. A render takes longer
than the take itself; leave the game window alone until the Render row says Video saved.
Cancel Render stops it and keeps the frames captured so far. The video has the size of the
game window. Music from a .wav or .mp3 file in the presets folder is not recorded: the video
is silent and the status says so.

ffmpeg is FFmpeg (https://ffmpeg.org), an LGPL build by BtbN. Its licence is in
ffmpeg/LICENSE.txt and where to get its exact source is in ffmpeg/SOURCE.txt.

Clean scene
Scene > Hide Characters takes enemies, or enemies and NPCs, out of the picture while you are
in CINE Mode. The world is not paused. Hidden characters are frozen and cannot be hit, new
ones do not spawn, and everyone comes back as they were when you leave. Bosses are left
alone, and it stays off during a boss fight. Keep Lock-On Target leaves the enemy you are
locked on to in the scene; a fight with it is real, it can hurt you.

Breakdown
Scene > Breakdown shows the take in layers like a VFX breakdown reel: clay, wireframe,
skeleton, virtual bones, IK bones, spring bones, sockets, physics bodies, the movement
capsule and the frame passes (base colour, normals, depth and more), wiped in one after
another. "Built-in reel" works on any scene. "From the preset" uses the scene's own steps.
Labels and Arrows name what is on screen. The look track is off while a breakdown runs.

Music scenes
A music scene plays its song from the roll and cuts the camera on the beat while CSS changes
outfits, skins and palettes with the song. Leave Expression on "As set in CSS" (the default),
and set Breakdown to "From the preset" for the scenes that stage the rig. Each scene puts
your own look back when it ends. Scenes need the outfits they show and CSS Shared Assets for
the dances; if an outfit is missing, the look changes turn off and the camera and music
still run.

Your own presets
Presets you write live in the presets folder as .cine.json files; Take > Reload Scenes reads
them again. A broken file is skipped and named in the CSSX log. A preset with the same id as
a pack's scene is replaced by the pack's.

Any preset can carry a breakdown track:
  "breakdown": {"steps": [{"at": 0.1, "layers": ["clay"], "wipe": 1.2}, ...]}
"at" is a fraction of the whole take. Layers: clay, wireframe, skeleton, virtual_bones,
ik_bones, springs, sockets, collision, capsule, base_color, normals, roughness, metallic,
depth, occlusion. An empty list is the final image.

A music scene has "kind": "sequence", a "tempo" (bpm, offset, beats_per_bar), an "end", an
"audio" (a pack's audio id, or a .wav / .mp3 in the presets folder), and lists of "shots" and
"looks" timed in bars from 1. A shot has "from" and "to" poses (azimuth, distance, height,
aim, fov, roll), an "ease", and may "track" a part of her (head, neck, chest, pelvis, knees,
body) with "fit" centimetres of her in frame; "fixed" keeps a blocked shot on its side,
"view" holds the camera you framed. A look step changes "outfit" and "variant", "palette" or
"controls" on its beat, can switch the "idle", show a "title", and (with a preset
"performance") start the dance again.
