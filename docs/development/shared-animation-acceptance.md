# Shared animation acceptance

Current private candidate: `work/cine-animation-port/shared-contact-trial-v1`.
It is built and verified offline, **not installed or accepted for release**.
This page describes the current candidate. Earlier checkpoints remain in Git
history and the linked investigation records.

## Current counts

| Content | Packaged | Acceptance still needed |
| --- | ---: | --- |
| Idles, including Eve Default Idle and seven CSS poses | 8 | CSS/CINE gameplay and restoration |
| Photo poses | 31 | CSS/CINE gameplay and restoration |
| Dances | 7 | Complete temporal/source review and live checks |
| Other performances | 11 | Complete temporal/source review and live checks |
| Catwalks | 3 | Complete temporal/source review and live checks |
| Walk, Jog and Sprint choices | 3 | Matched stopping-transition and native gameplay checks |
| Eve facial expressions | 21 | Six-outfit face playback/restoration |
| Commander White facial expressions, including Neutral | 9 | Face playback/restoration |
| Facial performances per supported face rig | 6 | Full temporal and live checks on Eve and Commander White |

There are 63 animation entries, five outfit adapters, four native locomotion
alternatives and 99 cooked exports. The extra export since the previous
98-export candidate is an owned animation compression setting. No animation
entry was added or removed by that correction.

Full custom-rig coverage is Eve's six outfits, both Unholy Genessa faces and
Commander White. Default-game-human coverage is only Eve Default Idle, Walk,
Jog and Sprint. Old Seductress, GeneXXa/Nunexxa and other custom rigs are excluded.

## Full cooked-frame comparison

All original 57 clips now pass at every source-key time: 6,945 frames and
2,694,660 local bone transforms. The independent ACL decoder uses runtime
linear sampling and compares against fresh Unreal RAW poses with retargeting
disabled. Limits remain 0.05 cm translation, 0.25 degrees rotation and 0.001
scale distance. This replaces the older five-samples-per-clip evidence.

The first full comparison passed 55 clips. Black Hole and After Brute Battle
exceeded the rotation limit on `VB lowerarm_r`, with quaternion distances
0.00246522 and 0.00255662. Isolated compression-only copies use an owned ACL
threshold of 0.001 cm. Fresh editor comparison confirms every authored key
is unchanged. Both copies now pass, with maximum quaternion distances
0.000219304 and 0.000298369. Their original assets and failed reports remain.

The preceding precision pack replaces only those two animation exports, adds the
settings asset, and preserves the other 96 exports byte-for-byte. Both facial
resources are unchanged. Stable IDs, adapters, profiles and native alternatives
are retained. Independent container readback passes. Actual CSS combined and
pack-only loader tests pass 114,262 and 114,250 checks.

The preceding pack additionally replaces Spin with its verified neck correction.
It preserves the other 98 exports byte-for-byte and reuses the existing owned
compression setting. Entry/export counts remain 63/99. The same combined and
standalone loader checks pass on this new pack. Both facial resources, all
adapters and the native alternatives remain unchanged.

Spin's neck roll pinched the target collar despite matching its source motion.
The correction edits only `neck_01`, `neck_02` and `head` rotations. Two separate
editor runs verify all 111 raw frames, with exactly unchanged translations,
scales and other local rotations. Cooked comparison passes at every source key.
All 371 joints outside the corrected neck/tie branch preserve their world poses
within 0.000874 cm and 0.022862 degrees; the head differs by at most 0.000393 cm.
The existing adapter evaluation passes 81 cases across all nine requested outfit
meshes. These are offline comparisons, not cloth or live acceptance.

The current pack adds measured contact corrections for Bass Down Low, Black
Hole, Come Closer and Spin. It preserves the other 95 exports byte-for-byte,
including both facial resources, all adapters and native locomotion alternatives.
The four dances retain their source clocks and all non-leg tracks, including
Spin's neck correction. No outfit packages or original animation assets changed.

Independent decoding covers 6,684 poses at 120 Hz. On Black Pearl's neutral
footwear surface, the deepest reference-floor intersections are respectively
0.0382, 0.0023, 0.0412 and 0.0916 cm. This is a sampled geometric measurement,
not acceptance of gameplay placement, morphs or cloth. Full compressed animation
and post-process evaluation passes all 324 existing body/finger/restoration cases
for these four clips across the nine requested outfit meshes. Both host loader
configurations and the candidate-only installation preflight pass.

Evidence under `work/cine-animation-port/`:

- `shared-full-motion-v1`: full original capture, decode and first comparison.
- `shared-precision-v1`: exact authored-copy verification, Windows cooking,
  independent decoding and passing full comparisons for the two replacements.
