# Mark of the Beast: the film (goal, 26 Oct 2026)

Read this first when picking the work up. The request: a cinematic, epic, sensual scene for
Profane Proxima and Unholy Genessa on the whole of Polyphia's "Mark of the Beast" (4:29),
presented as **Mark of the Beast, a Mortal Shell II series by _eins0fx**. The spoken intro
(0 to 27 s) is the only text narrated, as animated words over the game's own Proxima and
Genessa shells (nothing else in the song is words); the
instrumental intro after it carries the title. Use every CSS animation that fits, the game's
own moves where they can be reached (kneel, block, parry, riposte), the user's favourites
(After Brute Battle, Eve Default Idle), expressions on both faces, and the lore of each woman
and her place in the world.

## The song (work/beast/analyze.py, subs.srt from the visualizer)

269.35 s. The groove is 92.3 BPM (a beat every 0.65 s); the intro feels double-time, 150 to
165 s is double-time, 196 to 210 s is the loudest part. Structure boundaries (librosa, 8 s
scale): 27.0, 95.9, 122.8, 132.8, 164.7, 177.7, 209.7, 231.6, 246.6; finer: 41.9, 51.9, 67.9,
88.9, 150.8, 186.7, 196.7, 253.6. The only words in the song are the speech, 0.4 to 26.9 s;
the visualizer's auto-captions also put labels on accents at 41.2, 49.5, 74.1, 92.6, 126.6,
138.2, 151.7, 175.4, 190.3, 197.8, 202.7, 209.2, 224.4 and 245.8, which are not lyrics (they
are sound chops) and are only used as cut points. A bell at 29.5 s. The drop-out at 246.6 to
253.6 is near silence; the ending 253.6 to 269 fades.

The speech (faster-whisper small.en and medium.en over the visualizer's captions; "perdition"
corrected from the caption's "predition"):

| from | words |
| --- | --- |
| 0.54 | This criminal, this arch-criminal, has many aliases. |
| 3.88 | He's known not only as the Beast, |
| 5.82 | he is known as the Man of Sin. |
| 8.32 | He is known as the Son of Perdition. |
| 10.80 | He's known as the Antichrist. |
| 13.92 | We're getting closer. |
| 16.64 | And the closer to him, the faster things are going to happen. |
| 24.32 | The faster... deliverance. |

On screen the speech is made hers (the user's call): "She's known not only as the Beast, she
is known as the Woman of Sin. She is known as the Daughter of Perdition. She's known as the
Antichrist. ... And the closer to her, the faster things are going to happen."

Reviews place the sample in a 1990s public-access TV segment (a man cursing the devil in the
name of Jesus); the names are from 2 Thessalonians 2:3 and Revelation. The track is read as
"a song for the bad guys", which is the film's angle: the two women as the Beast's marks.

## The lore (the game's own text, work/research/locres)

- **Proxima, the Broodseeker.** Shell tagline: "Purpose gives the soul its shape." A
  knight in grafted armour with a horned helmet, found lying on a table in the Shattered
  Beacon (Blackridge Cliffs, Fainweald). Ability Biosampler: a hook fired from her left arm
  pulls enemies or herself into range, then a strike that inflicts Lightning; "The Biosampler
  does not stab." Passive Grafted Armor. Her memory: "You may call me Proxima." "My grafted
  form prevents intoxication." "Where I am from... my grafted armor would be reclaimed."
  Merrick: "All you gotta do is bring Proxima back to life." Shades: Ivory Proxima, Obsidian
  Proxima, Bonegrafted.
- **Sester Genessa.** The returning Sester of Fallgrim Tower, now a shell in the Marrow Keep
  arena. "I am Sester Genessa." "Let Sester Genessa take control over you." Faithful Doubles
  (spirits that strike and vanish), Duality: when her health is gone she becomes Stray
  Genessa instead of being severed. Names in the game: Undermether Genessa, Dream Genessa,
  Young Genessa, Genessa Prime / Secundus / Tertius / Decima, Genessa's Censer, "Genessas (in
  unison)". Shade: Obsidian Genessa.

Places: Proxima's scenes at the **Shattered Beacon** (or Blackridge Pass around it);
Genessa's at **Marrow Keep** (the arena, the lantern). The user places her; CINE films.

## What can move her

- CSS shared library (63 entries; all on both outfits): idles (Eve Default, Seated Recline,
  Kneeling Bow, Low Lean, Deep Kneeling Bow, Reclining, Forward Fold, Side Kneel), dances
  (Bass Down Low 12.7 s, Black Hole 20 s, Come Closer 19.3 s, Jang Won Young Spin 3.7 s,
  DumbDumb, Iris Out 16.7 s at 136 BPM, Cinderella), catwalks (Eve, Adam, Lily, 12 s),
  performances (ATOOL 1 to 8, 2 to 6.7 s; Performance 801 18.85 s; Gesture 200 0.4 s; After
  Brute Battle 13.67 s), 30 photo poses (held).
- Expressions: 20 on Profane Proxima (css.profaneproxima.face.*), 21 on Unholy Genessa
  (css.unholygenessa.face.*): angry, contempt, daydream, gloomy, happy, kiss, laugh, neutral,
  oops, puffed, sad, serious, sigh, smile, sob, sulk, surprised, tease-ya, wink, zzz (+
  look-down on Genessa). Confirm live: the 23 Oct game reported none for Profane Proxima on
  Shared Assets 1.0.3; they are in the 1.0.4+ manifest.
- The game's own montages, on SKEL_Human_Skeleton (they play on CSS outfits in gameplay, so
  they play here): Kneel 4.27 s, Kneel loop 8.5 s (loops), KneelDown 4.27, Sitting idle 10.87,
  Reach Out 1.6, Use Mask 4.77, Axeatana perfect block 2.23, seal parry 1.44 / 1.6, riposte
  1.9, Slayer Seal charge 1.67, Saved By The Shell 2.07, Proxima's Biosampler Start 0.6 /
  Pull 0.86 (loops) / Stab 3.03. Paths under /Game/Sparta/Characters/Shells/_Shared/Animation/
  (Actions, Parry, SlayerSeal, Abilities) and .../Shells/KnightLady/Animation/Ability/.
  CINE plays them with ACharacter::PlayAnimMontage through a new look-step field `montage`.
