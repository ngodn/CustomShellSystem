# Shared animation acceptance

Checkpoint: 2026-10-06. This is a private candidate, not release approval.

The catalog contains eight released idles, eighteen moving clips and thirty-one
photo poses. DumbDumb and Iris Out remain additional unfinished clips. All live
acceptance entries below are pending for this shared-pack revision, even where
an older outfit release has previously worked in game.

## Evidence boundaries

- Cooked samples: independent compressed-pose readback compared with the authored
  sequence, up to five samples per clip. This does not prove every frame or contact.
- Adapter poses: all eighteen moving clips pass on all nine meshes, including
  38 finger bones, 1,458 cases and 87,255 full-motion poses at 60 Hz. This does
  not establish the world cloth solver or contacts.
- Sampled silhouettes: all eighteen moving clips have complete sampled reviews;
  close contacts, native-speed motion and live physics remain open.
- Live acceptance: playback, interruption, restoration, cloth/wings, contacts and
  frame costs remain required in both CSS and CINE.

Receipts beneath `work/cine-animation-port/shared-v1/`: `plan.json`,
`catalog.json`, `cooked-samples-v3/motion-verification.json`,
`performance-fingers-200-v1-result.json` and the completed
`performance-fingers-all-v1-result.json`. Source means the audited pre-cook
authoring asset; `plan.json` also retains the original source paths and rate
metadata for new ports. Original archives are inventoried separately.

## Per-clip state

All source assets in the table are beneath `/Game/CSS/`.

| Clip | Canonical ID | Audited source | Cooked samples | Live |
| --- | --- | --- | ---: | --- |
| Eve Default Idle | `css.eve.idle.default` | `Eve/Anim/AN_I2_Idle` | 5 pass | Pending |
| CSS Seated Recline | `css.eve.idle.seated_recline` | `Eve/Anim/Idles/AN_Idle702` | 5 pass | Pending |
| CSS Kneeling Bow | `css.eve.idle.kneeling_bow` | `Eve/Anim/Idles/AN_Idle703` | 5 pass | Pending |
| CSS Low Lean | `css.eve.idle.low_lean` | `Eve/Anim/Idles/AN_Idle704` | 5 pass | Pending |
| CSS Deep Kneeling Bow | `css.eve.idle.deep_kneeling_bow` | `Eve/Anim/Idles/AN_Idle705` | 5 pass | Pending |
| CSS Reclining | `css.eve.idle.reclining` | `Eve/Anim/Idles/AN_Idle706` | 5 pass | Pending |
| CSS Forward Fold | `css.eve.idle.forward_fold` | `Eve/Anim/Idles/AN_Idle708` | 5 pass | Pending |
| CSS Side Kneel | `css.eve.idle.side_kneel` | `Eve/Anim/Idles/AN_Idle713` | 5 pass | Pending |
| Bass Down Low | `css.eve.dance.bassdownlow` | `AnimLab/RT_L2_S01A000` | 5 pass | Pending |
| Black Hole | `css.eve.dance.blackhole` | `AnimLab/RT_L2_S02A000` | 5 pass | Pending |
| Eve Catwalk | `css.eve.catwalk.evecatwalk` | `AnimLab/RT_L2_S03A000` | 5 pass | Pending |
| Come Closer | `css.eve.dance.comecloser` | `AnimLab/RT_L2_S04A000` | 5 pass | Pending |
| Jang Won Young Spin | `css.eve.dance.jangwonyoungspin` | `AnimLab/RT_L2_S07A000` | 5 pass | Pending |
| ATOOL Performance 1 | `css.eve.performance.atool1` | `AnimLab/RT_L2_S11A000` | 5 pass | Pending |
| ATOOL Performance 2 | `css.eve.performance.atool2` | `AnimLab/RT_L2_S11A001` | 5 pass | Pending |
| ATOOL Performance 3 | `css.eve.performance.atool3` | `AnimLab/RT_L2_S11A002` | 5 pass | Pending |
| ATOOL Performance 4 | `css.eve.performance.atool4` | `AnimLab/RT_L2_S11A003` | 5 pass | Pending |
| ATOOL Performance 5 | `css.eve.performance.atool5` | `AnimLab/RT_L2_S11A004` | 5 pass | Pending |
| ATOOL Performance 6 | `css.eve.performance.atool6` | `AnimLab/RT_L2_S11A005` | 5 pass | Pending |
| ATOOL Performance 7 | `css.eve.performance.atool7` | `AnimLab/RT_L2_S11A006` | 5 pass | Pending |
| ATOOL Performance 8 | `css.eve.performance.atool8` | `AnimLab/RT_L2_S11A007` | 5 pass | Pending |
| Bring It On | `css.eve.photo.bringiton` | `AnimLab/RT_L2_Photo000` | 5 pass | Pending |
| Charm | `css.eve.photo.charm` | `AnimLab/RT_L2_Photo001` | 2 pass | Pending |
| Clap | `css.eve.photo.clap` | `AnimLab/RT_L2_Photo002` | 5 pass | Pending |
| Confident | `css.eve.photo.confident` | `AnimLab/RT_L2_Photo003` | 5 pass | Pending |
| Cutie | `css.eve.photo.cutie` | `AnimLab/RT_L2_Photo004` | 5 pass | Pending |
| Flow | `css.eve.photo.flow` | `AnimLab/RT_L2_Photo005` | 2 pass | Pending |
| Give Me | `css.eve.photo.giveme` | `AnimLab/RT_L2_Photo006` | 5 pass | Pending |
| Hand Heart | `css.eve.photo.handheart` | `AnimLab/RT_L2_Photo007` | 5 pass | Pending |
| Hush | `css.eve.photo.hush` | `AnimLab/RT_L2_Photo008` | 5 pass | Pending |
| Kiss | `css.eve.photo.kiss` | `AnimLab/RT_L2_Photo009` | 2 pass | Pending |
| Kung Fu Salute | `css.eve.photo.kungfusalute` | `AnimLab/RT_L2_Photo010` | 5 pass | Pending |
| Natarajasana | `css.eve.photo.natarajasana` | `AnimLab/RT_L2_Photo011` | 5 pass | Pending |
| Oh My | `css.eve.photo.omg` | `AnimLab/RT_L2_Photo012` | 5 pass | Pending |
| Reach | `css.eve.photo.reach` | `AnimLab/RT_L2_Photo013` | 2 pass | Pending |
| Salute | `css.eve.photo.salute` | `AnimLab/RT_L2_Photo014` | 5 pass | Pending |
| Shutter Chance1 | `css.eve.photo.shutterchance1` | `AnimLab/RT_L2_Photo015` | 5 pass | Pending |
| Shutter Chance2 | `css.eve.photo.shutterchance2` | `AnimLab/RT_L2_Photo016` | 5 pass | Pending |
| Shutter Chance3 | `css.eve.photo.shutterchance3` | `AnimLab/RT_L2_Photo017` | 5 pass | Pending |
| Slav Squat | `css.eve.photo.slavsquart` | `AnimLab/RT_L2_Photo018` | 5 pass | Pending |
| Somersault | `css.eve.photo.somersault` | `AnimLab/RT_L2_Photo019` | 5 pass | Pending |
| Stretch | `css.eve.photo.stretch` | `AnimLab/RT_L2_Photo020` | 5 pass | Pending |
| Sweetie | `css.eve.photo.sweetie` | `AnimLab/RT_L2_Photo021` | 5 pass | Pending |
| Thumb Up | `css.eve.photo.thumbup` | `AnimLab/RT_L2_Photo022` | 5 pass | Pending |
| Toast | `css.eve.photo.toast` | `AnimLab/RT_L2_Photo023` | 5 pass | Pending |
| Victory | `css.eve.photo.victory` | `AnimLab/RT_L2_Photo024` | 5 pass | Pending |
| Walk | `css.eve.photo.walk` | `AnimLab/RT_L2_Photo025` | 5 pass | Pending |
| Warm Up | `css.eve.photo.warmup` | `AnimLab/RT_L2_Photo026` | 5 pass | Pending |
| We Did It | `css.eve.photo.wedidit` | `AnimLab/RT_L2_Photo027` | 5 pass | Pending |
| What | `css.eve.photo.what` | `AnimLab/RT_L2_Photo028` | 5 pass | Pending |
| Wish | `css.eve.photo.wish` | `AnimLab/RT_L2_Photo029` | 5 pass | Pending |
| Worry | `css.eve.photo.worry` | `AnimLab/RT_L2_Photo030` | 5 pass | Pending |
| Adam Catwalk | `css.eve.catwalk.adamcatwalk` | `AnimLab/RT_L3A_Adam` | 5 pass | Pending |
| Lily Catwalk | `css.eve.catwalk.lilycatwalk` | `AnimLab/RT_L3L_Lily` | 5 pass | Pending |
| ATOOL Gesture 200 | `css.eve.performance.example200` | `AnimLab/RT_L4S_Example200` | 5 pass | Pending |
| ATOOL Performance 801 | `css.eve.performance.example801` | `AnimLab/RT_L4D_Example801` | 5 pass | Pending |
| After Brute Battle | `css.eve.performance.afterbrutebattle` | `AnimLab/RT_L4C_CameraBody` | 5 pass | Pending |

