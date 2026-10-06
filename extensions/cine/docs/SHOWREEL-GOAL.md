# CINE showreel preset: goal and plan

Read this first after any break. It continues `SCENE-GOAL.md` (CINE 1.1.0) on the same branch,
`feature/cine-scene`, in the main checkout. No worktrees. Nothing is pushed or released without
the user asking.

## What the user asked (5 Oct 2026, evening)

Reference: `docs/reference/XG - SOMETHING AINT RIGHT Official Music Video.mp4`.

- "is it possible for us to have new preset like the cinematic sensual camera with vfx breakdown
  also follow that music? u can watch it and also listen to understand the beat, from 0 sec to
  01:21, so we demo our unholy genessa, commander white, and eve, with all the outfit and
  palette cycles and everything customization run in background"
- Correction, after I misread it: seconds 0 to 19 of the video are for LEARNING the shot
  concept. The scene the user wants runs from 0:00 to about 1:20 of the track. "u learn the
  whole 1:20 la better."
- Same evening, for every take: a signature block (`_eins0fx`, then "Custom Shell System (CSS)
  + Extensions (CSSX) CINE + Unreal Engine 5", then `github.com/ngodn/CustomShellSystem`), a
  "tech cool" label font, and everything laid out inside whichever frame guide is picked.
  Those three are DONE (see "Done so far").

CINE cannot play music. The take is timed to the track's beat grid; the user lays the track
over the recording in their editor, starting at take time 0.

## What the video does (measured, `work/export-ref/`)

- Tempo 124 BPM (123.99 measured): beat 0.4839 s, bar 1.9355 s. Bar 1 starts at 0.87 s; the
  drop is bar 10 at 18.29 s. Bar n starts at `0.87 + (n - 1) * 1.9355`.
- Phrases are 4 bars (7.74 s). Boundaries: bar 10 at 18.29, 14 at 26.0, 18 at 33.9, 22 at 41.7,
  26 at 49.4, 30 at 57.0, 34 at 64.9, 38 at 72.5, 42 at 80.2 (the end of the scene).
- 0 to 18.3 s, bars 1 to 9: ONE continuous shot. Static wide establishing (2.5 s), a figure
  drifts through frame, then the camera flows from subject to subject: whip to a face close-up
  with a slow arc (6.4 to 8.3), whip to two full-body figures with a close pass-by (8.8 to
  10.7), wide group (11 to 13.6), low angle (14.4), then a symmetric push-in on the last
  subject from full body (15.2) through medium (16.0) to a face close-up (17.6), speeding up
  into the cut on the drop. Slow drift, fast whip with blur, settle: every landing is a new
  subject in a new framing.
- 18.3 to 49.4 s, bars 10 to 25: four solos, one per 4-bar phrase, each a new look in a new
  place. Per solo: face close-up, wide (walk toward camera or locked off), a 1.5 to 3 beat
  detail insert (feet, wheels), then a low-angle arc or an orbit from behind. Cuts sit on bar
  lines and half bars; a few-frame flash marks each phrase boundary.
- 49.4 to 80.2 s, bars 26 to 41: chorus. Wide group shots of 6 to 12 beats alternate with
  singles of 1.5 to 3 beats (low angles, dutch tilts), quickest around bars 32 to 35, then a
  long 12-beat shot and a locked, symmetric wide for the last 2 bars.

## The scene for CINE (plan)

One take, 80.2 s, one character, three looks: Unholy Genessa, Commander White, Eve
(`CSS_UnholyGenessa_eins0fx_P`, `CSS_CommanderWhite_eins0fx_P`, `CSS_EveStellarBlade_eins0fx_P`).
A look change takes the place of the video's change of member, hidden in a whip or on a cut.

| Bars | Seconds | What happens |
| --- | --- | --- |
| 1 to 9 | 0.9 to 18.3 | The oner. Wide establishing, slow drift. Whip, Unholy Genessa: face close-up, slow arc. Whip, Commander White: full body, close pass-by, low angle. Whip, Eve: symmetric push-in from full body to face, speeding up into the drop. |
| 10 to 13 | 18.3 to 26.0 | Unholy Genessa solo: face, wide, body-glide inserts (chest, hips), orbit from behind. Palette changes on bar lines. Breakdown wipes: clay, wireframe. |
| 14 to 17 | 26.0 to 33.9 | Commander White solo: same shot family, new angles. Breakdown: skeleton, virtual and IK bones, spring bones. |
| 18 to 21 | 33.9 to 41.7 | Eve solo. Breakdown: physics bodies, capsule. |
| 22 to 25 | 41.7 to 49.4 | Customization run: controls and palettes change on every beat on one look, camera on slow inserts. Frame passes: base colour, normals, depth. |
| 26 to 33 | 49.4 to 64.9 | Chorus: wides of 6 to 8 beats, singles of 2 beats, a look change on every phrase and palette changes on bars. |
| 34 to 41 | 64.9 to 80.2 | Fastest intercut (looks and palettes every bar or half bar, dutch low angles), one long 12-beat orbit, then a locked symmetric wide with the signature for the last 2 bars. |

## The three looks (read from the installed packages' manifests, 5 Oct)

The user: "for each unholy genessa, eve, cw, u learn that whole outfit and customization first
so u know what to do for the scene", and CSS may be changed for it ("if needs to update CSS
please go ahead").

All three wear on the Genessa shell (Commander White on nothing else). All three carry the same
eight idles, set with CSS's `animation_choice`: `eve` (default), `eve_idle_702` seated recline,
`703` kneeling bow, `704` low lean, `705` deep kneeling bow, `706` reclining, `708` forward fold,
`713` side kneel. Another agent is porting one more idle (the user, same evening).
All three have the six figure shapes (`shape_FBMBodyTone`, `shape_PBMBreastsSize`,
`shape_PBMGlutesSize`, `shape_PBMHipSize`, `shape_PBMThighsTone`, `shape_PBMWaistWidth`, 0 to 1)
and motion controls (`chest_motion`, `glute_motion`, `thigh_motion`, `belly_motion`; hair too
on Commander White and Eve).

| Look | Outfit id, variants | Palettes | What to show |
| --- | --- | --- | --- |
| Unholy Genessa | `eins0fx.unholygenessa`: `eve` (Eve skin), `genessa` (Genessa skin) | mist, ember, tar, gold, moon | Pieces: `toggle_bodice`, `toggle_skirt`, `toggle_wings`, `toggle_veil`, `toggle_hood`, `toggle_crown`, `fabric_visible`. Glow: `outfit_glow` (0 to 8, default 4.4), `outfit_glow_fill`, `outfit_glow_color`, `eye_glow`, `eye_mist`. `fabric_opacity` (sheer, 0 to 1). Colours: fabric, veins, edge, crown, hood, eye_color. |
| Commander White | `eins0fx.commanderwhite` (0.0.6-dev): `original` | crimson, midnight, emerald, amethyst, obsidian | Fifteen modular pieces: `coat`, `top`, `skirt`, `leotard`, `lace`, `collar`, `belt`, `glove_left`, `glove_right`, `boot_left`, `boot_right`, `hair_band`, `ornament_left`, `ornament_right`, `outfit_visible`. Colours: `uniform_fabric`, `ruffle_fabric`. Hair motion. |
| Eve | `eins0fx.seduxtress` (1.4.0): `black_pearl`, `prototype`, `skin_suit`, `bikini`, `casual_sweater`, `midsummer_alice` | five or six per variant (black_pearl: moonstone, garnet, abyss, jade, obsidian, rose_quartz) | Six whole outfits to cut between, each with its own pieces (suit, stockings, heels, tail, ribbons, glasses...) and colours; skin and hair colour. |

What that suggests for the scene: Unholy Genessa is the glow and sheer-fabric look (pulse
`outfit_glow` on the beat, sweep `fabric_opacity`, drop wings / veil / hood / crown on half
bars); Commander White is the modular one (pieces come off and on with the beat); Eve is the
wardrobe (a new variant on each bar of her phrase). The customization run uses the figure
shapes and palettes on one look. Idles change on phrase boundaries, hidden by a cut.

Summary with every control: `work/export-ref/outfits/SUMMARY.md` (git-ignored; rebuild with
`build/repak/release/repak unpack <pak>` and read `manifest.json`).

## What CINE needs for it

1. **Sequence takes** (`"kind": "sequence"`): a list of shots on a tempo grid (`bpm`, `offset`,
   times in bars and beats). A shot is a camera move from one pose to another (azimuth,
   distance, height, aim, fov, roll, optional f-stop) with an ease; it starts on a cut (camera
   placed at once) or flows on from the shot before (the oner, whips). Arcs are driven per tick
   like the dolly is now, so they stay arcs.
2. **Look steps that change the outfit**: CSS's `css.customize` service gets a `wear` action
   (outfit, variant; CSS already has `selected_outfit` / `apply_pending` for its own page) and a
   way to list what the worn shell can wear. Steps on bar and beat times. Looks are worn once
   while the take prepares, so their assets are loaded before the camera rolls.
3. **Breakdown and look steps at bar times** (they are fractions of the take today).
4. **Camera roll** (dutch tilt) in the rig.
5. The user's own look is saved before the take and put back after it, as the look track
   already does.

Measured live (5 Oct, 22:37, CSS dev core with `wear`): a look lands 0.2 to 0.5 s after the
call, and CSS's apply is one long frame on the game thread: 125 to 450 ms the first time a
look is worn, 12 to 190 ms after that. So:

- a look change is never mid-shot. It sits under a **flash** (the video marks its phrase
  boundaries with a few-frame flash too): a full-frame HUD layer fades in over two frames,
  `wear` is called, the flash holds until `describe` says the look has landed, then fades out.
  The frozen frame is under the flash.
- the take wears each look once while it prepares (first-time cost out of the recording).
- the user's own look goes back with one `wear` carrying the `customize` snapshot,
  `persist: true` (verified: customize identical afterwards).

Still open: whether a breakdown survives an outfit change (clay and wireframe are per mesh
part and the locker holds the old look's materials: a breakdown must end before a `wear` and
begin again after it, or look changes and material layers must not share a phrase).

The other agent's animation library (`ANIMATION-PORT-GOAL.md`, same checkout, same evening)
is reworking CSS's animation catalog. Idle changes in the scene wait for that; do not touch
CSS's animation code meanwhile, and commit only my own files (never `git add -A`).

## Done so far (this evening, on `feature/cine-scene`)

- Label style: Rajdhani SemiBold (OFL, `fonts/`), upper-case letter-spaced readouts, accent
  bar, shelf, arrow. The font is a TTF made into a font object at runtime (`Typeface`).
- Signature block from `signature.txt`, toggle "Signature" in the Shot section.
- Frame-aware layout: labels, caption and signature sit inside the chosen frame guide's
  opening and scale down for narrow frames (unit tests cover all five guides).
- CSSX 1.3.1: screen-level HUD root kept alive across garbage collections, camera pose in the
  frame, viewport in HUD units, `"resize": true` for array writes.
- Seen in game at 22:14: labels, caption, signature and font all draw. Not yet seen: 9:16 and
  the other guides in game; the a3 pak (bone rim shading), which needs one game restart.

## State at the end of 5 Oct (23:20)

BUILT, host-tested and run live three times (`work/live/reel.py showreel-124 preset` rolls it
and proves her look and materials are as before; the last two runs: "RESTORED"):

- `src/sequence.{hpp,cpp}`: tempo grid, shots (cut / flow, eases, roll, fstop), look steps,
  `ShotPlan` / `choose_plan` / `planned_pose`.
- Director (`cine.cpp`): `prepare_looks` (every look worn once, breakdown built on its look,
  opening look), `plan_surroundings` (every shot traced at five moments for eight angles; a
  chain of flows is judged and turned as one), `sequence_frame` (frame clock, cuts on their
  beat, look steps, transitions), `sequence_tick` (camera per tick plus one live trace),
  `roll` (music, real-time clock while it plays), own look put back in `finish_take` and
  remembered in `own_look_owed_` until CSS confirms it.
- Outfit changes ride the beat under a frame-pass wipe (normals, base colour in turn): one
  beat in, one beat held, one beat out starting on the step's beat; half-beat wipes when
  changes come every bar. The user rejected the first version, a white flash ("ugly"), and
  asked for "smooth transition that also follows the beat" using our frame effects.
- `src/audio.{hpp,cpp}`: `"audio": "audio/x.wav"` in a preset plays through winmm. The
  reference track's first 81 s sit ONLY in the game's CINE folder (`audio/showreel.wav`);
  never commit or ship it.
- CSS service additions (dev core staged): `outfits`, `wear`, `describe.busy`, `idle`
  (one transient idle across looks; preset field `"idle"`, the showreel uses `eve`).
- `presets/showreel-124.cine.json`: 42 shots, 43 look steps (19 wears), breakdown on bars
  22.5 to 29.5. It was generated from a Python table in the session, then edited; the JSON
  is the source now.

Lessons that cost a broken character each (both fixed, both in `SCENE-GOAL.md` notes too):
the breakdown must read her materials when clay comes, not while preparing (CSS makes new
instances per wear and blocks every change when it finds foreign ones); and a take must not
capture "her own look" while a restore is still landing.

The user's judgement of the latest run (transitions, music sync, camera in the open spot,
idle) is the next input. Likely tuning: shot framing per look, which pieces and palettes
change when, transition lengths.