- After Brute Battle: a GPU crash was reported once near the end of the clip on Unholy
  Genessa when it looped as an idle (docs/development/idle-repeat-crash-audit.md). As a
  one-shot performance it does not loop; the camera cuts away before its last second anyway.
- Not used: Genessa's Stray form switch (css genessa_form; needs the Genessa shell, interrupts
  the performance layer) and the real Harden ability (a GAS ability with a stone-skin cue):
  both are candidates for a later version.

## CINE additions for this film (sequence look steps)

- `"expression": "<id>"`: applies a facial expression on the beat (restored with her look).
- `"performance": "<id>"` (a string, beside the existing true/false): starts that CSS
  performance on the beat, stopping the one playing; `"performance_rate"` optional.
- `"montage": "<asset path>"` with `"montage_rate"`: plays the game's own montage on her;
  `"montage": ""` stops it. Stopped at the end of the take whatever happened.

## The takes (tools/mark_of_the_beast_film.py)

One preset per take, each with its own slice of the song (tools/mark_of_the_beast_audio.py),
so a retake is one take. Proxima's takes in the KnightLady shell at the Shattered Beacon,
Genessa's in the Genessa shell at Marrow Keep; the user switches shells between the two
groups. The two intro takes are shot with CSS on **Default** (the game's own shell), the rest
with the outfit worn. The film is assembled in post (tools/mark_of_the_beast_post.py): the
narration, the title cards, and the intercut of the climax.

| take | song | who | what |
| --- | --- | --- | --- |
| beast-p0 | 0 to 54 | original Proxima, Profane Proxima from 41.2 | narration; kneels on the bell at 29.5 (CSS Kneeling Bow) under the title; wears Profane Proxima on her name slam |
| beast-g0 | 0 to 54 | original Genessa, Unholy Genessa from 49.5 | the same, intercut; wears Unholy Genessa on her name slam |
| beast-p1 | 54 to 96 | Profane Proxima | After Brute Battle cut on the bar with whips; the Eve Catwalk from 67.7 with the glow cooling; ATOOL 7 at 80; the Forward Fold on the silver skin at 86.7; an 80 degree orbit on the loud bridge at 88.9; the Reach pose on the accent at 92.6 |
| beast-g1 | 95.9 to 150.8 | Unholy Genessa | After Brute Battle cut on the bar; Come Closer through the quiet from 109.6, mist, the hood off; an orbit on the section change at 122.8; Performance 801 from 125.4 through the intercut; ember on the accent at 138.2; ATOOL 3 at 143.4 |
| beast-p2 | 132.8 to 177.7 | Profane Proxima | DumbDumb against Genessa in the intercut (a bar each) on the bronze skin; the starlight skin on the double-time at 150.8: block, parry, riposte, Slayer Seal, Biosampler on its beats; then the breath: Reclining in the Human skin, cameras above the grass, daydream, kiss |
| beast-g2 | 177.7 to 209.7 | Unholy Genessa | the Adam Catwalk through the build; Black Hole from 186.7; wings on the accent at 190.3; gold on the loudest section at 196.7 with the camera cut on the half-bar; glow 8 on the accent at 202.7; the Hush pose |
| beast-p3 | 209.7 to 246.6 | Profane Proxima | intercut with g3 a bar each: the Slayer Seal on the hit; Bass Down Low at 214 cut on the bar; the Spin at 226.7; the Lily Catwalk at 231.6 cut on the bar through the loud outro; Natarajasana at 243.6; the Confident pose on the last accent |
| beast-g3 | 209.7 to 246.6 | Unholy Genessa | ATOOL 6 on the hit; Iris Out at 0.679 rate from 214 cut on the bar; Cinderella in gold through the loud outro from 238.6; the Charm pose at 243.6; a laugh on the last accent |
| beast-p4 | 246.6 to end | Profane Proxima | the drop-out on her face (contempt, smile), the ending from the ground; from 4:15.26 the Seated Recline (the user's choice) |
| beast-g4 | 246.6 to end | Unholy Genessa | the same, intercut, the Seated Recline from 4:15.26; end card |

Screen time, solo sections: Proxima 54 to 95.9 and 150.8 to 177.7 (68.8 s), Genessa 95.9 to 132.8 and
177.7 to 209.7 (68.9 s); 132.8 to 150.8 and 209.7 to 246.6 are intercut a bar each, the ending too.

Second pass (26 Oct, after the user's review of the first film): Eve Default Idle is only ever the
pose she returns to; every section has a clip running (catwalks, Come Closer, DumbDumb, Cinderella,
ATOOL 3, 6 and 7 joined the dances and the hunt). The camera follows the song's energy map
(work/beast/energy.txt, librosa loudness, low band and onset density per section): the loud parts
cut on the bar, the loudest (196.7 to 209.7, -3.8 dB, the heaviest low band) on the half-bar with
whips and roll; the quiet parts (164.7 to 177.7, the drop-out) keep long moves. Profane Proxima's glow strength (`outfit_glow`, 0 to 8, default 1) is set
on every body skin through `GLOW` in the film tool, tuned live.

Live findings (26 Oct, Genessa batch at Marrow Keep): the game's Kneel montage is a kneel-and-rise
action (4.3 s) and Kneel Loop does not hold the pose, so the intros kneel with the CSS Kneeling
Bow idle instead; the Seated Recline lies her back with the legs toward the capsule's rear, so
"from behind" looks along her body: its shots are a profile, her face from above and a close
profile. After Brute Battle moves the capsule itself, so tracked shots now anchor on her live
position (CINE change), or the camera stays where she started. The montages themselves play
their full length through CINE (checked with GetCurrentMontage).

Framing rules as before: the body from behind, front and side shots on the face and neck.
Every take restores her look and idle. Live checklist: expressions present on both outfits;
montage plays (Kneel) and stops; a dance at rate 0.679; the two intros on Default.

Customization in takes (26 Oct, second pass): the user saw their CSS settings missing from the
recordings (glow strength, glow fill, flow speed, motion travel, shape sliders). A take's wear
carried only the outfit and variant, and CSS resolves such a wear from its remembered
customization per outfit and variant: her own skin came up right, a skin she never customized
(Profane Proxima's Human skin) came up on package defaults. CINE now keeps, per outfit, the
customization a look opened with in the take (hers for her own outfit, the first-seen one for
any other) and sends it with every wear; CSS keeps what the variant can take. Verified live on
beast-p4: the Human skin carries glow 5.25, fill 0.65, flow 0.46 and the shape sliders, and her
own look is restored after the take.