- `shared-precision-trial-v1/original-clips-full-motion.json`: all 57 passing
  comparisons with report hashes.
- `shared-precision-trial-v1/verification.json` and `host-validation.json`:
  combined pack preservation, round-trip and actual loader checks.
- `spin-neck-ue-v1/`: saved/fresh editor checks, Windows cook, independent
  decoding, full raw/cooked and world-pose comparisons, and nine-mesh adapter
  evaluation. `shared-neck-trial-v1/` is the resulting combined candidate.
- `dance-contact-v3/`: fresh saved-data verification, Windows cook, independent
  key and 120 Hz decoding, footwear measurements, selected close renders and
  the 36 clip/mesh adapter evaluations. Earlier contact iterations remain in
  `dance-contact-v1/` and `dance-contact-v2/`.
- `shared-contact-trial-v1/`: current combined pack, preservation proof,
  updated 57-clip comparison index, host tests and installation preflight.

DumbDumb, Iris Out and Cinderella have their own full cooked-key checks and
contact studies, linked below. Source-key compression fidelity does not prove
between-key contact, simulated cloth, continuous visual smoothness or gameplay.

## Per-clip state

The table lists the original 57 clips. Source assets are beneath `/Game/CSS/`.
All original authoring sources remain. After Brute Battle uses its verified
`Studies/Precision1` copy. Bass Down Low, Black Hole, Come Closer and Spin now
use `Studies/Contact3` copies; Spin also retains its prior neck correction.

