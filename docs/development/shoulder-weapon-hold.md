# Shoulder-carried weapon hold, CSS 1.0.1

## Current result

CSS 1.0.1 loads measured calibration and mesh/weapon bindings from
`catalog/hand-support.json`. Seven bindings, five weapon classes, 15 finger
profiles, 25 grip transforms and carry settings match the accepted measurements
exactly. The solver and cooked rigs are unchanged. See
[catalog authoring](../modding/hand-support.md). This release remains local and
unpublished while the catalog build is installed and tested.

October 17: CSS 1.0.1 and Shared Assets 1.0.1 are built under `dist/v1.0.1`
and installed from those exact archives for local testing. Nothing is published.
The three shoulder rigs are consolidated into Shared Assets; the standalone
hand provider is backed up outside `~mods`. Outfit packages do not change.
The release uses annotated local tag `v1.0.1`, commit `ffef9ad`, with developer
diagnostics and transition tests disabled. All 28 host suites, 14 packaging
tests, 180 startup checks and the production four-package catalog check pass.

Steam applied game build 26001001 before launching. The exact release loader
and core are mapped in the running game on workspace 3, with CSS state unchanged.
The existing save loads Smert, which has no saved CSS appearance; no appearance
or shell was changed during this startup check. New-game-build shoulder pose
and gameplay testing remain for the user. Exact installation and startup
evidence: `work/shoulder-weapon-hold/release-install-1.0.1`.

The earlier R7 candidate was installed under the user's “Install and test” approval. It
uses a relaxed thumb/index support pose and fits the right arm to the native
weapon placement without stretching the arm or moving the weapon in world
space. Its validation below is the source and asset baseline for CSS 1.0.1;
released CSS 1.0.0 and the three outfit releases remain closed.

The Windows core builds, all 28 host tests pass, and all three saved rigs pass
fresh-process component checks. Cooking, container verification, exact export
round-trip and independent cooked readback pass. A cold game start loads the
new rig and preserves saved CSS state.

The live gait matrix is complete on all five variants: eight primary weapons
in idle, walk, jog and 800 cm/s sprint, with full restoration after all 40 cases.
All five variants provide 450 shoulder-carry poses with zero measured
weapon crossings and zero cross-region hand self-intersections. Each variant
contributes 90 poses across the five shoulder-carry weapons.
All measured surfaces use neutral cooked skin, without morph displacement.

R6 failed live Commander White body-motion acceptance: its replacement rig
kept `UpdateSerial=18` and `TotalSteps=0`, while the old cached rig received
advancing input counters. R7 forwards the node's compiled input pins to the
active compatible rig after waiting for outstanding animation work. It copies
inputs only, preserving simulation state and the existing motion settings.
The engine regression fixture reproduces the failure without forwarding and
advances 58 simulation steps in 30 frames with it, including the disabled
shoulder phase. Live R7 confirmation passes: the body rig advances from 1,159
to 2,192 steps across three samples with its reset count unchanged at two.
A separate corrected/Default/corrected check advances 1,192, 1,705 and 2,246
steps in the same rig, with reset count one throughout and all motion settings
unchanged. The five-weapon CW gait regression passes, including 800 cm/s sprint;
all 381 captured hand poses clear the weapon. One Hadern sprint frame has a
0.057 mm intersection segment at the wrist/thumb-base skin crease. Its strict
zero-self-crossing check remains failed and the measured residual is retained;
this is not the original folded-finger failure.

The scoped validation and final handover restart are finished. The
expanded R7 combat matrix stopped at a restored case boundary after CW and
Unholy Eve, eight primary/sidearm pairs each: 128 action takes and 4,262 atomic
poses, zero rig errors and three initial block blend-out overlaps. This is not
exhaustive live coverage of every outfit/action combination.

The Gragu comparison passes on native Genessa appearance and all five custom
variants with Scythe, across idle/walk/jog/sprint. All 405 reconstructed custom
poses have zero weapon crossings and zero cross-region hand intersections.
All four existing profiles load and retain full carry correction when native
animations and a visible melee weapon are selected temporarily. Their saved
contents remain unchanged. The first profile attempts incorrectly required a
carry correction while the profiles intentionally hid melee weapons at idle;
the retained failed reports document that test mistake.