## Checklist

- [x] 1. Sequence kind: preset format, pure timeline maths, unit tests
      - `src/sequence.{hpp,cpp}`: `Tempo` (times in bars, counted from 1), `SeqShot` (cut or
        flow, from/to `SeqPose` with roll, ease incl. whip, fstop), `SeqLook` (outfit +
        variant, palette, controls, flash), `Sequence::parse`, `wardrobe()`, `shot_at`,
        `pose_at`, `flash_alpha`. Preset `"kind": "sequence"` with `tempo`, `end`, `shots`,
        `looks`; breakdown steps of a sequence are in bars. Tests in `cine_tests.cpp`
        (`sequence()`), 1314 checks.
      - NEXT (item 2): the director. Decided: the take clock of a sequence runs on frames
        (render), so a cut lands within a frame of its beat, and the camera is placed at once
        on a cut and driven per tick in between, like the dolly. Prepare order: save her look,
        wear every look of `wardrobe()` once, end on the look the breakdown belongs to, build
        the breakdown, wear the opening look, roll. All breakdown steps must sit in one span
        without a wear (pieces and locked materials belong to one look). The flash is a
        full-frame HUD layer on the screen root.
- [x] 2. Rig: camera roll; director: sequence takes (cuts, flows, whips)
- [x] 3. CSS service: `wear` and the list of wearable outfits; CINE look steps with outfits
      - 5 Oct: service side DONE (`outfits`, `wear`, `describe.busy`; commit 5602759), staged
        as a CSS dev core and timed live. CINE side (look steps, flash) not started.