## Unfinished source clips

| Clip | Retained source | Blocker |
| --- | --- | --- |
| DumbDumb | source-05, `HD-ATOOL_P/anim/971` | Seven unidentified source bone indices; nine unresolved facial controls; public conversion permission not established |
| Iris Out | source-06, `HD-ATOOL_P/anim/970` | Seven unidentified source bone indices; two unresolved facial controls; public conversion permission not established |

Neither clip is silently discarded, approximated or counted among the 57 cooked
sequences. See [source audit](shared-animation-library.md) and
[facial curve audit](performance-facial-curves.md). Jang Won Young Spin has a
cooked body clip but still lacks mappings for its blink, woo and ooo facial
controls; the body table does not claim complete facial fidelity.

## Facial and runtime state

The installed private trial has 21 static Eve expressions and Come Closer facial
curves on six verified derived Eve meshes. Gesture 200 facial curves are now
installed through the catalog-only revision recorded in
`work/cine-animation-port/live-face-motion-2/deployment.json`. Come Closer and Gesture 200 retain source timing and
signed curve values; neither has live acceptance yet. Unassigned facial actions
in 801 are not treated as active animation.

The stopping-pop correction is installed but still needs the matched gameplay
check described in [the transition investigation](eve-stopping-transition.md).
Eve, Genessa and Commander White isolated-package checks pass; those dependency
checks do not substitute for their live outfit/physics tests.

Before release, complete the unresolved source work, native-speed/contact and
live acceptance, source permission review, performance measurements and the
separate asset/runtime distributions with credits, BBCode notes and checksums.

The [source requirements](shared-animation-source-requirements.md) specify the
missing rig data. The [permission audit](shared-animation-source-permissions.md)
now covers all six additional source pages as well as DumbDumb and Iris Out.
Their published terms prohibit cross-game conversion; any separate grant still
needs to be established before public packaging. The installed files remain a
private trial, with no public upload made.