## Measured cause

| Hypothesis | Evidence | Finding |
| --- | --- | --- |
| Changed in-hand socket parent or local transform | All 82 socket records on both custom skeletons match the game skeleton | No socket-data mismatch found |
| Wrong gameplay-shell held offset | Scythe uses `Socket_Prop_R`, parent `prop_r`, with identity adjustment on both Genessa and Gragu | Gragu's +5 cm `Socket_Prop_L_02` table entry is a different socket |
| Off-hand IK misses the grip | Scythe idle has left/right correction alphas zero and retarget alpha one; Martyr's Blade alone enables left correction in the eight-weapon idle comparison | Does not explain the right-hand curl failure |
| Different hand geometry and animation transfer | Native Genessa and Peony receive identical right-finger local rotations, but their finger-base translations and skin differ | The same game pose collapses the custom fingers and misses the handle |

Fresh cooked-skin reconstruction reproduces bind geometry within 0.0001 cm.
Before correction, Scythe idle produces 673 cross-region right-hand triangle
crossings on each Eve/Unholy variant. Commander White has 132 on the right and
486 on the left. These are mesh-topology-dependent counts, not penetration depth
or a severity ranking between meshes. Peony's thumb-through-pinky terminal
median distances to the weapon are 2.457, 3.976, 4.410, 3.744 and 0.157 cm.

Fixing finger articulation alone is insufficient during locomotion. The R5
live Scythe matrix still has weapon crossings in 27 of 65 poses. The animated
prop-to-hand transform changes through the gait, so one fixed finger pose or a
single standing/moving scalar cannot keep contact.

Evidence: `native-peony-idle-comparison.json`,
`scythe-hand-analysis-skin-only/skin-measurements.json`, `gragu-control-r2`,
`primary-rendered-components/report.json`, and the R5 locomotion/contact reports
under `work/shoulder-weapon-hold/`.

## Implemented route

The runtime selects a compatible replacement Control Rig only for the exact
verified custom mesh/post-process combinations. Eve and Unholy retain their
existing left-hand adapter; the appended stage corrects right articulation.
Commander White's body rig retains its original graph and adds the measured
left/right finger adapters. Existing body and hair motion settings are preserved.

For Scythe, Hadern's Sword, Black Needle, Heavy Hammer and Battle Axe, the rig
uses a per-variant prop-to-hand reference and a relaxed per-weapon finger target.
It rotates `upperarm_r`, `lowerarm_r` and `hand_r` using the native two-bone
solver. Local translations and scales remain unchanged. It then compensates
the unweighted `prop_r` local transform to restore its incoming global transform.
The weapon stays where the game placed it. No skeleton, socket, bind pose, body
proportion, accepted motion limit or original outfit asset is edited.

The runtime checks the held weapon, socket, visibility, grounded state,
montages, sidearm use and gameplay input blocking. Imported CSS idles, gaits
and performances suspend the added stage. Carry blends in over 0.15 seconds
and out over 0.06 seconds, with action/bypass guards. The rig records its incoming
wrist height and prop Z before correction, avoiding feedback from the corrected
pose. Same-evaluation pose and reachability checks guard the arm solve.

Default (game) disables the added stage without replacing the surviving rig
instance or restarting Commander White's body simulation. Exact asset gates
exclude stock shells and unaudited meshes.

The game control carries these weapons primarily with the right hand in the
observed shoulder pose. The correction preserves the game's left-arm pose;
it does not move the lowered left hand onto the handle merely to make both
hands touch it.

Relevant source:

- `native/src/hand_support.hpp`, `.inl`, `_targets.hpp`, `_grips.hpp`.
- Integration in `native/src/engine.cpp`, `engine.hpp` and `core.cpp`.
- `tools/authoring-probes/hand-transfer/native_arm.py`, `native_nodes.py`,
  `calibration.py`, `export_carry_targets.py` and `export_grip_references.py`.
- `tests/hand_support_tests.cpp` and its CMake registration.

## Verification