| Clip | Stable ID | Authoring source | Cooked comparison | Live acceptance |
| --- | --- | --- | --- | --- |
| Eve Default Idle | `css.eve.idle.default` | `Eve/Anim/AN_I2_Idle` | 211 frames pass | Pending |
| CSS Seated Recline | `css.eve.idle.seated_recline` | `Eve/Anim/Idles/AN_Idle702` | 101 frames pass | Pending |
| CSS Kneeling Bow | `css.eve.idle.kneeling_bow` | `Eve/Anim/Idles/AN_Idle703` | 101 frames pass | Pending |
| CSS Low Lean | `css.eve.idle.low_lean` | `Eve/Anim/Idles/AN_Idle704` | 101 frames pass | Pending |
| CSS Deep Kneeling Bow | `css.eve.idle.deep_kneeling_bow` | `Eve/Anim/Idles/AN_Idle705` | 101 frames pass | Pending |
| CSS Reclining | `css.eve.idle.reclining` | `Eve/Anim/Idles/AN_Idle706` | 101 frames pass | Pending |
| CSS Forward Fold | `css.eve.idle.forward_fold` | `Eve/Anim/Idles/AN_Idle708` | 101 frames pass | Pending |
| CSS Side Kneel | `css.eve.idle.side_kneel` | `Eve/Anim/Idles/AN_Idle713` | 101 frames pass | Pending |
| Bass Down Low | `css.eve.dance.bassdownlow` | `AnimLab/RT_L2_S01A000` | 381 frames pass | Pending |
| Black Hole | `css.eve.dance.blackhole` | `AnimLab/RT_L2_S02A000` | 601 frames pass | Pending |
| Eve Catwalk | `css.eve.catwalk.evecatwalk` | `AnimLab/RT_L2_S03A000` | 361 frames pass | Pending |
| Come Closer | `css.eve.dance.comecloser` | `AnimLab/RT_L2_S04A000` | 581 frames pass | Pending |
| Jang Won Young Spin | `css.eve.dance.jangwonyoungspin` | `AnimLab/RT_L2_S07A000` | 111 frames pass | Pending |
| ATOOL Performance 1 | `css.eve.performance.atool1` | `AnimLab/RT_L2_S11A000` | 121 frames pass | Pending |
| ATOOL Performance 2 | `css.eve.performance.atool2` | `AnimLab/RT_L2_S11A001` | 201 frames pass | Pending |
| ATOOL Performance 3 | `css.eve.performance.atool3` | `AnimLab/RT_L2_S11A002` | 201 frames pass | Pending |
| ATOOL Performance 4 | `css.eve.performance.atool4` | `AnimLab/RT_L2_S11A003` | 61 frames pass | Pending |
| ATOOL Performance 5 | `css.eve.performance.atool5` | `AnimLab/RT_L2_S11A004` | 131 frames pass | Pending |
| ATOOL Performance 6 | `css.eve.performance.atool6` | `AnimLab/RT_L2_S11A005` | 201 frames pass | Pending |
| ATOOL Performance 7 | `css.eve.performance.atool7` | `AnimLab/RT_L2_S11A006` | 201 frames pass | Pending |
| ATOOL Performance 8 | `css.eve.performance.atool8` | `AnimLab/RT_L2_S11A007` | 61 frames pass | Pending |
| Bring It On | `css.eve.photo.bringiton` | `AnimLab/RT_L2_Photo000` | 7 frames pass | Pending |
| Charm | `css.eve.photo.charm` | `AnimLab/RT_L2_Photo001` | 2 frames pass | Pending |
| Clap | `css.eve.photo.clap` | `AnimLab/RT_L2_Photo002` | 7 frames pass | Pending |
| Confident | `css.eve.photo.confident` | `AnimLab/RT_L2_Photo003` | 7 frames pass | Pending |
| Cutie | `css.eve.photo.cutie` | `AnimLab/RT_L2_Photo004` | 7 frames pass | Pending |
| Flow | `css.eve.photo.flow` | `AnimLab/RT_L2_Photo005` | 2 frames pass | Pending |
| Give Me | `css.eve.photo.giveme` | `AnimLab/RT_L2_Photo006` | 7 frames pass | Pending |
| Hand Heart | `css.eve.photo.handheart` | `AnimLab/RT_L2_Photo007` | 7 frames pass | Pending |
| Hush | `css.eve.photo.hush` | `AnimLab/RT_L2_Photo008` | 7 frames pass | Pending |
| Kiss | `css.eve.photo.kiss` | `AnimLab/RT_L2_Photo009` | 2 frames pass | Pending |
| Kung Fu Salute | `css.eve.photo.kungfusalute` | `AnimLab/RT_L2_Photo010` | 7 frames pass | Pending |
| Natarajasana | `css.eve.photo.natarajasana` | `AnimLab/RT_L2_Photo011` | 7 frames pass | Pending |
| Oh My | `css.eve.photo.omg` | `AnimLab/RT_L2_Photo012` | 7 frames pass | Pending |
| Reach | `css.eve.photo.reach` | `AnimLab/RT_L2_Photo013` | 2 frames pass | Pending |
| Salute | `css.eve.photo.salute` | `AnimLab/RT_L2_Photo014` | 7 frames pass | Pending |
| Shutter Chance1 | `css.eve.photo.shutterchance1` | `AnimLab/RT_L2_Photo015` | 7 frames pass | Pending |
| Shutter Chance2 | `css.eve.photo.shutterchance2` | `AnimLab/RT_L2_Photo016` | 7 frames pass | Pending |
| Shutter Chance3 | `css.eve.photo.shutterchance3` | `AnimLab/RT_L2_Photo017` | 7 frames pass | Pending |
| Slav Squat | `css.eve.photo.slavsquart` | `AnimLab/RT_L2_Photo018` | 7 frames pass | Pending |
| Somersault | `css.eve.photo.somersault` | `AnimLab/RT_L2_Photo019` | 7 frames pass | Pending |
| Stretch | `css.eve.photo.stretch` | `AnimLab/RT_L2_Photo020` | 7 frames pass | Pending |
| Sweetie | `css.eve.photo.sweetie` | `AnimLab/RT_L2_Photo021` | 7 frames pass | Pending |
| Thumb Up | `css.eve.photo.thumbup` | `AnimLab/RT_L2_Photo022` | 7 frames pass | Pending |
| Toast | `css.eve.photo.toast` | `AnimLab/RT_L2_Photo023` | 7 frames pass | Pending |
| Victory | `css.eve.photo.victory` | `AnimLab/RT_L2_Photo024` | 7 frames pass | Pending |
| Walk | `css.eve.photo.walk` | `AnimLab/RT_L2_Photo025` | 7 frames pass | Pending |
| Warm Up | `css.eve.photo.warmup` | `AnimLab/RT_L2_Photo026` | 7 frames pass | Pending |
| We Did It | `css.eve.photo.wedidit` | `AnimLab/RT_L2_Photo027` | 7 frames pass | Pending |
| What | `css.eve.photo.what` | `AnimLab/RT_L2_Photo028` | 7 frames pass | Pending |
| Wish | `css.eve.photo.wish` | `AnimLab/RT_L2_Photo029` | 7 frames pass | Pending |
| Worry | `css.eve.photo.worry` | `AnimLab/RT_L2_Photo030` | 7 frames pass | Pending |
| Adam Catwalk | `css.eve.catwalk.adamcatwalk` | `AnimLab/RT_L3A_Adam` | 454 frames pass | Pending |
| Lily Catwalk | `css.eve.catwalk.lilycatwalk` | `AnimLab/RT_L3L_Lily` | 200 frames pass | Pending |
| ATOOL Gesture 200 | `css.eve.performance.example200` | `AnimLab/RT_L4S_Example200` | 10 frames pass | Pending |
| ATOOL Performance 801 | `css.eve.performance.example801` | `AnimLab/RT_L4D_Example801` | 1132 frames pass | Pending |
| After Brute Battle | `css.eve.performance.afterbrutebattle` | `AnimLab/RT_L4C_CameraBody` | 821 frames pass | Pending |

