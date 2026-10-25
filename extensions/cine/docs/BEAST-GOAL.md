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
| beast-p1 | 54 to 96 | Profane Proxima | After Brute Battle on the groove's entry, then Low Lean and Side Kneel with the glow cooling; ATOOL 7 at 80; the Reach pose on the accent at 92.6 |
| beast-g1 | 96 to 150.8 | Unholy Genessa | After Brute Battle in the arena, like Proxima; the quiet on her knees from 109.6 (Deep Kneeling Bow, look-down); rises into Performance 801 at 123; ember on the accent at 138; ATOOL 3 at 142 |
| beast-p2 | 150.8 to 177.7 | Profane Proxima | the hunt: block, parry, riposte, Slayer Seal, Biosampler on the double-time beats; then the breath: Reclining in the Human skin, cameras above the grass, daydream, kiss |
| beast-g2 | 177.7 to 209.7 | Unholy Genessa | the build and the loudest part: wings on the accent at 190.3, Black Hole from 186.7, glow 8 on the accent at 202.7, the Hush pose at the end |
| beast-p3 | 209.7 to 246.6 | Profane Proxima | the fast section and outro, intercut with g3: Bass Down Low at 214, Spin, Lily Catwalk at 231.6, the Confident pose on the accent at 245.8 |
| beast-g3 | 209.7 to 246.6 | Unholy Genessa | Iris Out at 0.679 rate (136 BPM danced at 92.3) from 214, then the Charm and Hush poses |
| beast-p4 | 246.6 to end | Profane Proxima | the drop-out on her face (contempt, smile), the ending from the ground; from 4:15.26 the Seated Recline (the user's choice) |
| beast-g4 | 246.6 to end | Unholy Genessa | the same, intercut, the Seated Recline from 4:15.26; end card |

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