| Check | Result and evidence |
| --- | --- |
| Windows build and host tests | Pass, 28 tests; `r7-windows-build.log`, `r7-host-tests.log`, frozen manifest |
| Independent arm replay | 451 poses, zero weapon crossings, no stretching or unreachable targets; `live-r6-arm-support-r2` |
| Native saved-class evaluation | 1,804 evaluations across five variants, plus nine disable/unreachable/outside-carry cases; `arm-rig-{eve,unholy,cw}-r3/r6-profiles` |
| Fresh component load | All three classes pass class selection, output-bone transfer, bypass and restoration; `cold-arm-r2/result.json` in each rig directory |
| Cooked asset validation | Pass; `hand-cook-r6/exit.json`, `hand-runtime-pack-r6/receipt.json` |
| Cold game start | R6 selected, shoulder support active, full saved CSS state equal; `install-r6-restart/after.json` |
| Peony live gaits | Eight weapons, four gaits; pilot plus completed Peony cases in `live-r6-all-weapons` |
| Peony live contact | 90 shoulder-weapon poses, zero weapon crossings; `live-r6-pilot-skin` and `live-r6-checkpoint01-skin` |
| Other variants' live gaits | Complete and restored; `live-r6-all-weapons/report.json`, combined `live-r6-gait-matrix.json` |
| Default switch | All six cases preserve the held weapon world transform and re-enable the same rig; `live-r6-defaults` |
| Default bone invariants | Five strict whole-pose cases pass. CW hair responds to changed arm collision shapes during the frozen-pose toggle; see diagnostic below |
| R6 live body simulation | Failed on CW; `live-r6-cw-body-clock`. R7 repairs input forwarding |
| R7 input-clock regression | Pass, negative reproduction and positive forwarding with 21 compiled inputs; `arm-rig-cw-r3/input-clock-r7` |
| R7 live CW body clock | Pass, advancing clock/steps and no further resets; `live-r7-cw-body-clock/analysis.json` |
| R7 Default/body simulation | Pass, same rig, advancing steps, no resets or motion-setting changes; `live-r7-cw-default-clock/analysis.json` |
| R7 CW gait regression | Five weapons, four gaits, 381 poses, zero weapon crossings. One 0.057 mm wrist/thumb crease intersection; `live-r7-cw-gaits-skin/crease-detail.json` |
| R7 cold start | Pass, full CSS state, appearance and geometry restored on workspace 3; `install-r7-restart/after.json` |
| R6 combat/draw/stow | All eight native controls plus Peony Scythe: 72 action takes, 2,749 atomic poses, all restored. Stopped at a case boundary for the body-clock fix |
| R7 combat/draw/stow | CW and Unholy Eve, eight primary/sidearm pairs each: 128 action takes, 4,262 atomic poses. All 16 loadouts restored, zero rig errors; `live-r7-combat-loadouts/coverage.json` |
| R7 Gragu gameplay shell | Native control plus five custom variants, Scythe, four gaits; 405 custom poses clear weapon and cross-region intersections; `live-r7-gragu-r2/measurements.json` |
| R7 existing-profile load | Four profiles pass, carry alpha one with temporary native animations and visible weapon; saved profiles/state restored exactly; `live-r7-profile-load-r4/report.json` |
| Final original controls/settings | Original nine asset files and complete CCS settings verified after final cold start; `live-r7-profile-load-r4/final-control-restoration.json` |

Maximum independent arm replay changes are 11.5803 cm at the wrist,
10.6524 degrees of upper-arm local rotation, 20.6287 at the forearm and 72.2379
at the wrist. These alter the animated pose, not arm lengths or binding.
Native wrist-target error is at most 0.00810 cm. UE's hierarchy skips transform
writes within its 0.0001 quaternion-component comparison tolerance, which
explains the measured small differences from the independent solver.

One retained Unholy Hammer sprint pose has already left the carry envelope
(wrist height -35.7124 cm, prop local Z -1.6035 cm). The native rig correctly
bypasses its arm solve. Thus 451 offline fitted poses do not mean 451 runtime
carry-eligible poses; 450 qualify in that retained set.

The refined Needle references clear weapon and cross-region hand intersections
on all five variants. A previous index-metacarpal Y adjustment caused new
index/palm and index/middle crossings and was rejected. Black Pearl uses the
Peony Needle grip reference. Failed trials and their receipts remain retained.