## Additional body and facial evidence

DumbDumb and Iris Out retain their L5X contact corrections. All cooked source
keys pass, including virtual bones. The 240 Hz neutral Black Pearl footwear
scan stays within 0.030 cm of the floor; all nine outfit meshes pass the
adapter comparisons. Full renders contain 1,972 and 501 frames at 30 FPS.
Sampled sheets were reviewed, but full temporal and live acceptance remain.
Evidence: `l5x-cook/full-render-review.json`,
`l5x-cook/decoded-runtime-v1/` and `l5x-adapter-v1/`.

Cinderella C11 passes full cooked-key comparison and complete-surface clearance
checks. Its 4,191-frame render is complete and 28 sampled stills were inspected.
That is not a claim that the entire video has been visually reviewed.
See [author-head and Cinderella evidence](shared-animation-author-head.md).

The combined Commander White face is E8. It retains all 90 prior morphs,
material bindings, skin weights, cloth, skeleton and post-process bindings.
Its 116 added shapes support 128 catalog controls. All six facial performances
are packaged. Geometry checks cover 19,584 source poses and 19,579 independent
intermediate poses under the unchanged 0.015 cm limit. Cinderella's stored
performance bytes are unchanged from its accepted independent export.
See [Commander White facial evidence](shared-animation-cw-face.md).

Eve's six derived faces have 115 facial shapes, 21 expressions and six facial
performances. The optional author head resolved the previously missing facial
controls. Unholy Genessa's two variants use a rigid native mask without verified
deforming face controls. Preserve that mask and keep body playback independent;
do not apply guessed Eve expressions to it.

## Installed state and remaining work

The installed private trial is still the earlier 57-animation build recorded
in `live-face-motion-2/deployment.json`. It includes Eve's 21 static expressions,
Come Closer and Gesture 200 facial tracks. The new combined candidate has not
been installed. A user question about availability for its trial is pending.

The stopping-pop correction is installed but still needs the matched gameplay
check in [the transition investigation](eve-stopping-transition.md).

The combined candidate now has an installer using the existing backed-up
replacement and rollback path. Its check-only run passed against the real
99-export pack and core `a943a5be35cb1035e0bfc407cf7092cf4e292dd7042aad308ab43e2bf7568e78`.
The old installer's 68-export facial check remains intact for older trials;
the new validator checks the combined facial baseline, container hashes,
dependencies, host loader results and full original-clip comparison reports.
Twelve existing transaction tests and six combined-pack tests pass. The new
path keeps the installed CINE extension and menu unchanged.
The validator follows the hashed replacement reports back to the combined
facial baseline, including the neck and contact revisions, and rejects changed reports.

For the next authorized closed-game trial, run from the CSS checkout:

```sh
python3 tools/authoring-probes/animations/install_shared_animation_trial.py \
  work/cine-animation-port/live-face-motion-2 \
  work/cine-animation-port/shared-contact-trial-v1 \
  work/cine-animation-port/cw-combined-trial-v1/verification.json \
  work/cine-animation-port/live-combined-contact-1 \
  --core build/cine-win/css_core.dll
```

Adding `--check-only` validates the candidate without installing it. It does
not claim that the running game or installed selectors were checked. The actual
installation checks those before writes and refuses an unexpected prior trial.
No installation has been performed by this checkpoint.

Remaining acceptance:

- Finish full temporal/source and contact review for the requested library.
- Resolve seven added source bone definitions, indices 3269 through 3275.
  All 54 required body/finger chains are resolved; the other tracks remain
  preserved. The missing definitions are documented separately, not silently
  counted as resolved. See [source requirements](shared-animation-source-requirements.md).
- Test playback, interruptions, saved selection and restoration through CSS
  and CINE on all six Eve outfits, both Genessa faces and Commander White.
  Include footwear, hair, cloth and Genessa's wing behavior.
- Test only the four requested locomotion choices on representative compatible
  default-human appearances, including starting/stopping and gameplay recovery.
- Measure selection, cold/warm playback and repeated-take frame costs and memory.
- Prepare the separate assets/runtime distributions with credits, installation
  instructions, BBCode notes and checksums together after acceptance.

Source permission was confirmed by the user on October 6. Do not re-open it
as a permission blocker. Original sources, archives and released outfits remain
unchanged. No public upload has been made.
