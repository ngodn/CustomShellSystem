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

## Checklist

- [ ] 1. Sequence kind: preset format, pure timeline maths, unit tests
- [ ] 2. Rig: camera roll; director: sequence takes (cuts, flows, whips)
- [ ] 3. CSS service: `wear` and the list of wearable outfits; CINE look steps with outfits
      - 5 Oct: service side DONE (`outfits`, `wear`, `describe.busy`; commit 5602759), staged
        as a CSS dev core and timed live. CINE side (look steps, flash) not started.
- [ ] 4. Bar-timed breakdown and look steps; breakdown re-applied after an outfit change
- [ ] 5. The showreel preset itself, timed to the grid above; host tests
- [ ] 6. Live: wear latency, the whole take with the user, timing against the track
- [ ] 7. Docs: README (sequence format), Nexus texts