The six Default takes freeze base animation. The attachment/core hand-update
phase has median 0.0534-0.0597 ms enabled and 0.0212-0.0246 ms on Default for
the custom variants. These timings do not measure the complete animation cost
or establish a gameplay FPS impact.

Commander White's frozen Default comparison initially changes 31 hair bones.
The repeated trace retains the same hair, body and belt rig instances. The
hair rig's step counter stays fixed, but its reset counter advances on toggles.
The contact trace records `TickSeconds=0` and collision endpoints on
`upperarm_r`, `lowerarm_r` and `Hand_R`. The original hair graph marks changed
collision endpoints as `PoseChanged`, then reseeds when delta time is zero.
Thus a whole-pose equality assertion across an arm change is invalid for this
frozen secondary simulation. This does not justify excluding arbitrary hair
changes from normal gameplay checks. Retained evidence:
`live-r6-cw-hair-trace`, `live-r6-cw-hair-contacts`, and the original
`build_controlrig_full_hair.py` / `controlrig_full_hair_contacts.py` authoring
sources. The failed strict comparisons remain available.

The body-clock issue is separate from the hair reset. Live R6 reads show the
post-process update serial at 546, 769 and 980, while the active body rig remains
at serial 18 with zero steps. UE 5.6.1's local `AnimNode_ControlRig.cpp` and
`Tools/ControlRigVariableMappings.cpp` retain cached pin writers across the
dynamic class change. R7 resolves the reflected source/destination pin names,
checks matching types and bounded arrays, and forwards those values to the
active rig. It does not copy private simulation counters or poses. The engine
fixture retains an unchanged reset count of one in both test phases.

The faster R6 combat sampler captures one brief overlap at the beginning of
Peony's block transition: carry alpha 0.1333 at 0.067 seconds. Subsequent
samples are zero. This is retained evidence of the designed blend-out, not an
all-frame zero-overlap result. Metadata and component poses are sequential.

### Broader default-game comparison

The completed before-fix matrix covers all eight primary weapons and five
variants: idle/walk/jog/sprint, sampled light/heavy chains, free Infinite-seal
parry, running light/heavy and dodge. `capture-matrix.json` indexes the captures.
The valid native control comes from `vanilla-genessa-all-weapons`,
`vanilla-genessa-combat-default` and `vanilla-genessa-running-combat`.

R5 adds a full sustained-gait matrix: 40 cases, 160 gait categories and 721 atomic
poses. Its later native combat batch captured 33 cases, 198 action sequences
and 1,403 poses before user activity interrupted restoration. After the user
handed control back, `restore-followup-r2.jsonl` verified full restoration of the
last take. No sampled montage read had carry alpha above zero. Metadata reads
are sequential and do not prove zero overlap on every animation frame.

Offline action analysis follows 155 primary source sequences and 20,311 keys,
with 1,008 distinct skin poses. The sidearm/seal inventory includes 126 montage
references and 119 source sequences. Of these, 118 export successfully (19,944
keys); the Head King riposte export rejects incomplete reference coverage for
its high bone indices. Its failed receipt is retained. The sidearm/seal skin
batch has 1,347 poses.
The final articulation gain leaves local cross-region folds in 22 primary and
8 sidearm/seal poses. Keep those residual counts visible. Source-key tests
precede game retargeting, overlays and blending and do not replace live tests.

Still untested live: every attack branch/finisher, successful enemy parry and
riposte, Axatana's alternate form, the eight sidearms' complete movesets and
all four seals. Offline bytecode confirms that equipping Slayer calls
`SetSlayerSealLockout(true)` before checking the `Save` argument. Its live test
must not introduce that flag on a save that has not already set it; read the
existing flag first. `equipment-readback-r6/slayer-side-effect.json` records
the evidence. The detected roster is eight primary weapons, eight sidearms
and four seals. Primary coverage must not be described as all game weapons.

### Visual evidence

- [Captured game comparison](../../work/shoulder-weapon-hold/live-r6-defaults/scythe-game-comparison.jpg): native Genessa, Default and corrected Scythe idle for each variant.
- [Measured hand surfaces](../../work/shoulder-weapon-hold/live-r6-defaults/scythe-hand-comparison.png): the same captured poses, reconstructed from neutral cooked skin.