- [x] 4. Bar-timed breakdown and look steps (the breakdown belongs to one look; a preset that
      changes outfit under it is refused)
- [x] 5. The showreel preset itself, timed to the grid above; host tests (161 director checks,
      3009 pure checks)
- [ ] 6. Live: wear latency (done), the whole take with the user (three runs done), their
      verdict and tuning (open)
- [ ] 7. Docs: README (sequence format), Nexus texts

## 6 Oct, 00:00 to 01:20: what the user decided, and where it stands

The user watched every run and corrected the cut several times. What holds now:

- **No separate breakdown block, no normals or depth.** The frame is split for almost the whole
  film (not the intro, not the outro): mesh and rig on the left, the finished look on the right.
  "Why dont wireframe side by side is like almost whole video except intro and outro."
- **The split sits on the middle of her body**, not of the screen (`"split": "body"`): the
  director projects pelvis, chest and head each frame and moves the line there.
- **Everything on the left the whole time** (wireframe, skeleton, spring bones, IK and virtual
  bones) with the labels and arrows on throughout. Do not switch layers in and out on a
  schedule: the user found every timed version repetitive or meaningless.
- **Context.** Every change has a title ("Palette  //  Crimson", "Pieces  //  Coat off") and the
  camera shows the thing that changes. Titles are short and plain: the user rejected "Under the
  mesh" and "The rig" as AI wording; the section is titled "Rig".
- **Commander White release film** (the user's release video for that mod): only the white
  (`original`), obsidian and crimson palettes; no custom colours; figure sliders are never
  touched ("muscles etc default size"); motion presets Natural on the chest and Soft on glutes,
  thighs and belly; lace is toggled only in a neckline shot whose frame ends above the bust
  line, top only from behind. Written by `tools/commander_white_film.py`, which also writes
  the 9:16 twin (`"frame": "9:16"`, close shots further back).
- **Walk version** (`commander-white-walk`, kind walk): needs the user's route and final shot.
  Walk takes now carry titles, a body-following split and a look track beside a breakdown
  without clay. Host-tested only.

Built for it (all host-tested, 8 suites): breakdown across outfit changes (bone groups per set
of looks, `learn` / `assemble` / `adopt`), split line that rests and follows, overlay-only clay,
materials kept alive between uses, helper bones not drawn, covers as layer sets, look-step
`title` / `cover` / `idle` / `reset` / `physics`, preset `frame`, fixed label rows in a split,
take picture setting (shadow rays and dither; `work/live/quality_probe.py` is the evidence).

Track facts for the second preset (`showreel-finale`, audio from 141.0 s of the reference with
a 1.2 s fade in, `audio/showreel-finale.wav` in the game folder only): 124.000 BPM, no drift.
The audible downbeats are 130 ms later than the grid used here (bar 1 at 1.000 s, not 0.87);
the video's own cuts sit about three frames ahead of the beat, which is what 0.87 gives.
From 141.0: bridge from +1.29 s, break at +14.84, drop at +16.77, full groove from +18.71,
last phrase from +40.0, music stops dead at +47.64, silence from +50.

**GPU crash, 01:10.** The game died with "GPU Crash dump Triggered" a minute into a take. No
stack in CINE or CSS. The take picture setting then also raised the virtual shadow page pool
(512 to 4096 pages, back at take end), on every take that night. That part is removed; only
ray count and dither remain. Not proven to be the cause.

Seen in game before the crash: the split on her body, titles, all layers left, rig cover for
outfit changes, her look restored after every take. NOT seen yet (written after the last run
that showed the current cut): the neckline shot for lace, the motion section, fixed label
rows, the 9:16 twin, the walk preset, the reduced picture setting.

Also that night: a probe script of mine started a take in the user's game by being imported
(`reel.py` had no main guard). Fixed; never import a script that drives the game.

## 6 Oct, 01:20 to 01:40: after the relaunch, verified in game

Rolled the Commander White film five times (wide and 9:16) and read every frame of each.

- **Camera.** The user: "camera movement masih banyak yang patah2, i want yang mulus, CINEMATIC
  SENSUAL". Two causes found and fixed. (1) A sequence camera was steered from the 10 Hz tick
  with engine moves, ten straight segments a second; it is now placed every frame
  (`sequence_frame`), and a run of flowing shots with `"ease": "smooth"` is one Hermite
  spline through its poses. (2) The live obstacle trace hit her own hand or sword and pulled
  the camera to 20 percent of its distance at once (seen at 78.9 s: the camera 80 cm from her
  in the outro). Hits within 75 cm of the aim point are now her own and ignored, and the
  pull-in eases over about half a second instead of snapping.
- **The film is shot as long glides**, not cuts: one slow move for the intro, one arc for the
  palettes, one pass down her body for the pieces (each comes off as the camera reaches it),
  half circles for dressing again and for the rig, one slow move in for the outro.
- **Frame times of a whole take** (loader ring, 86 s, no screenshots): 47.8 fps, median
  20.7 ms, p95 25.6, p99 28.2, worst 55 ms, 8 frames over twice the median. CINE's share:
  core tick 0.70 ms a frame on average, of which the per-frame camera placement is 0.11 ms.
- **Arrows.** IK and virtual bones are not shown: their markers float beside the body and a
  label for them points at air. Three layers on the left: wireframe, skeleton, spring bones.
  Labels sit in fixed rows; a spring label points at whichever bone of its pair is on the
  layers' side; a target right of the line gets no label.
- **Lace**: seen at 9:16 and wide, the frame ends above the bust line while it is off. The
  camera aims at a height above her capsule, so when her idle bows her head the frame shows
  more hair, never more chest. The top is not toggled at all.
- **Motion section removed**: a title over a standing pose showed nothing moving. The presets
  (chest natural, rest soft) are applied at the opening; the walk preset applies them too.
- Low Lean replaced by a two-bar Seated Recline (user). In 9:16 it is shot from her feet.
- No crash in six takes with the reduced picture setting.

Still not rolled in this form: Showreel 124, Showreel Finale, Commander White Walk (needs the
user's route).