Triangle crossings are not penetration depth. Unsigned nearest-surface gaps
do not prove load-bearing contact. Stock and custom topology differ. These
measurements supplement the game captures rather than replacing visual review.

## Exact installed test files

The runtime root is
`MortalShell2/Binaries/Win64/ue4ss/Mods/CustomShellSystem`.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `cores/css_core-shoulder-support-8977a2c68192d922.dll` | 5,137,920 | `8977a2c68192d922cd8aca02790904cd8d39c568256a8f65f070e65acbef38f7` |
| `CSS_HandSupport_P.pak` | 347 | `75e7144577253917f6da7312ef5e585b12fb728226a22b0938323751a6b555cd` |
| `CSS_HandSupport_P.utoc` | 1,713 | `861f2e8bd40d99a411ab36a43b811786c85edf2a58d50ec1f42e58e2e193ee65` |
| `CSS_HandSupport_P.ucas` | 5,568,896 | `a4ccab61cdd7ab4a40fd7a9434182d9d7082fd8292b97d2914bdf2e43644c7a2` |

The three containers are in `MortalShell2/Content/Paks/~mods/CSS_HandSupport_P`.
`core.json` selects the listed DLL. The containers contain only
`CR_EveShoulderSupport`, `CR_UnholyShoulderSupport` and
`CR_CommanderBodyShoulderSupport` under `/Game/CSS/Runtime/Hands`.
`candidate-r7/manifest.json` records source hashes and limitations. Its asset
containers are byte-identical to R6. `live-candidate-r7/backup` retains the R6
DLL, selector, containers and complete saved-state directory. Earlier frozen
bundles and rollback copies are retained.

## Shared Assets consolidation, October 17

The shoulder rigs belong in the existing `CSS_SharedAssets_P` dependency. The
separate `CSS_HandSupport_P` was used for isolated testing, not as an additional
permanent download. `tools/shared-assets/add-shoulder-support.py` now builds the
combined private candidate from the released Shared Assets ZIP and verified R6
rig container. It preserves asset paths, so the tested R7 core needs no rebuild.

Candidate: `work/shoulder-weapon-hold/shared-assets-r7/CSS_SharedAssets_P-v1.0.1-candidate.zip`.
`shared-assets-r7/verified.json` records input/output hashes and checks:

- All 291 existing packages plus the three shoulder rigs, 294 total.
- All 588 reconstructed asset files round-trip byte for byte.
- Independent decoded rig objects match the previously tested package exactly.
- Existing catalog entries and facial resources are preserved; only container
  checksums and sizes change. ZIP extraction matches the built trio.
- The production catalog loader accepts all three outfits and the shared
  library: four loaded packages, zero rejected and ten facial bindings matched.

The installed Shared Assets metadata and released ZIP differed only in two
version fields: installed `0.1.0-dev`, released `1.0.0`. Asset containers and all
other catalog fields matched. The candidate uses the released catalog and keeps
its version fields unchanged until release preparation. Its filename marks it
as a private candidate, not a published 1.0.1 release.

The released v1.0.0 ZIP and installed files were not modified by this packaging
step. Installing the candidate requires replacing the Shared Assets trio and
retiring the standalone `CSS_HandSupport_P` trio in the same stopped-game update.
The earlier live results apply to identical rig payloads; this combined
container has not been tested in a running game.

## Restoration and delivery

The nine original Genessa containers have been restored with their recorded
hashes, and the complete original CCS settings have been restored. The
R7 shoulder-support DLL and its three asset containers remain installed.
The original CSS state, four named profiles, appearance, equipment, health,
Resolve, unlocks and Slayer flag are preserved. The game is running on workspace 3 with shoulder support active. Final handover evidence is recorded in
`live-r7-profile-load-r4/final-after.json` and
`live-r7-profile-load-r4/final-control-restoration.json` after the completed restart.

`final-matrix-r7.json` consolidates the scoped checks. The R7 combat table covers
CW and Unholy Eve; older candidate checks and offline analysis remain separate.
Other weapons on Gragu and the expanded live combat combinations remain
untested in R7. Do not describe this as exhaustive all-moveset verification.

All evidence lives under `work/shoulder-weapon-hold/`. The experimental history
and rejected trials are retained in `report-history-before-r6-live.md` and the
individual failed receipts. No commit, public push or release stamp was made.
