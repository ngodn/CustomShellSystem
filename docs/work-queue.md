# Current work queue

## October 17: Vista sitting animation and local skin releases

The user requested an in-game inspection of the sitting interaction, a separate
choice using the full Idle Animation list in CSS 1.0.1 if supported, and all skin
release ZIPs with BBCode changelogs under `dist`. The live main animation instance
is playing `A_Shared_Actions_Vista_Sit_Loop_Montage`, establishing the game's
Vista Sit interaction as the target. Preserve native entry, exit and interaction
ownership, and keep ordinary idle selection independent. Evidence:
`work/sitting-animation/`. The release request covers local artifacts; no online
publication was requested.

Follow-up: audit every native Vista animation mapping and supported Idle body/
skeleton, including Stray Genessa, Harbinger, Dark Bro and Lazlo. Reuse existing
Shared Assets Idle compatibility; do not require an outfit-only idle layer.
The user supplied these images for the Vista BBCode changelog:
`/home/eins0fx/Pictures/screenshot-2026-10-17_18-07-12.png` and
`/home/eins0fx/Pictures/screenshot-2026-10-17_18-05-18.png`.

Current checkpoint: all 62 exact carrier choices pass on native Genessa. Four
choices and native sit/stand pass on 16 additional bodies, including every
Harbinger body, Dark Bro and our four outfit variants. After the user's timing
complaint, stop expanding the body sweep. Six targeted gameplay cases now pass:
Lazlo, Harbinger and Stray, each native and with Peony, including mirror behavior.
The original profile and Faithful form are restored. Eight nearby non-boss hostile
enemies were cleared with explicit authorization, then cleared after respawn.
The final Enter-focus repair is installed and verified: filtering for `iris` and
pressing Enter selects Iris Out without changing normal Idle. Final packaging
uses that exact DLL. Do not expand the matrix again. All four skin/shared release
ZIPs and BBCode notes are verified under `dist`; both supplied Vista screenshots
are preserved in `dist/v1.0.1/media`. The delivery index records final archive
paths and hashes; no public upload is part of this request.

Final delivery: CSS 1.0.1 is built from annotated tag `v1.0.1` at `2e67249`.
All five archives and separate BBCode changelogs are under `dist`. Both final DLLs
match the archive. Startup, restored Peony appearance, position/camera and unchanged
CSS settings are confirmed in `work/sitting-animation/final-install-confirmed`.
The earlier resume timeout was not evidence of a game failure. Shader compilation
was observed directly on the final launch; the existing native menu functions
continued the save after a black menu screen. No additional pose sweep was run.

The user subsequently requested uploading the supplied Vista images to the
repository and placing them beside the Vista feature description. Completed on
branch `docs/vista-rest-1.0.1-media`, with two verified raw image URLs pinned to
`9b85d80`. Source and dist BBCode match, and the separate bottom image block is
removed. This publication covers the screenshots and changelog only.

## October 17: Commander White blood and Peony revealed skin

CW 0.1.1 and compatible Shared Assets 1.0.1 metadata are built and installed.
The later explicit request for local releases is fulfilled under `dist`. Spatial garment masks and nine blood-enabled skin instances
pass ten native blood ladders, all displayed colors/palettes, profile reload,
package round-trip and catalog checks; 28/28 host suites pass. The user's
correction adds our Unholy Genessa, both variants, to the blood references.
Peony's reproduced bands are the axe's native shadow; preview controls share
the same harsh shading. No Eve or runtime repair is justified by this evidence.
All 65,536 Peony palette/glow/visibility combinations pass native section
resolution and save round-trip, with representative world/preview captures.
Literal exhaustive visual acceptance remains unclaimed. Three garment masks
need increased dimensions while preserving their constant roughness/metallic
values; the skin parent has documented static-permutation differences.
The user's saved state is restored. Frozen releases are unchanged, and nothing
is published. Evidence, separate ZIPs, exact staged files and limits are in
[the finding doc](development/cw-blood-peony-skin.md).

Follow-up: the user identified the dark coverage in their new Peony close-up
as the stocking layer. Only live status was read; no game settings were changed.
The user then accepted the appearance and requested all skin releases with
BBCode changelogs. The local release delivery supersedes the candidate-only
checkpoint; the finding doc retains the exact tested matrix.

## October 17: hand-support catalog, CSS 1.0.1

CSS 1.0.1 loads `catalog/hand-support.json` once at startup, following the
existing runtime catalog pattern. It owns the exact bindings, weapon IDs/classes,
15 finger profiles, 25 grips, output-bone lists and carry ranges/blend settings.
All values match the accepted measured baseline exactly. Outfit/weapon bindings
and calibration values are data; the solver and rig interface remain code.
Malformed or missing catalogs disable hand support with a diagnostic.
[Authoring and schema](modding/hand-support.md).

Completed: the unpublished CSS 1.0.1 archive under `dist/v1.0.1` and local
installation now contain the catalog build, tagged at `e76447a`. All 28 host
suites, 15 release tests and catalog ASan/UBSan checks pass. The cold start loads
the JSON without errors and resumes the existing Smert save on workspace 3.
Runtime hashes match the archive; saved CSS state, Shared Assets 1.0.1 and outfit
packages are unchanged. Evidence: `catalog-install-1.0.1/receipt.json` under
`work/shoulder-weapon-hold`. No online publication. The lookup benchmark has
zero allocations; full-game FPS and exhaustive combat acceptance are not claimed.

## October 17: bundle shoulder rigs in Shared Assets

The user then requested local release archives under `dist` and installation
of those exact release builds for testing before publication. CSS and Shared
Assets advance together to 1.0.1; the three outfit versions stay unchanged.
The coordinated source pins `shared-assets-1.0.1`, with both shared catalog
version fields stamped 1.0.1. Remove the isolated hand-support provider during
installation. Public upload is not part of this step.

Completed: both release ZIPs and checksum files are in `dist/v1.0.1`, built
from local annotated tag `v1.0.1` at `ffef9ad`. Those exact files are installed;
the old hand provider is outside `~mods` in a rollback directory. The game
loads the shipping core on workspace 3 after Steam updated to build 26001001.
Saved CSS state is unchanged. The save loads Smert without a CSS selection;
no shell/appearance switch or new pose matrix was performed. User testing
comes before publication. See `release-install-1.0.1/receipt.json` and the
dist release status for the exact scope.

The user requested the existing Shared Assets package own the shoulder rigs.
Built `work/shoulder-weapon-hold/shared-assets-r7/CSS_SharedAssets_P-v1.0.1-candidate.zip`
with all 291 released packages plus three rigs. All 588 reconstructed files
round-trip exactly and the independent decoded rigs match the tested R6 assets.
The released archive, installed trios and core are unchanged. Installation must
replace the shared trio and retire `CSS_HandSupport_P` together. The private
candidate retains the released catalog version until release preparation.
See [consolidation evidence](development/shoulder-weapon-hold.md#shared-assets-consolidation-october-17).

## October 16: shoulder-carried weapon hold, CSS 1.0.1

Implement relaxed thumb/index support on Eve Peony/Black Pearl, Commander
White and both Unholy skins. Compare the native game's eight primary weapons,
gaits and combat, plus sidearm/seal transitions. Preserve proportions, V44
binding, motion limits, existing profiles and released package versions.
[Report and matrix](development/shoulder-weapon-hold.md).

The user approved “Install and test”, authorized autonomous game control, and
explicitly handed control back after interrupting the R5 combat run. Use English
and workspace 3. Do not repeat ready/go or routine staging-permission questions.

R7 is installed: core `8977a2c68192d922` plus the unchanged three R6
`CSS_HandSupport_P` containers, with R6 rollback copies in `live-candidate-r7`.
Windows and all 28 host tests pass. Three saved rigs pass 1,804 native evaluations, nine negative
cases, fresh component checks, cooking and independent cooked readback.
Cold game start passes with saved CSS state preserved.

R6 failed live CW body-motion acceptance: the new rig's UpdateSerial stayed at
18 and TotalSteps at zero, while the old cached rig kept receiving inputs.
R7 forwards only the compiled input pins with reflected type checks. The
engine fixture reproduces the failure without forwarding and advances 58
steps across 30 frames with it, with no extra reset. R7 cold start and live
body-clock checks pass: 1,159 to 2,192 steps, no further reset. Evidence:
`install-r7-restart`, `live-r7-cw-body-clock`, `live-r6-cw-body-clock` and
`arm-rig-cw-r3/input-clock-r7`. No asset recook was needed.

The fix preserves native weapon world placement by fitting the right arm
without stretching and compensating `prop_r`. Finger targets remain relaxed.
The independent 451-pose replay has zero weapon crossings; one retained pose
is outside the runtime carry envelope and correctly bypasses the arm solve.

Live R6: all five variants have completed all eight primary weapons and four
gaits, including 800 cm/s sprint: 40 cases, 160 gaits, 721 atomic poses.
All 450 measured shoulder-carry poses have zero weapon crossings and zero
cross-region hand self-intersections. Combine `live-r6-all-weapons` with the
two Peony pilot cases. Non-shoulder weapons keep carry alpha zero.

All six Default toggles preserve the held weapon world transform; stock has
zero bone changes. CW's frozen take changes hair because its existing hair rig
reseeds when arm collision endpoints change with zero delta time. The contact
trace confirms the right arm/hand endpoints and resolved contacts. Same-setting
comparisons are stable; later toggles do not repeat the initial hair rotation
change. Keep the strict failed analysis.
Game-frame and measured hand-surface comparisons are in `live-r6-defaults`.

The R6 combat batch stopped cleanly after eight native controls and Peony
Scythe: nine restored cases, 72 action takes and 2,749 poses. One initial block
sample has carry alpha 0.1333 during the blend-out; later samples are zero.

R7 CW gait regression now passes for all five carry weapons, including full
sprint. Its 381 poses clear the weapon; one Hadern sprint pose retains a
0.057 mm intersection segment at the wrist/thumb-base crease, inspected and
recorded in `live-r7-cw-gaits-skin/crease-detail.json`. Strict zero-self-crossing
therefore does not pass. The unfrozen
corrected/Default/corrected test passes: 1,192 to 2,246 body steps, one reset
throughout, same rig and unchanged motion settings.

R7 combat stopped cleanly after CW and Unholy Eve: 16 primary/sidearm loadouts,
128 action takes and 4,262 poses, zero rig errors. Three first block samples
show the designed carry blend-out. The other R7 outfit/action combinations
remain untested; earlier R5/R6 and offline evidence is separate.

The Gragu comparison passes with Scythe on the native appearance and all five
variants, four gaits each. Its 405 custom hand poses have zero weapon or
cross-region hand intersections. `live-r7-gragu-r2` is complete and restored.
The first Gragu attempt caught an asynchronous selection-save delay in the
probe; the outer restore succeeded and the corrected wait passed.

Four existing profiles pass in `live-r7-profile-load-r4`: native animation
choices and temporary visible melee weapons all reach carry alpha one.
Saved presets and full CSS state are restored exactly. Earlier test attempts
mistakenly required carry support while the profiles hid melee weapons at idle;
those failed receipts are retained. Profile visibility was never saved over.

The nine original Genessa containers and complete CCS settings are restored.
The approved R7 bundle remains installed. The final handover restart on workspace
3 passed: `live-r7-profile-load-r4/final-after.json` and its matching control
receipt verify saved state, equipment, health/Resolve, geometry, mapped R7 core,
active support, all nine original file hashes and complete original CCS settings.
The implemented fix is installed and handed back. `final-matrix-r7.json` records
the completed validation scope and remaining coverage limits.

The broader before-fix and offline matrices remain available. Exhaustive live
moveset coverage, successful enemy parry/riposte, Axatana alternate form and
Slayer are not claimed. Slayer remains excluded because equipping it alters an
unset save/achievement flag before Save=false is checked. No commit or public
push has been made.

## October 15: current release status

The [goal](development/three-outfit-rig-physics-goal.md) and
[release audit](development/three-outfit-release-gates.md) supersede the older
checkpoints below. Do not restart accepted checks from historical queue entries.

- The user accepts leaving the isolated October 13 developer reload/rescan
  crash unresolved. It is nonblocking. Retain the evidence and investigate
  again only if it recurs; no confirmed cause or crash fix is claimed.
- Peony fit, motion, palettes, flowing trim and profile restoration, CW belt,
  skirt and walking, and Genessa world cloth have user acceptance. Recorded
  facial, doubles, sidearm and UI results remain valid at their tested scope.
- Zhirelle and the individually documented difficult bosses move to v1.0.1.
  Slipstream, minor clipping polish and the original After Brute Battle
  end-loop issue remain deferred. Older instructions below do not reopen them.
- Current Eve 1.5.0, Unholy Genessa 0.3.0, Commander White 0.1.0 and Shared
  Assets 1.0.0 candidates are in `dist`. CSS targets stable 1.0.0, not beta.12.
  Preserve the accepted sources and finish coordinated runtime delivery.
- Remaining original requirements are listed in the release audit. Resolve
  them against existing evidence first; research limitations do not create
  new test requirements. The failed clean-install preparation is retained;
  the later [fresh shipping-runtime test](research/2026-10-15-clean-shipping-runtime.md)
  passes startup, provider discovery, facial playback and exact restoration.

The running game is left alone during this offline source/documentation work.

## October 7: facial idle follow-up regressions

Installed the CW E9 optical-material candidate and CSS core 45d2015ae1e118bd.
Independent comparison preserves all 206 morphs and 67 other material slots.
The doubles diagnostic gate now honors saved Use CSS choices on startup.
Commits: 6d74a49 and f41f4eb. Installation hashes and unchanged saves/settings
verified in work/cine-animation-port/cw-optical-fix-v4/installation/deployment.json.
Await user checks of CW Iris Out eyes and Faithful/Stray Use CSS summons.
Keep After Brute Battle end-of-loop crash investigation active.

## Completed storage cleanup, 7 October 2026

Owner approved removing the VaM installation/Wine environment and AssetRipper
temporary extraction data under `CustomShellSystem/work/v1.0.0-body`.
Removed only `vam/` and `assetripper/temp/`; 6,483 remaining files verified
unchanged. Net free space increased by 36.1 GiB; the folder is now 958 MiB.
Record: ignored `work/vam-cleanup-20261006T165343Z.json` (UTC timestamp).
Adult/body work is deferred until the owner requests it again.

## Latest user correction: four native locomotion choices only

For appearances using the default game human skeleton, implement only
**Eve Default Idle, Eve Walk, Eve Jog and Eve Sprint**, where technically
compatible. This applies to native, CSS and non-CSS appearances using that
skeleton. Validate their selection, smooth starting/stopping, restoration
and gameplay compatibility.

Do not expand native support to dances, photo poses, other idles or facial
expressions. Native Black Hole and montage experiments are retained as
historical evidence only; they are not release requirements or part of the
requested native selection. No remaining native dance batch should run.

The full requested animation library and feasible facial support remain in
scope for **Eve's six outfits, Unholy Genessa and Commander White**. Old
Seductress and GeneXXa remain excluded. This correction supersedes broader
native-coverage requirements and historical next steps below.

## Authoritative user scope correction, October 6

Support exactly these authored outfits: **Eve (all six outfits), Unholy Genessa
(both faces), and Commander White**. Also support appearances using the
**default game human skeleton** where technically compatible, whether native,
CSS or non-CSS. This does not authorize adapters for other custom rigs.

**Older Seductress, GeneXXa/Nunexxa and other unrelated custom-rig projects are
out of scope.** Do not work on, integrate or make them release prerequisites.
Do not use their unfinished evidence as a reason to delay this animation pack.
The old Seductress project is not the requested Eve `CSS_SeduXtress_eins0fx`.
Preserve existing evidence and source files; no further jobs or installation
for those excluded projects. The prepared Seductress S9 batch was never run.
This explicit correction overrides all earlier broad collection wording and
historical next-step notes below. All requested clips, photo poses, feasible
facial support, CSS/CINE integration and acceptance for the actual scope remain.

Historical checkpoints below are evidence only. Excluded-project instructions are cancelled.

## October 7: cleanup and Commander White facial checkpoint

Removed 3,247 full-resolution PNG intermediates from three rejected L5P
motion-review directories, totaling 1,985,384,005 bytes (1.85 GiB). Kept
58 representative frames, source data, commands and reports. The hashed
removal receipt is `work/cine-animation-port/cleanup-20261007-rejected-frames.json`.
Original assets, accepted candidates, installed mods and release archives were
not deleted. Large new facial trial assets and Unreal temporary save files
now use `/mnt/eins0fxE/CSS-authoring-scratch/cw-face-e2` through explicit binds.
Continue removing only inspected, obsolete intermediates as work proceeds.

Commander White's 512 expression/blink calibration samples now fit 30
corrective morphs, alongside 44 original control samples. These are 74 new
morph shapes for eight expressions, not 74 expressions. Compression residual
is below 0.001 cm. All 172 original comparisons and 204 fresh holdout checks
pass the unchanged 0.015 cm source comparison limit. The latter peak is
0.008444912 cm, including asymmetric blinks.

Unreal's existing morph filter would remove small corrections. Scaling stored
deltas and inversely scaling playback weights passes the filter simulation;
the original build threshold remains unchanged. The E3 candidate imports and
reloads with 90 total morphs, retaining the original 16 and the original
geometry, binding and cloth data. `cw-expression-native-v4` has successful
creation and fresh verification receipts. The prior v3 attempt failed during
saving because the system drive was full and is not accepted.

Fresh shared-path verification now passes for all 90 morphs, bindings and
2,312,092 render-to-source vertex mappings (`cw-expression-map-v2`, exit 0).
The first map check rejected this unchanged mesh solely because its diagnostic
limit was two million vertices. The bounded limit is now four million, with
exact map length and every source index still checked. Earlier failures remain
recorded. Independent cooked readback, performance-curve integration and live
acceptance are unfinished. Other evidence: `cw-expression-basis-holdout-v1`,
`cw-expression-compact-v2` and `cw-expression-native-v4`. Eleven focused Python
tests and the UE editor helper build pass. Nothing was installed.

## October 6: native locomotion runtime and combined candidate

The four native choices now use shared catalog resolution and independent
profile settings without requiring a wearable outfit manifest. Idle uses an
explicit validated BlendSpace carrier when the native post-process graph has
no CSS idle layer. Movement retains the existing blend ownership and gait
selector. The native graph is not replaced. Missing-pack choices persist,
Default remains selectable, and incompatible library entries are filtered.

`native-locomotion-trial-v1` contains 62 stable body/movement identities, 95
exports and four native alternatives. All 71 prior export payloads and five
requested-outfit adapters remain unchanged. The package passes container and
metadata readback; the combined catalog passes 1,125 checks and the pack alone
passes 1,114. The five focused data, profile, animation, library and runtime
tests pass. The Windows C++23 core builds. These checks do not establish
real-mesh contact, live graph behavior, smooth transitions or physics.

Nothing was installed. Continue native gameplay verification and the remaining
facial/source work for Eve, Unholy Genessa and Commander White. The full goal
and exclusions remain unchanged.

## October 6: four native locomotion choices, asset checks passed

The native Idle, Walk, Jog and Sprint candidates now have 20 retargeted
sequences and four carriers under `/Game/CSS/Locomotion/NativeHuman/Studies/NL2`.
The walk/jog/sprint carriers retain all 36 samples each; idle retains four.
All 624 blend queries match the original Eve setup, including direction
wrapping, clamped speed, weights and per-sample rates. Fresh-process readback
passes all four carriers and all 754 sequence frames.

The Windows cook and independent game-only package round-trip pass for 25
packages (20 sequences, four carriers, one codec). Complete decoded motion
passes all 754 frames and 1,208 tracks per frame. Maximum differences are
0.001314966 cm position, 0.000178365 quaternion distance and 6.04e-8 scale,
within the existing unchanged comparison limits. Dependency inspection finds
only the owned animation assets, codec and canonical game skeleton. No outfit
or diagnostic skeleton/mesh ships in this study. Protected inputs and the
ordinary authoring skeleton placeholder remain unchanged.

Evidence:

- `native-locomotion-v4`: 20 sequences, exact binding checks, successful exit.
- `native-locomotion-blends-v3`: four carriers, 624 matching blend queries.
- `native-locomotion-readback-v1`: fresh reload and full raw captures.
- `native-locomotion-cook-v1`: successful cook, `pack/verification.json`,
  `decoded/decoded.json`, `full-motion-verification.json` and
  `dependency-verification.json`.

The first bind retained an editor FK Control Rig bound to the diagnostic
skeleton. Rebuilding the model introduced a tiny Euler round-trip difference
and was rejected. The accepted helper instead duplicates the model, gives its
private FK rig a new binding to the final skeleton, and retains exact authored
key checks. The failed attempts remain recorded; their assets are not used.
The blend-script retries corrected Python editor-property access and explicit
axis-value comparison, without changing the interpolation checks.

Next: integrate the four choices with CSS selection/profile persistence and
native idle playback, merge the accepted assets into the shared candidate,
then check real-mesh contact, transitions, physics and live restoration.
These are asset checks, not proof of finished native gameplay support. No game
installation occurred. Full animation/facial work on the three requested
outfits and the unresolved source mappings remain active; do not resume
excluded rigs or expand native performance scope.


## October 6: native playback and catalog candidate

The owned native montage route is implemented for CSS and CINE without a CSS
outfit requirement. It preserves the player's native post-process graph and
stops only its own montage. Thirteen isolated UE 5.6.1 lifecycle checks pass
in `native-montage-v2`; the first fixture's missing event-dispatch cleanup is
documented. Windows compilation and the focused host/CINE tests pass.

`native-performance-trial-v1` adds the verified native Black Hole sequence to
the same stable catalog entry, preserving all 71 existing exports and the five
requested-outfit adapters. Combined and pack-only catalog checks pass 1,093
and 1,082 assertions. No installation. Other native clips, real game-graph and
visible-mesh physics checks, live acceptance, source accounting and remaining
facial work are unfinished. See the [native support record](development/shared-animation-native-human.md).

## October 6: default-human reference audition

The native 1,199-bone reference and nine virtual bones now match the game
dump in a fresh editor check. UE strips six mirrored accessory scales during
IK retargeting; the new opt-in preservation path restores those branches.
The full Black Hole raw audition passes all 601 frames, with every other raw
bone unchanged. See [native support evidence](development/shared-animation-native-human.md).
Fresh precise-compression evaluation passes all 601 frames and 1,208 bones;
the default Eve retarget retains all 233,188 authored keys exactly. Default
compression failed the rotation limit; precise L5X settings pass without
relaxing it. Results and exits are in `native-retarget-v1`. The saved native
audition now has a separate precise-codec native binding with a successful
Windows cook. `native-binding-v6` passes full independent comparison of 601
frames and 1,208 tracks, with only the game skeleton and owned codec as imports.
The ordinary authoring skeleton placeholder stays unchanged. The decoder now
reconstructs native virtual-bone reference locals; both complete Eve dance
readbacks remain byte-identical. Native runtime ownership, real meshes and
live acceptance remain.
Do not resume excluded custom rigs.

## October 6: requested-outfit dance integration

The combined private trial now has 59 animations: the original 57 plus the
full L5X DumbDumb and Iris Out body auditions. All 68 prior cooked exports and
57 existing catalog entries stay unchanged; five adapters cover only Eve,
Unholy Genessa and Commander White. The two dances and their owned compression
settings bring the package to 71 exports. No excluded outfit is included.

Fresh current-module evaluation passes 18 mesh/clip pairs, 162 playback and
restoration cases, with fingers checked. Packaging round-trip and independent
readback with each of the three outfit mods alone pass. The host catalog passes
1,090 combined checks and 1,088/1,080/1,080 isolated checks for Eve/Genessa/CW.
Evidence: `l5x-adapter-v1/corrected-current-result.json`,
`shared-dance-trial-v1/verification.json`, `shared-dance-isolated-v1/report.json`.
The first packaging preflight correctly rejected evidence tied to the older
authoring module; the fresh evaluation supplies the current evidence.

This trial is not installed or release-accepted. Unknown source indices and
remaining facial controls are retained as open items. Next implement the
compatible default-human playback path, complete feasible facial support and
live acceptance for the actual scope. Do not resume excluded custom rigs.

## October 5: shared locomotion pack and questman's follow-up

Seductress S8 now also passes full raw/cooked comparison: 4,641 keys,
388 tracks, maximum position error 0.003582 cm. The independent 480 Hz
cooked scan covers 9,281 poses; visible foot materials 23/24/27 have worst
penetration 0.025908 cm, with no samples below -0.1 cm. Evidence lives in
`s8-shared-cook-v3/{full-motion-comparison,cooked-visible-contact-summary}.json`.
The BodyHairV42 playback adapter now passes Windows cook and independent
comparison against the released outfit graph, including 23 serialized CSS
defaults and the original secondary-motion nodes. The three legacy rig
dependencies are explicitly hashed and left unchanged. See
`seductress-adapter-cook-v2/probe/verified-binding.json`. This is structural
physics preservation, not live physics acceptance. Remaining clips, footwear,
catalog integration and live checks still prevent release; nothing installed.
Both cook and package jobs finished successfully. Continue broader clip
coverage before returning to the small CW facial interpolation residual.

Seductress S8 cook integration now passes. `CSSCookAssets` accepts explicitly
hashed external skeleton/retarget-mesh dependencies without recooking them.
An isolated `/Game/CSS/Locomotion/Studies/S8/AN_ComeCloserFlat` candidate clears
only its editor preview mesh, keeps the retarget source and original tracks,
and owns its compression settings. Fresh comparison retains all 388 tracks
and 4,641 keys. Windows animation/settings cooks, two-export container
round-trip and full 240 Hz decoding pass. The private package is 1,645,446 bytes;
41 protected authoring inputs and the released outfit containers are unchanged.
See `s8-shared-cook-v3/acceptance-checkpoint.json`. The decoded skeleton resolves
to the original legacy package. Next capture unretargeted raw poses, compare
every cooked pose and contact, then integrate the broader adapter/clip set.
This study is not installed or live-accepted. All jobs for this checkpoint are
terminal; failed earlier runs are retained in `s8-external-cook-v1` and
`s8-shared-cook-v1/v2`. Other outfit/native support and facial work remain open.

CW blend diagnosis: exact independent left/right source pairs reproduce all
nine failing bilateral poses with zero measured residual. The remaining error
is interpolation, not interaction between eyelids. The optional not-a-knot
cubic comparator reduces the wide-matrix failures from nine to three, maximum
0.017902 cm, still above the unchanged 0.015 cm limit. Four mathematical basis
checks pass. Evidence: `cw-expression-pair-diagnostic-v1/pair-additivity.json`
and `cw-expression-blend-grid-v1/cubic-v5-result.json`. This has no native or
runtime deployment. Further facial refinement remains open; continue the
Seductress external-dependency cook and pack integration before another facial
calibration pass. Report concrete milestones rather than an unmeasured overall
percentage.

Latest CW continuation: eight original expression controls are now sampled,
visually reviewed and imported alongside jaw/blinks into the isolated CWE1
trial (44 added shapes, 60 total morphs). Creation and fresh readback preserve
the complete base description, 69 built sections, cloth properties and existing
bindings. `cw-expression-native-v1` has successful terminal receipts; nothing
was cooked, catalog-bound or installed. Source single-control interpolation
passes at all 44 tested intermediate strengths.

Combined expressions and blinks require additional corrections. The source
adds 0.1 blink for Flirting and 0.15 for Frown, then caps closure. Driver
evidence and evaluated shape values establish this behavior. Normalizing the
corrective grid by remaining closure fixes the earlier saturation error. A
small eight-case check passes, but the wider 128-combination check still fails
9 cases at the unchanged 0.015 cm limit, with maximum 0.025027 cm. Preserve
`cw-expression-blend-grid-v1/grid-v5-result.json` and all earlier failures.
Next inspect those strong-expression intermediate poses and refine the
correction representation. The 256 calibration corrections cannot be shipped
as individual morphs under the existing 128-shape limit; do not raise that
limit or accept the smaller passing test merely to finish the trial. The
correction study is Python/source geometry only, not native or runtime support.

October 6 continuation: Commander White's original jaw and left/right blink
controls now have an isolated, saved 12-shape trial. The source evaluation
combines joint movement and corrective shapes. Fitted correspondence accounts
for the recorded 547-point skirt insertion and matches the current C4 assembly
within 0.000007645 cm. Original sources and installed files remain unchanged.
`cw-facial-native-v3` records creation and fresh readback, retaining all 16
existing morphs. `cw-facial-preservation-v2` proves the remaining mesh description,
69 built sections and stored cloth properties match. The same preservation
check passes all six existing Eve facial meshes in
`cw-facial-preservation-legacy-v1`. Eighteen strict schema rejection cases pass;
the earlier JSON boolean coercion failure is fixed and its receipt retained.

`cw-facial-interpolation-v1` evaluates independent eighth-weight source samples
and combined jaw/blinks. Quarter-weight interpolation has maximum point error
0.010966 cm against a declared 0.015 cm limit. Combined-control interaction is
at most 0.000179 cm at the four tested strengths. This measures the source
approximation, not cooked morph filtering or in-game facial motion. Next expand
verified source controls and character-specific mappings, then cook and check
runtime restoration/physics. Unknown source tracks, the older Seductress cook
dependencies, native-human ownership, wider compatibility and full release
acceptance remain open. See the updated coverage matrix and
`tools/authoring-probes/animations/FACIAL.md`.

L5X full previews are now terminal, both render and encoding exits zero.
`l5x-cook/full-render-review.json` verifies complete videos: DumbDumb 1,972
frames / 65.733333 s and Iris Out 501 frames / 16.7 s, both 30 FPS at original
speed (including the final held frame). Maximum pose reconstruction errors
are 0.001047 and 0.000938 cm. Both sampled review sheets were inspected with
no obvious limb collapse. This remains neutral geometry without cloth
simulation, not complete temporal or live acceptance. No render jobs remain.

Commander White facial source recovered: `cw-facial-drivers-v1` includes the
original DAZ/Rigify armature-data drivers, read with embedded scripts disabled.
Jaw and blink combine joint movement with corrective shapes, so exporting only
the retained B9 shape names would lose part of the expression. Next evaluate
verified original controls and bake their combined deformation onto audited
fitted correspondences. Source bytes are unchanged; no facial candidate is
installed. DumbDumb L5X's complete 1,972-frame neutral preview is encoded at
30 FPS (`l5x-cook/dumbdumb-full-review.mp4`); sampled visual review finds no
obvious limb collapse. This does not establish temporal or gameplay acceptance.

Unholy facial audit correction: its Eve and Genessa choices are body skin
variants sharing the native mask, not two different faces. The source's 105
matching Eve shape names do not establish visible facial support. Repeating
the original export filtering leaves 87 controls on body/head backing and
removes 18 mouth controls entirely; the separate 1,303-vertex face and
124-vertex eye objects have no shape keys. No derived mesh was imported or
installed. Keep body playback independent and preserve the native mask;
facial support needs actual mask controls rather than an Eve-face substitution.
Evidence: `unholy-facial-v2/export-report.json` and the updated outfit coverage
and acceptance matrix. L5X full renders continue on their existing process.

Latest L5X continuation: localized corrections now pass fresh authored repeat,
Windows cooking, the three-export package round-trip, full cooked-key comparison,
and a 240 Hz neutral footwear scan. Across 15,769 DumbDumb and 4,001 Iris Out
poses, material 20 remains within 0.030 cm of the floor, with no samples below
-0.1 cm. The matched cooked-key knee check adds no flips. Both clips retain
their original duration and 60 Hz keys, with only six leg tracks eligible for
changes. Evidence: `retarget-l5x`, `l5x-cook/footwear-contact-summary.json`,
`l5x-cook/knee-motion.json`, and both full comparison reports beneath
`l5x-cook/decoded-runtime-v1`. The nine-mesh adapter run passed all 162 cases
and 44,496 full-clip poses in `l5x-adapter-v1`, with successful process exit.
Full fitted-mesh renders are active under `l5x-cook` using
hashed cooked-pose captures from `mesh-motion`. Poll their existing processes
and inspect results before continuing; do not restart on stale log timing.
This is not live acceptance or installation. Other footwear/proportions,
remaining rigs, facial mappings, source definitions and release gates remain.

Latest dense/contact continuation: the corrected L5W Iris Out and
L5UPrecision2 DumbDumb pass all nine outfit meshes, 162 adapter cases and
44,496 full-clip poses (`corrected-dances-adapter-v1`). Independent cooked ACL
sampling now covers 120 Hz, including between-key poses. All original-key
transforms and five-sample JSON remain bit-identical to the verified 60 Hz
decodes. Mesh translation retargeting agrees with raw UE poses within
0.000020 cm. The neutral cooked Black Pearl footwear section (material 20)
still dips between keys: Iris Out left/right -0.18258/-0.27834 cm, DumbDumb
-0.42759/-0.42878 cm. At source keys these same surfaces are within 0.008 cm.
Larger negative values belong to alternate flat/covered-foot sections, so keep
them separate from the active heeled footwear. Continue a localized contact
correction and verify its full motion, dense cook and adapters; neither dance
is installed or release-accepted. Results: each study's `decoded-dense-v1`,
`current-eve-skin-audit-v1`, and `dense-acl-regression.json`. Broader rigs,
facial mappings, unresolved source controls and live/release acceptance remain.

Latest October 6 continuation: Iris Out L5W repairs only four right-leg keys
at the two near-straight transitions. All 1,001 authored contact poses and
fresh-repeat checks pass; the correction is committed as `758f5ac`. Windows
cooking, package export round-trip and full 388-track compressed comparison
also pass when sampled with Unreal's linear ACL policy against unretargeted
raw poses. DumbDumb's L5UPrecision2 copy also passes all 3,943 key times after
tightening its owned ACL codec threshold from 0.01 to 0.001 cm. Its animation
is 1,280,027 bytes, plus a separately cooked settings asset included in the
verified package. These are still offline candidates, not installed or accepted.
The new full-key check exposed two diagnostic mismatches: CUE4Parse's nearest
ACL sampling skips interpolation over stripped keys, and mesh-contact poses
include retargeting. Both are now explicit in the comparison tools. Preserve
the failed diagnostic reports. Continue dense/cooked contact, updated adapter playback,
facial mappings, remaining source definitions, broader rig/native support and
live/release acceptance. Older checkpoints below retain their original scope.

Seductress contact update: S8 Come Closer Flat now passes exact fresh repeat
for all 388 tracks and 4,641 keys. The 480 Hz scan reduces visible-footwear
penetration to 0.02607 cm, with no new knee-plane flips at matched 240 Hz
samples. A neutral 60 FPS lower-body render is saved and its sampled sheet
reviewed. All six older outfit graphs pass 54 compressed component cases,
15,606 poses and 38-finger checks, including distinct-base restoration.
This is one flat-foot diagnostic candidate, not full compatibility acceptance.
Stop increasing density for this clip: next verify Windows compression/contact
and cost. The existing cook path rejects this rig's released legacy skeleton
under `/Game/CSSAuthoring/`; support audited external dependencies without
rewriting old outfit packages or removing the portable-path guard. Continue
the remaining footwear/clips and DumbDumb/Iris Out source-plane correction.
Evidence: `retarget-seductress-s8/`, `CONTACT.md`, and the coverage matrix.
No runtime, released outfit or game installation changed.

DumbDumb/Iris Out contact continuation: isolated L5U repeats the native
source-plane correction. Both full clips pass fresh authored readback and
exact non-leg pose preservation. DumbDumb clears all 3,943 measured key poses
within 0.000190 cm and removes all ten L5T-introduced knee flips. Iris Out still
has -0.217425 cm right-foot contact at frame 76 and amplified knee motion at
77; source-plane correction alone is insufficient there. Side-view comparison
frames 72-81 and full knee metrics are saved. All jobs exited zero. Next check
DumbDumb between-key/compressed contact and adapter playback; localize Iris's
near-straight transition before another bake. Do not treat either clip as
accepted or installed. See `retarget-l5u/` and CONTACT.md.

Older Seductress continuation: three full direct-source retarget auditions now
pass fresh saved/repeated readback, and S3 passes 162 adapter cases across all
six meshes, including 38 finger bones and restoration. All 27 CSS controls and
31 original graph nodes remain preserved. Cooked surface measurements still
find foot/footwear penetration; separately, foot-weighted skirt sections have
larger excursions. An independent matrix check confirms the measurement.
Continue visible-footwear/contact correction and full-motion review before
catalog activation. No released weights, package or game installation changed.
See the older Seductress section of the outfit coverage matrix for receipts.

October 6 requested showcase: generated offline vertical Black Hole body/finger
bone and Come Closer facial videos in `work/cine-animation-port/reels-20261006`.
User requested the Commander White CINE callout design, no Eve label, all
component arrows visible together, and corrected signature punctuation. The
renderers reuse CINE's Rajdhani font and layout constants; no CINE runtime or
game state changed. The facial clip uses source seconds 14.0 to 19.333 at
original speed. These renders are presentation artifacts, not new animation
or outfit compatibility acceptance. The full shared-pack goal remains active.

GeneXXa shared-pack integration now passes: the staged
`shared-v1/pack-with-genexxa-v1` has six adapters, with all 68 older export
payloads unchanged. Isolated dependency readback succeeds for Eve, Unholy
Genessa, Commander White and GeneXXa. The host catalog resolves the combined
set and each outfit alone, with no fabricated facial support. Seven evidence
validation tests cover changed files, missing clips/meshes, failed restoration,
finger failures and bind-mount identity. Packaging now protects all evaluated
inputs (46 hashes for this addition), not only the generated graph. No new
installation or gameplay acceptance. Other authored/native rigs and the
DumbDumb/Iris Out contact work remain active requirements.

Superseded scope interpretation (cancelled by the user): support all original eins0fx-authored CSS mods,
excluding port-only mods, plus other authors' CSS mods, non-CSS mods and native
appearances using the default game human skeleton where compatible. The current
nine meshes are a test set, not the complete support list. Inventory the full
original collection and verify actual rig/layer contracts. Facial compatibility
is separate and must not prevent compatible body playback. This requirement is
now recorded in both the repository goal and the active Codex goal attachment.

Runtime continuation: bound performance layers now resolve from audited catalog
entries instead of five hardcoded classes. Zero-control fixed-physics graphs
are permitted with exact package/class/skeleton matching and bidirectional
control-layout checks. Animation-library and performance-session host tests
pass; the Windows C++23 core builds. Not installed or live-accepted. Generic
native/non-CSS ownership and sequence compatibility remain required work.

Expanded implementation checkpoint: the GeneXXa fixed-physics graph now has an
isolated performance adapter. Creation, fresh reload and all 20 moving clips
on both GeneXXa/Nunexxa meshes pass (40 pairs, 360 cases, 29,278 poses). No new
runtime integration or installation yet. The additional authored collection
and native-human rig inventory is in
[outfit coverage](development/shared-animation-outfit-coverage.md). Older
Seductress references and native-human graphs require deliberate adapters or
retargeting. A stale v2.0.6 local package is retained as unverified.

DumbDumb and Iris Out now pass isolated Windows cook, export round-trip and
independent compressed-pose samples. Full motion renders continue. Measurement
found a -14.31 cm shoe height in DumbDumb at frame 3414; the source foot joint
remains near its usual ground height there, so inspect retargeted leg contact
before acceptance. The Blender reconstruction error peaks at 0.0010075 cm;
the repeat visualization uses an explicit 0.002 cm diagnostic limit, with
original motion unchanged. Raw editor and compressed-animation checks keep
their existing independent limits.

Contact localization: `retarget-l5p/contact-comparison.json` now confirms the
issue in raw UE retarget output, before gameplay or Blender. At 56.9 seconds,
the source feet remain at about 14.86/14.79 cm, while target feet are
-5.77/27.37 cm. Enabling/disabling translation retargeting in pose readback
does not change either result. Source import agrees with independently decoded
joint samples. Next inspect the IK retarget chain/goal handling and correct leg
contact in a new candidate, preserving source timing and the existing audition.
DumbDumb's full 1,972-frame and Iris Out's 501-frame diagnostic renders completed.

Contact repair continuation: changing FK rotation modes did not solve the
problem. A world-preserving source-hierarchy experiment (L5Q) improved the worst
shoe penetration only to -7.17 cm and failed the strict UE source-key comparison
for DumbDumb. It remains experimental. Correctly initialized foot IK on the
unchanged L5P source restores the problem-frame foot heights to 7.78/7.71 cm.
Both full L5R candidates are generated. The inspected problem-frame shoe depth
is now -0.48 cm, from -14.31 cm. Fresh-process saved/repeated-pose verification
passes both clips (4,944 frames, 388 bones, zero differences). Dense skinned
measurements completed but still find -4.317 cm in DumbDumb and -1.209 cm in
Iris Out. A rotating local-offset trial improves these sparse cases but leaves
-3.841 cm on DumbDumb's left foot. Per-foot contact measurement and a separate
leg-IK diagnostic led to L5T full candidates. Their fresh saved/repeated poses
and non-leg preservation checks pass, but complete scans still find mixed-weight
heel-support penetration (-0.364/-0.217 cm) and magnified knee direction changes.
Neither candidate is accepted. Investigate source knee behavior and target
proportions alongside contact, without changing released outfit weights. See
[the source/contact investigation](development/shared-animation-source-requirements.md#leg-contact-investigation-october-6).
No new installation or release acceptance yet.

October 6 continuation: the user confirmed source permission and requested
continued work on DumbDumb and Iris Out. Investigate the seven additional tracks
against the actual source mesh and playback mapping; retain their complete keys
and do not invent bone identities. Source permission is no longer a pending user
question. All six Eve outfits, both Unholy Genessa meshes and Commander White
remain required compatibility cases. Black Pearl was the first requested live
facial test, not a reduction of supported scope. Validate facial compatibility
separately for each character, including any derived assets it needs.

New offline progress: full-length DumbDumb and Iris Out body auditions are
generated as L5P and pass fresh-process readback plus 60 Hz sampling checks.
All 158 source tracks per clip are preserved across the body projection and
its companion record. This does not identify the seven extra controls or
finish facial animation. See [source requirements and audition evidence](development/shared-animation-source-requirements.md).
Neither audition is installed or added to the shared release catalog yet.

The new two-clip adapter run has now passed all nine meshes, 162 cases and
44,496 full-motion poses. Separate facial source inspection confirms retained
Eve shapes in Unholy Genessa's Eve-face source, different DAZ/FACS controls in
Commander White, and no existing expression rig in Genessa's mask. Keep those
character-specific facial tasks active. See the acceptance document's required
outfit coverage table. The full DumbDumb render stopped at frame 1664 on a
0.0010039 cm reconstruction error against its 0.001 cm diagnostic limit;
a measurement-only run is checking the full error distribution before deciding
whether a numerical reconstruction correction is needed. This does not change
the animation data or count the unfinished render as accepted.

Current checkpoint, October 6: the combined private trial is installed with
57 shared sequences, five outfit adapters, 21 Eve expressions and Come Closer
facial playback. Gesture 200 facial data is also installed through a
verified catalog-only revision. The stopping-selector correction is also installed; visual gameplay
acceptance is still pending. The historical planning notes below are not the
current implementation status. Use the [per-clip acceptance table](development/shared-animation-acceptance.md)
and its linked evidence before repeating work or claiming completion.

Active animation work is defined in
[the CSS/CINE port goal](../extensions/cine/docs/ANIMATION-PORT-GOAL.md).
Preserve native source sample rates; the user's 60 fps remark was a question,
not a requirement to upsample every clip. Complete the current source inventory
and retargeting work before starting the related stop-transition investigation.

The user corrected the photo-pose answer: include feasible facial-expression
support too. Keep the 31 photo poses and investigate all 21 referenced facial
expressions for CSS and CINE. Exact Eve v1.4.0 readback confirms the six released
meshes lack these facial morphs, although the retained authoring body contains
matching shapes. Plan derived face-capable assets and explicit compatibility,
with Neutral separate from Reset. Continue the current body-animation batch
first; facial controls are not implemented yet.

The user explicitly asks to include questman's Eve stopping-pop report when
convenient. Carry the evidence and regression cases from the older queue entry
below into this animation work. Fix the brief jog/back-foot lift during stopping
after reproducing its actual state/phase cause. Do not call the reporter's setup
incorrect or treat a higher sample rate as a fix.

Separate follow-ups after the shared animation pack:


- Investigate an optional face/body rim light, including world versus preview
  scope, performance and restoring the original lighting on disable/switch.
- Investigate an optional body oil material control with outfit compatibility,
  saved-setting support and clean restoration.
- Investigate HIT2 DE Scyther body tint not affecting the face and the requested
  lighter skin option. Inspect its existing face/skin controls and original
  materials before choosing a CSS runtime or outfit-package fix. Preserve
  XTGMods attribution and the existing default appearance.

These are queued requests, not implemented features or release promises.

## Beta.11 release decision and beta.12 cloth follow-up

The author accepts both forms on trial8 and explicitly requests finishing
beta.11 with the remaining full-cloth creation hitch documented. Keep full cloth.
Faithful cached reuse is verified; fresh construction still costs about 80-100 ms
on this machine. Investigate a bounded CSS-owned visual pool across native clone
lifetimes for beta.12, without changing ability lifetime or gameplay. Verify
pose-source rebinding, cloth reset, hidden tick cost, material refresh, context
cleanup and memory bounds before replacing the same-actor cache.

## Queued for beta.12 consideration: Eve stopping transition

questman reports beta.10 with Eve selected in all locomotion slots: starting
movement looks natural, but stopping briefly shows the jog/run-start back-foot
lift. The author previously described the transitions as unfinished and now
asks whether this is a quick fix or should wait for beta.12. Keep beta.11's
full-cloth doubles validation first; no locomotion change is installed for this
report.

Initial code review: `animation_gait` selects Jog whenever speed exceeds 20 and
both walking/sprinting flags are false. `WalkOverride::update` immediately
selects that gait's custom blendspace. Releasing a movement mode while velocity
is still decaying could therefore briefly select Jog before Idle. This is a
hypothesis, not a reproduced cause or evidence that the user's install is wrong.
Capture speed, both flags, selected gait and blendspace through stopping; inspect
playback phase reset and the native stop transition before changing thresholds
or holding a previous gait. Check walk/jog/sprint stops, sudden reversal, slow
controller input, sidearm aiming, attacks, mixed Default/custom slots and native
locomotion. Avoid masking the pop by introducing foot sliding or delayed idle.

Epic's [animation sync guidance](https://dev.epicgames.com/documentation/unreal-engine/animation-sync-groups-in-unreal-engine)
describes how unsynchronized foot phases can make gait blending unnatural.
Verify any selected API or graph change against local UE 5.6.1 source/assets;
the current online landing page defaults to a newer engine version.

## October 5: release-wide performance and regression review

The author requests a broader CSS C++/UE 5.6.1 review, performance checks and
regression coverage before beta.11. Finish the measured summon-creation hitch
first, then run the applicable full suite and review lifecycle, profile/UI,
appearance recovery and extension compatibility. State what was tested and any
remaining gaps; do not promise zero bugs or frame drops in every game state.
Release FAQ must explain current CSS customization copying and distinguish
native copied attacks reflecting CCS from universal CCS settings inheritance.

Full cloth physics is mandatory for doubles. The author rejected removing it
after the hidden-component timing comparison. The current candidate retains
stopped visuals only on the same native cached double, with identity/settings
checks, fresh materials per activation and bounded expiry. Faithful cache reuse
and both forms' cloth continuity now pass live checks; fresh construction still
hitches. The author defers further optimization to beta.12.
See the doubles performance investigation for scope and remaining coverage.

## October 5: beta.11 release media and FAQ

The author requests two GIFs, each below 8 MB, from
`/home/eins0fx/Videos/screenrecording-2026-10-05_07-07-35.mp4`:
Faithful 00:03-00:06 and Stray 00:23-00:26. Combine the six October 5
screenshots (07-02-28, 07-01-34, 07-00-17, 06-46-27, 06-41-34, 06-39-18)
from `/home/eins0fx/Pictures/` into one image with no captions inside it.
Place the compatibility description below the image in BBCode and add a short
FAQ. Confirmed outfit tests cover Unholy Genessa, Eve and Commander White;
do not imply all authored or third-party CSS outfits are verified. Continue
the repeated-summon timing investigation before finalizing beta.11.

## October 5: doubles trial6 acceptance

The author subsequently confirms some NPC/enemy appearances work only partially
and explicitly defers their compatibility fixes. These are follow-up work, not
a blocker for the current release. MISC and release notes must distinguish
partial visual/animation compatibility from complete original-double fallback.

Commander White is accepted in both forms, following Unholy Genessa and Eve.
The author also accepts shape and movement through the CSS menu transition.
Trial6 is installed and its loaded core hash is verified. It includes the MISC
explanation of supported appearances and original-double fallback. Sariel and
Proxima work; the author accepts unsupported appearances such as Lazlo falling
back. Preserve the diagnostic pose differences for investigation without treating
them as a user-visible failure. Active-summon performance and remaining regression
checks are still open. See the latest doubles status for evidence and scope.

## October 5: distorted Stray doubles across outfits

The author confirms all copies and all CSS outfits show broken shape and missing
feet. Fix this within the active doubles task before broader acceptance. The
native attack changes its pose leader to the player; Copy Pose uses native clone
bone indices against that different mesh. Evidence and the uninstalled candidate
are in [pose ownership](development/genessa-doubles-pose.md). Form switching and
both Default/Use CSS selectors were accepted, but Stray visual acceptance is
withdrawn. Development/shipping builds and 18 host tests pass; live retest remains.

Trial5 is installed and the author accepts both Stray shapes and Eve in both
forms. A world-only UG capture matches 520 bone positions; the earlier mixed
menu/form sequence still contains initial-pose discrepancies to investigate.
Original-shell testing exposed Driver1 and null-slot material admission gaps.
These remain part of the active doubles work, with details in its status doc.

## October 5: Genessa form utility for doubles testing

The author accepted both Faithful Doubles appearance choices, but deliberately
dying to reach Stray makes the remaining test difficult. They request a MISC
Genessa Form row with Auto / Default (game), Faithful and Stray. Asked to choose
between a persistent lock and a one-time switch, the author delegated the
decision. Recommended behavior: switch once, show the observed current form,
then let the game handle subsequent death, revival and travel. Do not introduce
a per-frame form lock or save this gameplay action into appearance profiles.
Inspect and use the native state transition, not a mesh/tag overwrite. Initial
controller bytecode contains ActivateDarkform and ActivateShell; RemoveShell
also clears abilities and saved shell state, so it is not a safe shortcut.
Continue this supporting task within the active doubles work. Existing Default /
Use CSS controls remain independent. Stray selector acceptance is still pending.

## October 5: reduce restarts during native development

The author renewed the request to use hot reload instead of restarting for
every change. Preserve the active doubles priority. Existing `tools/css.py reload`
can replace the CSS core, but the September 20 animation-worker crash remains
unresolved in [the reload investigation](development/dll-reload.md). Validate
cleanup and animation-worker lifetimes before using it for routine core swaps.
The current doubles cleanup correction was installed with the game closed.
Trial enable/disable and existing runtime settings can be changed live without
replacing the DLL. Loader/UE4SS and mounted cooked-asset changes still require
a restart in the current supported workflow.

## October 5: Eve asset-load report, after the doubles work

Follow-up: the actual Eve v1.3.0/v1.4.0 ZIPs pass isolated catalog-export and
public-import checks. The reporter's cause remains unconfirmed without the full
path, versions and logs. See [audit results](development/evetest-load-report.md).
The author requested a reply only; do not add this to the BBCode changelog.

Didodiodi reports `Asset could not load: /Game/CSS/EveTest/` in the bottom-right
UI (Nexus, October 5 at 12:13 AM). CSS/Eve versions and the full failing asset
name are not yet known. The author asks whether this is hardcoded and requests
a fix after the active doubles work. Initial exact-string search finds no
`/Game/CSS/EveTest/` path in `native/src` or `packaging`; historical Eve authoring
tools do use that namespace. This does not establish the failing path's origin
or imply the installed Eve package is complete. Trace the reported load through
the released catalog, saved settings/profile, ZIP contents and mounted assets
before changing any paths. Do not rename valid cooked dependencies simply
because their namespace contains `Test`.

The author confirmed the game is closed and authorized installing the doubles
trial later, once ready. Preserve beta.10 and verify the installed hashes.

## October 4: Genessa doubles inherit CSS appearance

After accepting beta.10, the author requested Faithful and Stray doubles that
match the current customized Unholy Genessa appearance while retaining native
blue/red ghost effects and ability behavior. Initial dump inspection found
spawn, cache/reuse, ghost-material and fade interfaces. Full cooked-graph tracing
and live verification remain. The requested updated goal is recorded in
[Genessa doubles](development/genessa-doubles-goal.md). The existing gameplay
clipping investigation remains pending. Eve v1.4.0 and Commander White
v0.0.6-dev material support are also required, including their distinct hair
graphs, current colors and modular parts. Material experiments and lifecycle
observation are in development; no appearance transfer is installed yet.

## October 4: intermittent original-shell appearance after gates

The author clarified that the outfit reverts to the original shell. Captured
logs show successful restores but do not retain the failed state. A candidate
now recognizes the current shell definition's default mesh as well as CSS's
captured original, covering delayed resets after the transition edge. Host
regression and production build pass. The candidate was installed with the game
closed and its hash verified. The author tested it, reported "ok seems fixed",
and authorized beta.10. The live log confirms a delayed stock reset was recovered.
See [evidence and limits](development/intermittent-gate-appearance.md).
Genessa gameplay clipping remains queued below.

## October 4: profile input conflict, then Genessa gameplay clipping

Priority 1: the author reproduced the profile issue. The Save prompt advertises
Space, which inserts a space while the name field has focus; the typing guard
suppresses the save binding. Earlier validation messages did not fix this.
Use Enter for keyboard save, including with the field focused, and preserve
mouse/controller saving. The corrected candidate was installed and the author
confirmed that saving works. Separate mouse/controller acceptance is not implied.
A subsequently reported row-label overlap was repaired and installed:
menu invalidation now hides retained row status labels before clearing
the cache, preventing MISC "When in use" from leaking into SHELL.

Priority 2: Unholy Genessa's metallic/glowing outfit parts fit in Inventory/CSS
but clip in gameplay. Compare identical morph settings, component poses, weights,
physics and update order between preview and world before changing mesh fitting.
The author supplied paired screenshots at glute size 1.00. Preserve current
settings and accepted assets; finish the profile interaction first.

## October 4: new-game Harbinger regression

Live capture identified my broadened traversal guard as the cause of blocked
SHELL changes. `GA_ShellTraversal_StrongOne` is a persistent event listener with
ActiveCount 1 at rest. Removed that family from the blocking predicate while
retaining actual jump-action guards and the aiming repair. Five relevant host
suites pass. The corrected build is installed, and the author confirmed the
Harbinger SHELL-switching fix works in-game. Other pending fixes still need their
own live checks. See
[the regression repair](development/harbinger-start-regression.md).

## October 4: profile save validation report

Markuzkiller reports an invalid-slot error with only one visible row, probably
on beta.7 or beta.8. Both reject spaces in names and share a generic validation
error. Added specific save errors and a clearer input hint; host save/round-trip
and capacity regressions pass. A failure with a valid name is not reproduced.
Await the reporter's exact name or valid-name retry before attributing this to
a storage bug. See [profile save findings](development/profile-save-report.md).

## October 4: Heart of Vatra belt accessory

The reporter's NG+ quest heart is missed by MISC because its actor attaches
through a plain scene root. Added exact actor recognition and a separate
**Heart of Vatra** accessory row, avoiding its misleading seal socket and
keeping Revered Heart independent. The regression fails before the fix and
passes after; the production build passes. Live NG+ hide/show acceptance is
pending. See [Vatra visibility findings](development/vatra-heart-visibility.md).

## October 4: selected animations after revival

The author reports losing Eve Default Idle after death/Harbinger revival, and
asks to cover walk, jog and sprint too. A host regression confirmed that the
outfit was mirrored from the living shell while animation lookup used the
Harbinger's independent slot. Both now share the same resolver. Production build
and 5/5 relevant host suites pass; the combined candidate is installed for live
review. See [revival animation findings](development/harbinger-animation-recovery.md).

## October 4: sidearm aim after forced Harbinger traversal

Follow-up to the beta.9 appearance-swap fix: the reporter says short/long jump
gates and air-current jumps break aiming again after forcing Harbinger form.
Capture the equipped aiming layer before and after traversal, distinguish game
resets from CSS recovery, then repair the responsible path without replaying
weapon equip or carrying transient locomotion layers into normal gameplay.
Evidence goes in `work/sidearm-traversal/`. The user requested offline inspection
instead of searching for a gate. Cooked controller bytecode exposes the missing
layer preservation. The initial broadening of the traversal guard to persistent
`GA_ShellTraversal` listeners was incorrect and is superseded by the regression
repair above. The follow-up is built as a test core, with 5/5 host
suites passing. Live acceptance is pending; the public ZIP is unchanged. See
[sidearm transition findings](development/sidearm-mesh-transition.md#october-4-forced-harbinger-transitions).

## October 2: review and commit the CSS work

The user requested review and commits for the other agent's CSS changes, with
C++23/UE 5.6.1 compatibility, regression coverage and performance checks.
Socket-fit and Gragu helmet changes were reviewed separately against standards
and their specification. Corrections cover repeated stows, transform ownership,
component replacement, held-socket exclusion and cleanup of owned helmets.
See [review and release gates](development/socket-fit-review.md).
The reviewed cloth-overlay implementation is now merged into the normal
`nextgen100` checkout through `eca8c43`. The linked authoring worktree remains
at `work/cloth-core`. A private production-core patch ZIP is prepared under
`dist/unholygenessa-v0.1.0-trial.4`; restart testing is pending. Public beta.7
packaging and live frame-time validation remain pending.

## October 1: stowed gear fits the worn look, for beta.7

From a Nexus report (Sariel wearing Genessa: gun off her back, axe high).
Implemented and built, awaiting live validation: stowed weapons, sidearm and
seal take the look's shell socket adjustments instead of the worn shell's.
MISC "Sidearm position" is renamed **Gear position** ("Auto (fit the look)" /
"Default (game)"), because it has covered all stowed gear for a while and now
does more. Packages may name a `fit_shell`; otherwise their first listed shell
is used. Held (in-hand) weapons are not moved; their cause is still open.
Gragu's helmet is now a MISC shell item (own row, and follows Accessories &
Shell Tools); it rides the Head bone, which the socket rule skipped. The game
leaks one helmet per Player Menu build (ownerless, on the preview stage); CSS
now removes those, see [MISC shell items](misc-shell-items.md#gragus-helmet-beta7).
Details, data and test plan: [socket fit](development/socket-fit.md).

## October 1: preview lighting for beta.7

The user authorizes necessary CSS improvements discovered during Unholy Genessa
work, with clear scope and separate commits, for a later **v1.0.0-beta.7** release
alongside upcoming CSS features. Do not publish or bump the release prematurely.

Implemented, built and awaiting live validation: enable translucent lighting on
CSS's character-preview lights while CSS owns the preview, and restore every
previous flag on close, preview replacement and teardown. Four native preview
lights currently report `bAffectTranslucentLighting=false`. A reversible in-game
comparison illuminated the otherwise dark pale bodice, veil and upper skirt.
All four flags were restored and independently read back. Wings and world
lighting still require separate work; this does not solve garment deformation.

Evidence and screenshots: Unholy Genessa's `work/light1` and
`authoring/live-trial-notes.md` under
`CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P`.
CSSX provided development diagnostics only and is not a dependency for the fix
or the character mod. Preserve concurrent UI/performance changes when implementing.

Implementation records the original flag on each live preview light using weak
object handles. Entry enables the four lights, and close, native-tab exit,
preview replacement and teardown restore their captured values. No per-frame
light setter or world-light modification is added. Unsupported components are
skipped. Failed restorations retain valid handles for a later retry.

Validation: the Windows C++23 `css_core` Release build passes. Existing portable
`css_inventory_light`, `css_inventory_motion` and `css_inventory_backdrop` tests
pass (3/3). These test the existing orbit/camera math, not the new reflected
light lifecycle. The build retains an unrelated unused-variable warning in
`extension_data.cpp`.

Before beta.7 release, install with the next coordinated game restart and verify
entry/exit, original true and false flags, native-tab changes, preview replacement,
world teardown, orbit/reset controls and translucent-material readability. Measure
preview performance; enabling translucent light contribution can add GPU work.
The installed DLL and release version remain unchanged until that validation.

## Earlier work

2026-09-27 final scope correction: deliver **Black Pearl + Prototype Planet Diving Suit + Skin Suit + Vacation Bikini + Casual Knitwear + Midsummer Alice + War Aegis**. User confirmed Prototype, not 6th. Fit Prototype first, then follow this list. Holiday and unlisted outfits are out of this delivery. Retain Original plus five garment palettes, fitting, customization and physics requirements. Prototype tail must have physics and independent persisted Show/Hide control. Current bind-pose candidate and remaining defects are in `development/eve-outfit-goal.md`.

2026-09-27 latest user priority: **skip Holiday Reveler** because its investigation is consuming too much time and tokens. Work on **Planet Diving 6th, then Skin Suit, then the remaining outfits**. Holiday experiments are parked, not accepted or awaiting more automatic tests. Keep the complete fitting, physics and customization requirements for the active outfits.

2026-09-26 Eve hem follow-up: unconstrained persistent contact projection fails the full sprint sequence with large correction jumps and fabric stretching. Saved rejection evidence in `work/eve26/hemseq2.json`. Next solver experiment must enforce fabric shape and temporal continuity together with contact, not raise the projection cap. No game/release changes.

2026-09-26 Eve fitting: broader sprint checks find rear-hem/thigh penetration despite exact baseline-follow preservation. Static 22-carrier contact trial is diagnostic only. Residual upper-hip penetration is swinging-left-forearm contact, not a reason to enlarge the hip or edit body proportions. Continue lower-hem control and separate arm/sleeve contact diagnosis. See `development/eve-outfit-goal.md`; no game or release changes.

**Latest September 26 instruction: fitting first.** Focus on garment fitting to Black Pearl's standard. Park palette implementation. Start with Holiday Reveler, whose latest saved blend has missing vertex weights, then validate fitting, morphs and motion before extending to other outfits. Preserve Gemini's improved custom skeleton, including its game sockets. See the [updated execution order](development/eve-outfit-goal.md#next-implementation-steps).

**2026-09-26: Eve outfit work resumed.** Current priority is the [Eve outfit goal](development/eve-outfit-goal.md), with [evidence index](development/eve-outfit-index.md). User requires Original plus at least five clothing palettes per outfit and complete applicable customization, fitting and physics. Installed metadata confirms all ten added outfits currently have hair-only palettes because the generator removes garment controls and redirects suit colors to hair. Repair actual garment bindings before counting palettes as delivered. Claude Code owns concurrent CSS UI/UX and performance work. Older pause/alpha entries below are historical; the camera fix was subsequently accepted, and current manifests include Eve I2 idle.

## Earlier checkpoints

**Resumed by user on 2026-09-22 for an urgent alpha camera fix.** At the start of the parry/riposte animation, the camera sits too far behind and misses the character. Reproduce, fix, rebuild/install/test, then replace the affected alpha release ZIPs as explicitly requested. Check related camera-driven animations. Keep idle development parked and preserve the separate CSSX agent's work. The [prior pause handoff](development/resume-after-quota.md) remains the checkpoint for unfinished idle work.
2026-09-22: [Animation-camera repair](development/animation-camera-repair.md) is installed for user review. Only camera_pivot/camera_target translation modes change; release replacement awaits riposte/trap gameplay verification. Broader work remains paused.


2026-09-21: [I2 gentle Eve idle](development/eve-animation-sources.md#i2-gentler-idle-gesture) changes only three head/neck rotation tracks, preserving I1's stance and hands. Saved/raw checks, two-loop compressed playback and fitted-mesh replay pass. Reviewed frames and measured ponytail inclination show a smaller sweep; the user has a side-by-side style comparison pending. No cooking or game deployment. Next complete custom sequence playback and weapon visibility ownership, accounting for the game graph's downstream hand correction and CSS's active-montage recovery guard. Preserve accepted movement and physics. Full v1.0.0 remains active.

2026-09-21: [I1 Eve idle playback and review](development/eve-animation-sources.md#i1-saved-playback-and-visual-review) now passes fresh saved-track checks, two timed loops at center/corner blend inputs and fitted-mesh replay. A 14.13-second preview and front stills show the narrower stance, but the head gesture carries the ponytail too far back. Direct upstream rendering confirms the sweep exists before secondary motion; hair local rotations remain at bind. Refine this idle candidate without changing accepted global physics, then implement sequence playback and reversible weapon hiding. The renderer now supports a correctly labeled upstream comparison. The installed alpha and accepted movement remain unchanged. Full v1.0.0 remains active.

2026-09-21: User requested a separate-agent handoff for standalone CSSX v1.0.0 and Cheat Menu v1.0.0. [Execution prompt](cssx/standalone-v1-agent-prompt.md) assigns the new project to extensions/core, preserves the pinned alpha UE4SS build and unchanged CSS compatibility, and requires foundational FPS attribution, UI redesign and measured release checks. This is delegated future CSSX work, not authorization for this CSS/Eve agent to re-enable legacy CSSX. Eve idle creation completed offline before this interruption; readback/integration remain pending.

2026-09-21: CSS v1.0.0-alpha.1 and the separate Eve Black Pearl sample are published as a [GitHub prerelease](https://github.com/ngodn/CustomShellSystem/releases/tag/v1.0.0-alpha.1). Exact production boot/menu/settings/travel/destination checks pass. All six remote assets match local digests; both downloaded ZIPs match byte-for-byte. Stable latest remains v0.3.2. Alpha delivery milestone is complete; full v1.0.0 remains active, including custom Eve idle/weapon restoration, beacon animations, wider gameplay coverage and the modder kit.

2026-09-21: Exact alpha production archive installed and checked after normal restart. Loader/core PID and release hashes match; saved Eve appearance and animation choices survive unchanged. CSS menu is visually checked. Beacon travel returns to Eve with feminine idle; destination recording observes custom jog/sprint, idle and attacks without reported animation/maintenance errors. Finish public alpha delivery, then resume remaining v1.0.0 features. Follow up the Harbinger toggle help placement, which still sits inside the right panel rather than the shared footer in the release screenshot.

2026-09-21: User accepted the S1 movement correction after testing: "yeah its good now". Lock the corrected Walk/Jog/Sprint for alpha; do not ship M3 or reopen accepted body/rig/physics tuning. All three custom gaits and feminine idle were observed in the passive gameplay recording with no animation errors. Production runtime build passed with developer probes disabled. Package CSS v1.0.0-alpha.1 and the separate Eve Black Pearl sample, retaining unfinished idle/beacon/modder-kit work.

2026-09-21: User requires stable Eve Walk/Jog/Sprint in CSS v1.0.0-alpha.1. Do not ship the sideways-lean defect as a known issue. Alpha runtime and separate Eve technology-preview packages are the active delivery; movement stability is a hard release gate. Feminine idle grounded-check and status-footer fixes are installed; idle now engages live.

2026-09-21: User clarified the quota priority and authorized new playable Eve Walk/Jog/Sprint. [Movement delivery](development/eve-movement-delivery.md) is cooked, packaged and installed after a normal restart. Phase and cadence correction, 1,305 compressed component frames, payload-preserving package readback, native build and focused tests pass. New core responds. Live selector, gait/transition and visual review are in progress. The earlier pause question is superseded. Idle/beacon and the full CSS v1.0.0/modder-kit scope remain active.

2026-09-21: User prioritized delivery before the weekly quota runs out. [Playable checkpoint](development/delivery-2026-09-21.md) packages the exact tested choices1 core, matching loader and accepted Eve Black Pearl trio. ZIP integrity, complete byte readback and extracted outfit validation pass. A separate 7.4-second offline movement preview is available. No deployment or publication. Unfinished P1 phase-authoring code is committed at 2388bcc with build-only status explicitly recorded; no P1/M2 assets exist. Full CSS v1.0.0 and modder-kit scope remain active. A user preference question about pausing after delivery is pending.

2026-09-21: [Movement blends M1](development/eve-animation-sources.md#direction-and-speed-blend-candidates-m1) pass saved/readback checks, 264 interpolation queries, five negative fixtures and 1,305 engine-clock component frames. All 27 sampled fitted renders were reviewed. Rates remain provisional, left jog phase differs from forward, and upper-body/contact review remains open. No deployment. Next phase/cadence correction and weapon/gait-layer verification, then idle/beacon integration. Default is already committed as the visible animation label; legacy normal saves stay compatible.

2026-09-21: [D2 attachment propagation correction](development/eve-animation-sources.md#d2-attachment-propagation-correction) fixes the offline retarget export's missed appended branches. UE's contiguous-child traversal skipped CSS extensions. The shared skeleton, original proportions and accepted dynamics stay intact. All 23 candidates pass independent attachment/raw-track checks, fresh readback/repeat conversion and 1,643 compressed component frames; all 69 sampled fitted renders were reviewed. Formerly bad sprint roots now attach behind the head. Some dynamic overlaps remain, so whole-motion/contact acceptance is open. No deployment. Next build directional blends, validate phase/cadence and weapon layering, then moving-owner/live motion, custom idle and beacon integration.

2026-09-21: [Directional Eve batch D1](development/eve-animation-sources.md#directional-source-batch-d1) contains eight walk, eight run and three sprint clips from actual Stellar Blade lock-on references. All 19 pass fresh raw readback/repeat conversion and 1,075 compressed component frames with accepted dynamics unchanged. All 57 sampled fitted renders were reviewed. Raised combat arms and forward-hanging/overlapping hair in some run/sprint samples still need resolution; these candidates are not visually accepted for deployment. Production assets and the installed game are unchanged. Next inspect upstream head/hair transforms and initialization, then blend phase/cadence, weapon layering, idle/beacon integration and live acceptance. The UI label is already Default in source; legacy normal saves remain compatible.

2026-09-21: [Animation playback candidate](development/animation-playback.md) connects selected custom Walk/Jog/Sprint paths to the player BlendSpace override, validates skeleton/sample/axis structure and restores the original pointer/flag pair. Built-in speed scaling is player/thread-scoped; obsolete shared run-asset mutations are removed. Current player-linked gait flags replace global ability lookup. Portable ownership/gait checks and Windows compilation pass. No deployment. Live flag freshness under override, update latency, complete directional assets, custom idle/weapon restoration, beacon events and visual gameplay checks remain required.

2026-09-21: Expanded LOCOMOTION source now has five shared scrollable selectors and a validated per-outfit/variant save command. Idle and Walk independently control the built-in feminine override; Default releases it without changing other variants. Missing option IDs remain visible and saved, while navigation skips them. Custom playback, runtime ownership/weapon restoration, cooked assets and live UI review remain pending. Keep this candidate undeployed until playback is connected. See [animation controls](development/animation-options.md#locomotion-selectors-and-commands).

2026-09-21: The Default label is confirmed throughout the legacy Walk UI; remaining Normal properties belong to Unreal button styling. Animation playback investigation found that dynamic idle montages can stop same-group game montages. A private constant-idle BlendSpace helper builds and now has compressed component comparisons; strict secondary-motion equivalence is not yet accepted. Stellar Blade locomotion BlendSpaces establish source gait speeds and separate directional clips. No gameplay or installed asset change. Continue runtime ownership, directional authoring, animation UI and beacon support. See [playback evidence](development/eve-animation-sources.md#locomotion-blend-sources-and-idle-playback-investigation).

2026-09-21: [Native animation options](development/animation-options.md) now parse outfit/variant Idle, Walk, Jog, Sprint and Beacon definitions, exact weapon-tag idle overrides, unarmed idle intent and paired beacon clips. State and profile save/load preserve per-outfit/variant choices, including missing-mod IDs. Invalid definitions fail rather than silently applying a different option. The visible legacy Walk choice is renamed Default while saved normal values remain compatible, as requested. Runtime playback, expanded LOCOMOTION UI, weapon restoration, cooked assets and live review are still pending. No game deployment.

2026-09-21: Eve jog/sprint exposed a 90-degree source-facing mismatch hidden by the upright walk/idle. The corrected root basis, applied to reference and animated keys, now has fresh four-clip readback and compressed component playback through the real hair/body/hand rigs. Rejected anim2/anim3 candidates are superseded by anim4 V2. Production mesh, Skeleton, AnimBP and original proportions remain unchanged. The fitted preview now uses correct covered-foot sections, heeled stocking feet, lining and active hand morphs. No game deployment; cooked/live motion, contacts, transitions and CSS animation integration remain open. See [animation evidence](development/eve-animation-sources.md#facing-correction-and-component-playback).

2026-09-21: User supplied the original author GIF/PDF as pose references. The GIF demonstrates body physics; the PDF documents corrective shapes, tweak bones and a separate pose library. The local blend contains WalkInPlace and EveWalkTweaks actions, but their playback is unverified and the separate pose library is absent. Compare these after the active coordinate correction; preserve source permission/provenance notes for release. See [author references](development/eve-animation-sources.md#original-authors-pose-and-deformation-references).

2026-09-21: [First Eve walk/idle retarget candidates](development/eve-animation-sources.md#first-fitted-mesh-candidates) pass headless UE conversion, full-frame saved/repeated raw-pose readback and fitted-mesh replay. Source timing and protected mesh/Skeleton files are preserved. Reviewed solid-material stills and short motion artifacts are in `work/anim2/`; covered-foot section selection, lining and secondary motion were absent from this preview, so visual/game acceptance remains open. The headless Slate crash and sampled-key cache guard are documented to avoid repeating them. Next improve preview fidelity, check hands/heels, extend jog/sprint, then compressed/cooked validation and animation metadata/runtime/UI integration. Preserve the user's feminine, sensual Stellar Blade direction for Eve. No new game deployment.

2026-09-21: [Eve animation sources](development/eve-animation-sources.md) now include eight freshly decoded source clips, validated original key times/hierarchy, a reviewed source-joint comparison, and an unchanged H2 ACL export regression. The fast-travel montage's reverse playback and exact PreActivate/FadeToBlack/Activate events are identified from cooked assets. No new animation or runtime is installed. Next retarget onto the accepted rig and evaluate skinned motion, then finish modder animation metadata, saved/UI choices, runtime cancellation and weapon restoration, and beacon integration.

2026-09-21: Eve animation direction clarified: walk, jog and sprint should retain the feminine, sensual quality of her Stellar Blade movement, using her actual clips as candidates. Author a graceful, subtly sensual beacon kneel and matching rise, with soft torso movement and a controlled hip settle. This is Eve's optional animation set, not a global replacement for every CSS outfit. Preserve fast-travel events, cancellation and gameplay timing. Continue the active animation milestone; accepted rig, proportions, motion tuning and ground offset remain locked.

2026-09-21: [Shared choice lists and contextual helpers](development/inventory-choice-lists.md) are installed and verified. Shells and outfit variants share fixed-height scrollable rows with an equipped marker and selection reveal. Main-page editing hints share a stable right footer. Mouse/keyboard scrolling, unchanged preview view, restored state and actual menu re-entry pass; Steam screenshots were reviewed. The close-driver mouse-release race is documented and corrected to keyboard Back. Next continue expanded locomotion and release acceptance; investigate safe core DLL reload before relying on it for routine updates.

2026-09-21: User clarified the UI rule applies to context-sensitive helper placement throughout CSS: browsing and Back at left, preview controls at center, current-option helpers at right. Keep stable footer alignment while labels and controller/keyboard icons follow the context. Applied immediately to the shared selector revision.

2026-09-21: [Official-shell appearances](development/original-shell-appearances.md) now pass all ten live menu bindings, independent cooked-reference comparison, world/preview mesh equality, gameplay identity and animation-class preservation, Steam screenshot review and Eve state restoration. The public Eve name is installed and verified. Next implement the requested shared scrollable selector and verify menu re-entry. CSSX remains disabled; full Next-Gen release scope stays open.

2026-09-21: User requested a reusable scrollable choice list for the official-shell selector and other CSS options. Keep readable row sizes, show the equipped choice, preserve scroll position and reveal controller-selected options. Finish the current ten-shell appearance check, then implement this UI correction.


2026-09-21: The user reviewed the installed colors4 assets and colors2 core and
answered “Looks better” for skin tone, chest boundaries and the groin patch.
Keep this repair as the visually accepted color baseline. Live Ivory and black
texel checks pass, including the formerly rejected sparse UV material, and
both trials restored saved settings. Commit: `f949a57`. Do not reopen accepted
rig, body proportions, hair or ground tuning. Continue the public Eve package
rename, then the remaining release features and checks. This acceptance does
not prove every swatch combination or make CSS v1.0.0 release-ready.
Evidence: [authored colors](development/authored-color-swatches.md).

2026-09-21: [Public Eve package rename](development/eve-package-identity.md)
prepared and verified under `work/eve1`. Trio and ZIP use
`CSS_EveStellarBlade_eins0fx_P`, SHELL name `Eve (Stellar Blade)`, variant
`Black Pearl`. Preserve stable save IDs and all accepted cooked assets. Native
package checks and ZIP readback pass. Install with `--replace` during the next
normal deployment and verify live labels/state; the running game retains the
accepted pre-rename package until then. Version remains `1.0.0-candidate`.

2026-09-21: [Eve colors installed](development/authored-color-swatches.md#installed-color-checkpoint) in `css_core-colors1.dll` plus authoring `work/colors3/trio`, using a normal restart and verified backups/state preservation. Fresh world mesh/-99 cm checks pass. Steam still and game-window clip show the five-palette list with Jade selected; live material reads prove torso/genital slots retain original textures while the six tested outfit slots use color targets. Full palette, 12-chip, Default/cycling and region-boundary review remains. The user confirms they are testing colors; observe without changing their controls. Preserve their latest selection rather than restoring the older pre-restart look over it.

2026-09-21: [Authored swatches and Eve palettes](development/authored-color-swatches.md) prepared offline. CSS supplies ordered swatch UI, reset behavior, data validation and saved-state handling; mod authors supply model-specific masks, material bindings and palette content. Eve candidate `work/colors3` has five outfit-only palettes and ten color controls with 12 choices each. New region masks preserve original coverage; native package/cache checks pass. Live installation, appearance and menu checks remain. Preserve accepted V44 rig, hair 200/24/0, -3 cm ground offset and disabled CSSX.

2026-09-21: Queued v1.0.0 locomotion expansion after current color work. Outfit mods can provide Idle, Walk, Jog, Sprint and Beacon Teleport clips. Present a single Idle Animation control; resolve weapon-specific idle clips internally, or use one unarmed idle with weapons hidden only during that idle. Restore weapon visibility before combat and on cancellation, outfit removal or state transition. Beacon Teleport must cover standing-to-kneeling before travel and kneeling-to-standing after arrival while preserving gameplay notifies, travel timing, root motion and interruption behavior. For Eve, inspect the user's installed Stellar Blade assets for suitable walk/jog/sprint and expressive idle references, adapt compatible clips, and author/adapt the teleport pair. Do not assume matching skeletons, copy gameplay events blindly or enable an unverified clip live. This is additional requested release scope, not a switch away from colors or permission to reopen accepted hand/ground tuning.

2026-09-21: [Preview-light tab exit/reentry and menu recreation](development/preview-light-controls.md#tab-exit-and-menu-recreation) now pass. The old ownerless component must not be mistaken for the newly created light; both rejected probe assumptions are documented. The new preview starts with exactly the original light transform/intensity, and the reopened Steam still is reviewed. Remapping, focus and broader release acceptance remain open.

2026-09-21: Ground contact is accepted at -3 cm and installed in `css_core-ground1.dll` with a metadata-only Black Pearl package. [Evidence](development/footwear-grounding-followup.md#accepted-3-cm-correction): user-approved world-motion trial, exact cooked-container preservation, passing host/development/shipping builds and native -99 → -96 → -99 cm disable/enable read-back with the entire saved look preserved. Stop tuning the amount and move to remaining fixes. The full release goal remains active.

2026-09-21: Ground-contact acceptance clarified: feet should look planted. A slight visual overlap with the floor is acceptable, but visibly buried shoes are not. Do not prolong tuning to force mathematically exact zero clearance. Preserve proportions and the accepted rig; use measured clearance to choose a small reversible trial and judge it in world motion.

2026-09-21: Release naming corrected by the user: `CSS_EveStellarBlade_eins0fx_P`, with Black Pearl as the first variant displayed in SHELL and more variants later. Apply the new public name throughout the release outputs and metadata. The user clarified the order: finish ground contact and remaining fixes first, then complete the CSS v1.0.0 release checks and ship, then the modding guide in `CSS-Modding`. This does not cancel the open lifecycle/gameplay checks or authorize calling the current state release-ready without them.

2026-09-21: Queued appearance additions, without interrupting grounding: design five Black Pearl outfit palettes in addition to Original. Outfit palettes must exclude nipples, areolae, labia and genital areas. Offer independent color swatches for those regions and the other supported parts, including skin, with 12 choices per control including Default. Preserve original textures when Default is selected. Validate the actual material masks before assigning colors; an outfit tint must not recolor excluded anatomy.

2026-09-21: Queued CSS SHELL addition: place `Use Original Shell` immediately below `Original Appearance`. Selecting it exposes all official game shells in the right panel as visual-only choices over the currently selected gameplay shell. Do not switch gameplay identity, abilities or progression. Discover official shell assets/options from game data rather than inventing names. Existing `Original Appearance` continues to restore the actual gameplay shell's appearance. Verify switching, restore, saved-state behavior and combat compatibility before release.

2026-09-21: Y lighting and favorite sorting are installed in `css_core-ui1.dll` after a normal restart. The user confirms Y/Select/B work. [Lighting](development/preview-light-controls.md#y-shortcut-correction) passes controlled live orbit/camera/reset checks with reviewed Steam stills; a prior moving-input run is explicitly excluded. [Favorites](development/favorite-order.md#live-verification) passes actual row movement 3 → 6 → 3, retained target identity and complete saved-state restoration. Full lighting lifecycle, ground contact, broader combat and modular Next-Gen acceptance remain open.

2026-09-21: [Shell ordering](development/favorite-order.md) now uses equipped first, then favorites, then non-favorites, preserving catalog order within groups and selected outfit identity when rows move. Host tests and Windows development build pass. The Y lighting correction is committed as `dd9896f`; both UI changes await the next normal restart and live verification.

2026-09-21: [Short-path live trial](development/short-mesh-package.md#live-trial-checkpoint) is installed after backing up V44. World binding, accepted hair 200/24/0, hand configuration and limited collision geometry queries pass. A reviewed Steam rear still shows both restored heel supports; the reviewed world clip covers idle only. Walking/turning feedback, measured ground contact, preview lighting controls and broader gameplay acceptance remain open. Historical installation claims below describe earlier checkpoints. The later modder kit remains assigned to `CSS-Modding` and its own public GitHub repository.

2026-09-21: [Short mesh and complete package](development/short-mesh-package.md) pass offline verification. Five hair-weight import ties were detected and corrected to reproduce every existing V44 cooked weight assignment exactly. Geometry, three UV channels, 22 compressed morphs, 336 active rig frames and all 127 assets/dependencies pass. The catalog and material plan now use readable CSS paths. The prepared trio is authoring `work/short1/trio`; installation and live footwear/lighting/grounding checks remain. Original Blender/export and installed V44 stay intact.

2026-09-21: [Cooked short rig readback](development/short-rig-paths.md#cooked-rig-readback) passes. All six Skeleton/physics/animation/Control Rig documents match V44 after declared renaming; the seventh asset, the heel mesh, retains expected bindings, materials and morph references. Runtime CSS references resolve, and protected hashes remain intact. The local native allowlist accepts only the exact new shared B2 path and rejects preview-reference/lookalike rigs. Cooked mesh geometry, metadata/recipe integration, combined packaging and live checks remain; nothing is installed.

2026-09-21: [Short editor rig paths](development/short-rig-paths.md) pass fresh verification: all 44 copied dependencies resolve, 379 bones/nine virtual bones and bindings remain intact, 35 evaluated hand cases match exactly, and bound collision queries pass before/after physics recreation. All 268 protected asset hashes remain unchanged. The guarded seven-asset cook exits zero; independent cooked checks, runtime compatibility, metadata and packaging integration remain. V44 stays installed. The later public modder kit remains queued for `CSS-Modding`.

2026-09-21: [Short material and texture paths](development/short-material-paths.md) now pass: 30 cooked material headers relocate with exact unchanged/inverse round trips, 90 editor texture duplicates cook through the path guards, and every texture export/bulk stream matches V44 bytes. A fresh independent decoder confirms all 30 complete material documents with relocated references. The initial unresolved-texture readback is retained as a negative result. Next migrate editor material bindings and the heel mesh's skeleton/physics/animation/rig closure, then package and resume live footwear checks. V44 and the full Next-Gen objective remain intact.

2026-09-21: **Queued after stabilizing the Black Pearl authoring pipeline: public modder kit, guide and editor tooling.** User requests a proper standard for other modders, reusable base files, Blender and Unreal workflow plugins, and a recorded tutorial from authoring through `.pak/.ucas/.utoc`. Use `/home/eins0fx/development/mods/msII/CSS-Modding`, with a separate Git repository and public GitHub remote on the user's account. See [modder kit scope](development/modder-kit.md). This is a later milestone, not a priority switch away from the active path/dependency migration. Prove the manual workflow and validation gates first, then automate the same process in the plugins.

2026-09-21: **Before the next footwear cook, migrate candidate assets to short `/Game/CSS/` names.** [Path policy and evidence](development/asset-path-policy.md): rebuilt cooker and packager reject old namespaces/hash folders before output creation, including CSS dependencies and full Windows destination checks. The old plan has 129 packages, 125 violating the new rules. Equal-length material hash relocation is retired; safe serialized reference rebuilding and isolated dependency relocation remain pending. Use short deterministic work folders. V44 stays locked and installed, and the full Next-Gen goal remains active.

2026-09-21: [Isolated heel supports](development/footwear-grounding-followup.md#isolated-support-candidate) now have reviewed front/rear/side renders, exact preservation of all original mesh data, 168 added vertices in the existing footwear section, and 64 measured-pose/body-tone collar checks. Import, saved binding and fresh editor reload pass with accepted rig/material/physics assets unchanged. Actual idle scene traces measure 3.2–3.6 cm shoe clearance, distinct from the absent heel geometry and older 12 cm defect. User shape review is pending. Next cook/read back the candidate, check moving/barefoot grounding and live footwear visibility/motion. V44 remains installed and locked; full modular Next-Gen scope stays active.

2026-09-21: [Footwear follow-up](development/footwear-grounding-followup.md) is now active after locking V44. Fresh retained v202 ancestor comparison confirms a lower-footwear geometry/weight correction, with unchanged bones/topology/UVs, rather than a whole-character offset. The reviewed source shoe already lacks the rear stiletto support. Next measure V44 sole contact and prepare isolated footwear work; preserve accepted hands, proportions and dynamics.

2026-09-21: **V44 is locked as the user-accepted baseline.** User: "ok it's better now, we lock this, document this, and we move on". [Acceptance and pinned hashes](development/v44-live-trial.md#accepted-baseline-and-verified-world-entry) supersede the earlier waiting-for-pawn entries. Actual world bindings and geometry queries pass; the 30-second game-window recording is retained and sampled visually. Close active hand tuning, preserve original proportions and accepted body/hair settings, and move to grounding and missing heel supports using the v202 reference. Preview lighting is compiled locally, not installed. Full weapon/combat/lifecycle/performance and modular Next-Gen release checks remain open.

2026-09-21: [Preview lighting UI](development/preview-light-controls.md#menu-controls) is implemented locally. It reuses mapped Inspect (default Select/View and I) only with native character listeners inactive, excludes conflicting menu keys, and provides clickable controls. Returning to view keeps the edited light; reset is mode-specific and view reset preserves lighting. Development/shipping builds pass. Not installed or visually/hardware/lifecycle accepted. V44 fresh binding-check-3 still has no pawn; full gameplay and modular Next-Gen scope remain active.

2026-09-21: [Preview light backend](development/preview-light-controls.md) is implemented while V44 awaits world entry. Native orbit preserves authored distance/aim, locks view input and restores the original preview light on cleanup. Host motion/light tests and development/shipping Windows builds pass. No deployment or live-light acceptance. Select/View already maps to native Inspect, so routing, keyboard conflict checks, user toggle/reset and visual/lifecycle verification remain required. V44 gameplay checks and the full modular Next-Gen goal remain active.

2026-09-21: [V44 live binding check](development/v44-live-trial.md#next-live-checks) now distinguishes absent character, rejected binding and scoped success without changing settings. A fresh acknowledged probe still has no controller/pawn; the recorded V43 negative control is rejected before function invocation. Optional geometry queries are prepared but unexecuted and cannot prove damage/parry. V44 remains the installed trial, V43 the rollback. Await world entry for actual bindings, visuals and gameplay; full modular Next-Gen scope remains active.

2026-09-21: [V44 trial is installed](development/v44-live-trial.md) after an authorized normal restart. The final combined package independently decodes to the five verified assets. The native allowlist now accepts the exact audited B2 reference path; its formerly failing regression and Windows build pass. Fresh process/loader evidence confirms the new core, with CSSX disabled and saved CSS state unchanged by installation. The first Steam still is black before character load; no gameplay verdict yet. User was asked to Continue with Martyr’s Blade. Next verify live binding, hand/weapon motion, hit/parry, lifecycle and performance. V43 and the old core are backed up; the full modular Next-Gen goal remains active.

2026-09-21: [Bound and cooked V44 collision](development/body-collision-binding.md) now passes automatic body creation and physics recreation without overrides, repeated after fresh load. The five-package cook, Zen verification and independent decode pass; 22 bodies, 21 constraints, 23 shapes and 212 exclusion pairs survive. Geometry/skin/morph bytes and Skeleton/animation/rig data remain unchanged. The cooker now explicitly supports Physics Assets. Empty-bulk offset and enum-spelling verifier failures are documented. Next stage the combined candidate for actual game selection, damage/parry, weapon/sidearm, lifecycle and performance checks. V43 remains installed; proportions, accepted motion, disabled CSSX and the full modular Next-Gen goal are preserved.

2026-09-21: [Moving collision and hand coverage](development/body-collision-motion.md) now pass 31 actual-pose skin audits after extending only the two hand boxes. The saved V3 Physics Asset passes 1,688 ray and 608 intended-body distance observations, repeated after fresh load, plus 30 FPS kinematic/fall checks. Reviewed stills and diagnostic clips are retained. This fixes measured collision coverage, not weapon grip or live damage/parry. Next bind/cook the isolated V44 mesh and verify game behavior, recovery, full weapons and Next-Gen acceptance. V43 remains installed, CSSX disabled, accepted motion and proportions preserved.

2026-09-21: [Morph-aware collision fit](development/body-collision-reference.md#morph-coverage-and-orientation-refinement) replaces neutral-only coverage, which missed skin at 56 of 64 morph corners. The refined fixed shapes enclose all corner vertices, reduce excess limb/foot volume, and pass reviewed neutral/maximum views. Saved PA_B2BodyFit_V2 passes 1,592 real ray observations plus 512 intended-body distance observations, repeated after fresh load with exact saved properties. Next moving-pose/ragdoll, cooked mesh binding, live damage/parry and full Next-Gen acceptance. V43 stays installed; original proportions, accepted body/hair and disabled CSSX are preserved.

2026-09-21: [Fitted body collision and actual queries](development/body-collision-reference.md#fitted-geometry-and-actual-component-queries) now cover the neutral main skin plus modular feet with 22 human bodies. All six collider overlays were reviewed. The isolated saved Physics Asset passes 1,080 null/apply/remove/reapply ray checks, then the same checks and property preservation in a fresh process. Broad collider regions still need refinement, public-morph/moving-pose and ragdoll coverage, cooked binding and live damage/parry. V43 remains installed, CSSX disabled, accepted motion/proportions and the full modular Next-Gen goal unchanged.

2026-09-21: [Body collision reference](development/body-collision-reference.md) confirms live V43 has neither a Physics Asset nor an override. Original/MoreBeaute reference the same game asset; its 60 bodies include 38 cloth bodies, 25 explicitly simulated. An unfitted template retains 22 human bodies, 21 constraints and remapped collision-disable pairs. Next fit and visually inspect colliders, test actual queries, then verify game damage/parry. This is not yet a collision repair; V43 remains installed, CSSX disabled and full modular Next-Gen scope active.

2026-09-21: [Cooked V44B2 binding](development/b2-reference-binding.md#cooked-binding-checkpoint) passes exact geometry/skin/morph chunk preservation, corrected Skeleton references, nine virtual bones, all material paths and saved hand/hair defaults. Cook, pack, verify and decode exit zero. Numeric float32 checks account only for signed zero. The package still has no Physics Asset and is not installed; collision fitting, complete weapons/live/performance and full modular Next-Gen acceptance remain open.

2026-09-21: [V44B2 reference binding](development/b2-reference-binding.md) preserves CSS metadata and nine virtual bones in an isolated saved Skeleton, with accepted 200/24 and raw-hand defaults. The saved candidate passes 40 sampled animation/IK cases and 35 fresh component checks; both weapon views were reviewed. Old reference finger inputs differ by 34–38 degrees. V43 editor Physics Asset is null, confirming the unresolved collision gap; this revision still needs cooking, gameplay metadata, complete weapon/live/performance acceptance. V43 remains installed, CSSX disabled, proportions and full modular Next-Gen scope preserved.

2026-09-21: [Moving hand/body/hair integration](development/hand-postprocess-integration.md#moving-component-and-public-morph-isolation) passes 336 actual component samples with three overlays, six public morphs, hand toggles, physics reset and instance replacement. Public morph transfer is exact; private weight error is at most 8.05e-7. Both physics rigs remain active and hand changes preserve their outputs. The failed source-key comparison was upstream compression, now measured separately. Next finish production defaults/bindings and pose-domain selection, full weapon visuals, cooked/live performance and UI/gameplay acceptance. V43 remains installed; proportions, accepted dynamics, disabled CSSX and full modular scope remain preserved.

2026-09-21: [Hand post-process integration](development/hand-postprocess-integration.md) passes 42 actual component checks and a seven-frame fresh-import regression. The first test exposed missing morph-driver metadata: curves reached the graph output but not active mesh weights. The importer now stores mesh-local morph flags; all 22 survive cooking, with byte-identical decoded B2 mesh data. The appended hand stage preserves inherited body/hair controls and disjoint 7/29/19-bone outputs. Next finish moving-animation/public-morph checks, production bindings/defaults (accepted hair 200/24), weapon/gameplay and performance. V43 remains installed, CSSX disabled, original proportions and full modular Next-Gen scope preserved.

2026-09-21: [B2 cooked hand assets](development/b2-cooked-hand-assets.md) pass full import/fresh readback and independent cooking/decode of the mesh, isolated Skeleton and hand graph. All 379 bones, 22 morphs, 193,261 triangles and three UV channels pass; cooked VM retains 3,360 instructions. Quaternion rounding and unavailable Python getter failures are diagnosed and retained. Next verify cooked deformation and full hand/hair/body binding, weapon/gameplay, controls and performance. V43 remains installed; no shared Skeleton migration or game deployment occurred.

2026-09-21: [Combined native hand](development/combined-native-hand.md) now passes 759 sampled running-attack inputs and 759 measured-output skin checks. All four worst-step stills were reviewed. Original inputs are interpolated before calibration; added proximal step is 1.669191 degrees, a descriptive metric. Next finish B2 import/cook, full weapon/gameplay and modular physics/UI acceptance. V43 stays installed; proportions, accepted hair/body motion and disabled CSSX remain preserved.

2026-09-21: [Combined native hand](development/combined-native-hand.md) passes 914 original-input composition/preservation checks, 11 warm-instance lifecycle checks and 453 valid-pose skin checks using only native rotations/curves. Both isolated views were reviewed. All 454 reconstructed inputs match prior calibration evidence; all 16 B2 exported correctives match evaluated source geometry. The saved graph costs 0.954 ms median / 1.905 ms p95 in the editor, not live FPS or complete dynamics cost. Next verify native transitions, full B2 import/cook and actual weapon/gameplay plus modular physics/UI behavior. V43 remains installed; proportions, accepted dynamics and disabled CSSX remain preserved.

2026-09-21: [Native thumb-tip clearance](development/native-thumb-tip-clearance.md) passes 934 native cases and 464 actual-skin checks after measured native finger outputs, followed by offline web/correctives. Both isolated hand views were reviewed. The shared graph-code refactor preserves every recorded finger result across 934 cases exactly. Thumb editor execution is 0.363 ms median / 0.973 ms p95; complete graph and live cost remain unverified. Next combine calibration, articulation, finger/tip/web corrections and curves, test transitions/cost, complete B2 import/cook and all weapon/gameplay plus modular physics/UI acceptance. V43 stays installed; proportions, accepted dynamics and disabled CSSX remain preserved.

2026-09-21: [Native finger clearance](development/native-finger-clearance.md) now has a saved single-step candidate passing 934 native checks, 464 measured-output skin checks and 423 fresh offline transition samples. Both hand views of the largest numerical-difference case were reviewed. The original 12-step port is rejected for cost; do not revive it merely because its skin passed. Candidate editor execute is about 0.610 ms median / 1.200 ms p95, not live FPS or total dynamics cost. Next port thumb-tip clearance, combine the fresh-input graph, verify full B2 import/cook, weapons/sidearm, dynamics/UI lifecycle and visible gameplay. V43, original proportions, accepted physics and disabled CSSX remain preserved under the full Next-Gen goal.

2026-09-21: [Native articulation](development/native-hand-articulation.md) passes 899 execution checks and a 464-case skin replay using measured native outputs for 446 articulated inputs. Both views of the worst hierarchy-threshold case were reviewed. The verifier explicitly covers the engine skipping tiny transform writes; the strict computed-math tolerance remains unchanged. Next implement native directional clearance, combine the fresh-input chain, check transitions/cost, import/cook B2 and verify full weapons/gameplay. V43, accepted proportions/dynamics, disabled CSSX and the full modular Next-Gen goal remain preserved.

2026-09-21: [B2 clearance replay](development/b2-hand-clearance.md) passes all 464 actual-skin cases after restoring the missing articulation input stage. Full versus reduced radial support passes 47 end-to-end comparisons. The separate six-case V43 differential identifies finite-difference rounding; its strict optimized-equivalence gate remains red and is not a gameplay result. Native articulation is being built; native directional integration, fresh-chain transitions/cost, B2 import/cook and full live acceptance remain open. V43 and full Next-Gen scope remain preserved.

2026-09-21: [Native thumb-web stage](development/native-thumb-web.md) passes 1,143 execution cases, 569 actual-skin checks of measured native outputs, preservation checks and isolated paired editor timing. The V3 diagnostic graph uses the V2 web model. Next complete the V44B2 directional-model replay and native articulation/finger/tip stages, then combine the graph, measure total cost, import/cook B2 and verify full weapon/gameplay behavior. V43, accepted proportions/physics, disabled CSSX and the full Next-Gen goal remain preserved.

2026-09-21: [Thumb-web contact](development/thumb-web-contact.md) now passes 464 B2 saved-pose checks and 105 affected transition samples using the V2 offline guard. It changes 13 synthetic poses and preserves all captured poses, original controls and H2 samples. The prior subframe failure remains a regression case. Next fit/port the full finger/tip/web clearance sequence to the native graph, measure its cost, complete B2 import/cook and full weapon/live acceptance. Preserve V43, accepted proportions/physics, disabled CSSX and the full Next-Gen objective.

2026-09-21: [Native hand calibration and morph payload](development/native-hand-calibration.md) passes 91 native cases, including both input conventions, quaternion antipodes, singular bypass and lifecycle behavior. Measured H2 output passes independent skin checking. B2 now has a verified 22-morph interchange with unchanged base data and six body morphs. Next fit/port clearance constraints and resolve thumb/palm failures, then verify complete weapons/sidearm, B2 import/cook, combined dynamics and live behavior. V43 remains installed; the full Next-Gen objective remains active.

2026-09-21: [Captured hand transfer and native correctives](development/native-finger-correctives.md) passes 35 B2 skin replays with the old 40-pair thumb failure retained as a control. The 16 original corrective formulas reduce to local rotations and execute in a saved, disabled-by-default native rig across 53 cases without changing bones. Next integrate the actual pose-calibration and clearance stages, resolve remaining thumb/palm stress cases, then verify full weapons/sidearm, combined dynamics, cooked assets and live behavior. No game changes occurred; V43 and the full Next-Gen scope remain active.

2026-09-21: [Aligned finger calibration](development/aligned-hand-calibration.md) passes five H2 samples with exactly the four neutral seam pairs, versus 74–142 additional pairs before correction. Source-input rotation checks and original open/closed controls pass; fitted front/rear review shows improved fingers. Next extend this measured input convention to attacks/overlays and sidearm poses, then integrate and verify cooked/live behavior. Do not treat the open supporting hand as a reason to force a fist. V43 remains installed and the full Next-Gen scope remains active.

2026-09-21: [V44B/B2 rest alignment](development/arm-rest-alignment.md) preserves anatomical lengths and every stored morph, with improved arm/wrist contours in the matched H2 pose. Left fingers remain unresolved. B2 preserves five attachment locals and passes the ordinary canonical bind policy. The [renderer fit correction](development/fitted-pose-rendering.md) now matches the actual neutral V43 export exactly; use fitted geometry for further contact work. Next continue finger articulation/contact on the aligned candidate, expand the weapon/animation matrix, and complete full asset/cooked/live verification. No game changes or restart occurred; full Next-Gen scope and accepted physics remain active.

2026-09-21 arm-binding checkpoint: [V44A experiment](development/arm-twist-binding.md) is built and rejected after engine evaluation and front/rear skinning review. Separate source twist weights and fitted helpers alone produce curved forearms; fingers and main arm poses remain unchanged. Eighty bounded engine cases pass, with 40 paired non-helper checks and preserved geometry/morphs. Next test full arm/hand rest-pose alignment with anatomical lengths, proportions, morphs and outfits preserved. V43 remains installed; no game deployment or input occurred. The full Next-Gen objective remains active.

2026-09-21: the user reaffirmed the full Next-Gen scope, including modular physics/motion, reusable extensions, native runtime, UI and pipeline integration. The hand/arm repair is a current milestone, not a replacement objective. Preserve accepted body dynamics, hair 200/24, original proportions and disabled CSSX; its performance investigation and full lifecycle/performance acceptance remain open.

Latest result: [game-derived foundation comparison](development/game-foundation-comparison.md). The full diagnostic reference plus CSS modes exactly reproduces the compact candidate at the tested H2 poses; copying game modes stretches hands/wrists and is rejected. Older V42 hand binding does not repair the pose. An editor-only source-coverage crash is corrected and guarded; explicit-source controls reproduce earlier results exactly. Next isolate lost arm-twist influences and arm/hand reference alignment. The [user-supplied non-CSS guide](development/non-css-guide-findings.md) is reviewed; apply its pose/binding checks while preserving CSS's morphs, proportions and modular dynamics. No new game revision is deployed.

Latest checkpoint, 2026-09-21: [authored H2 hand graph](development/authored-hand-ik.md). Matching V43/Martyr's Blade/H2 capture is complete. The corrected evaluator preserves authored retarget data, animated virtual bones and both CopyBone anchors before left IK; 60 mechanical checks and six rejection checks pass. Candidate wrists, fingers and arm contours remain unacceptable. Earlier pending-capture entries below are historical. The user's game-based skeleton v2 suggestion is under [primary-source review](development/skeleton-foundation-research.md), with an isolated comparison next, not an accepted production migration. Preserve Eve proportions and V43; no new repair deployed.

User annotations on the offline H2 candidate, 2026-09-20: review both arm/elbow
contours, the strongly bent left wrist with fingers hanging around the handle,
and the right wrist/grip. These are required pose/deformation acceptance checks,
not an accepted candidate. Preserve original body proportions. Finish the active
same-weapon reference comparison, then address remaining deformation with these
front/rear views as evidence; a zero IK effector error is insufficient.

[Exact game-reference evaluation](development/game-reference-import.md#engine-candidate-checkpoint) now verifies an isolated engine candidate with 182 matching reference rotations, preserving all 379 authored bones and Eve's mesh. It reproduces the 9-11 cm remaining left-finger gap; contact is not fixed. The original H2 animation has eight animated virtual-bone tracks omitted by the old diagnostic sampler. Next include those tracks and the authored retarget base in the game hand-IK comparison. MoreBeaute remains selected; matching live V43 H2 capture is still requested. No new game fix is deployed.

Full-pose baseline now replayed and visually reviewed: [MoreBeaute reference](../../CSS-Mod-Authoring/docs/next-gen-morebeaute-reference.md#full-body-replay-checkpoint). The helper excludes disabled sections and Blender armature-display geometry, verifies mesh identity, and saves finger/weapon spacing. `full-pose-current-v1` was already MoreBeaute, not V43. Matching SeduXtress H2 capture is still requested; no game state or assets changed.

User-confirmed working reference: [MoreBeauteGenessa comparison](../../CSS-Mod-Authoring/docs/next-gen-morebeaute-reference.md). Its installed Original mesh shares the game Skeleton directly, and its 258 reference bones exactly match the canonical CSS prefix. Its post-process only adds nine secondary-motion springs; the package has no replacement attack/locomotion clips. Compare the complete same-weapon live graph/attachments against V43 next. Preserve Eve proportions; reference differences alone do not identify the defect.

Latest hand checkpoint: [live overlay and fingertip clearance](../../CSS-Mod-Authoring/docs/next-gen-hand-live-overlay.md). The game reduces the overlay during attacks. Thumb-tip correction clears all 35 captured poses offline with independent Blender/render checks; synthetic stress passes 405/411, leaving six thumb/palm or self-fold cases open. Next compare full heavy-weapon arm/body/attachment poses, including sidearm aim/fire. No correction deployed; preserve V43, body proportions, accepted dynamics and disabled CSSX.

User weapon review, 2026-09-20: Axe & Dagger and Axatana look good enough to tolerate their remaining hand/clipping defects. Other weapons, especially heavy weapons, also have wrong overall handling poses and substantial clipping. This extends the existing all-weapon validation requirement beyond fingers. Finish the active thumb check, then compare complete scythe arm/body/weapon poses first. Preserve the original body proportions and the tolerable weapon cases; do not declare heavy-weapon handling repaired from a hand-only gate. The user supplied two further screenshots: a wide weapon-ready stance with weapon overlap at the upper thighs, and a raised/open hand with the Fire (Sidearm) HUD prompt. Include ready/idle, sidearm aim/fire, arm alignment, weapon attachment and full-body clipping. A still image alone does not establish which attachment or animation stage is wrong.

Commit CSS changes per coherent context. See the [2026-09-20 checkpoint](development/checkpoint-2026-09-20.md) for committed groups and validation.

Current hand checkpoint: [investigation and rejected approaches](../../CSS-Mod-Authoring/docs/next-gen-left-hand-source-contact.md). V43 remains installed. Dense spread checks pass only 42/112 and reveal offset jumps on nearly static input, so the discrete search is rejected for runtime use. The sparse fine result remains 28/30. [Optional corrective export](../../CSS-Mod-Authoring/docs/next-gen-hand-corrective-export.md) now supplies the missing 16 private finger morphs; geometry, six original morphs and appearance controls match exactly. Its isolated cook now preserves all 22 morphs, base mesh data and original body morph records; it has no active driver and is not deployed. Next develop a continuous hand correction and integrate the verified shapes, then validate graph overlays, weapon contact, cost and gameplay. No game deployment or interruption occurred. Earlier checkpoints are historical.

Follow-up checkpoint: Higgsfield CLI authentication and account/model queries now work, with all eight skills installed; see `CSS-Mod-Authoring/docs/higgsfield-setup-and-3d-assessment.md`. No generation or MCP connection was submitted. CSS anatomical-frame candidate evaluation fails with 559 cross-region triangle pairs (`left-anatomical-authored-remap-audit-v1`, exit 2). Applying inverse-remapped source finger rotations to stock Genessa also fails (811 cross-region pairs, `left-source-stock-audit-v1`); its two-view render completed and the rear view was reviewed. Genessa has long nonhuman nail geometry, so that result does not establish how the actual DarkForm source mesh deforms or prove a live stock-game defect. Next compare the exact source pose on the animation's authored DarkForm mesh, or obtain a full live original-character pose, before further rotation edits. V43 remains unchanged.

Handoff checkpoint before user-requested Higgsfield setup (2026-09-20): a bounded live CSS post-process bypass leaves all 19 left-finger rotations unchanged, then restores the original flag and all 26 settings. The active source idle is `A_Shared_Idle_H`, with `SK_DarkForm` retarget source. Using the actual game Skeleton reference (1199 raw bones), Unreal's compatible-Skeleton rotation remapping reproduces 18 live finger rotations exactly; the remaining thumb difference is the known 33.8052-degree Genessa overlay. Evidence: `work/grip-grounding-v1/post-process-hand-isolation/`, `active-idle-exact-v1/`, and authoring `work/nextgen-audit/audit-engine-skeleton-remap.json`. This identifies a rotation stage, not a finished correction. Inverse-only and authored-frame candidates still fail skin-crossing checks (997 and 549 cross-region pairs). The anatomical-frame candidate is generated but not evaluated. V43 remains installed, with improved grip and broken left fingers. The canonical 258-bone prefix is a Genessa mesh subset, not the first 258 entries of the actual game Skeleton; preserve existing layout while correcting future compatibility claims. Resume this investigation after Higgsfield setup; no new game deployment occurred.

Current user verdict: V43 improves weapon grip; left fingers remain broken. Preserve the installed grip improvement and focus the next correction on left-hand deformation. Same-pose V43/stock close-ups and joint angles are saved. Eve locomotion and the newly inspected g-walk, p-walk and Auto-Rig Pro Tools archives are queued references. Damage/parry, grounding, heel supports, preview lighting and full acceptance remain open.

Latest hand evidence: original fitted geometry and left-finger pivots match V43 within 0.00004 cm. A repeatable evaluated-skin gate detects 812 cross-region crossings in the captured pose and zero in neutral. Source correctives, skinning changes, three rotation-transfer variants, and broad relaxation have not yielded a deployable correction. Same-weapon original-character comparison is requested; see next-gen-live-grip-grounding.md for evidence and rejected variants.

Latest live checkpoint: V43 is installed after the user-approved normal restart. The active mesh path, 388 evaluated bones, all nine virtual bones and hand IK targets pass runtime verification. Hair remains 200/24, CSSX is absent, and deployment preserved native core, UE4SS and saved settings. The 25-second game-window clip is reviewed at 3, 12 and 22 seconds; it shows distant dark scythe idle, insufficient hand detail for visual acceptance. User grip review and other weapon/animation coverage remain pending. Evidence: `CustomShellSystem/work/grip-grounding-v1/hand-v43-live/`.

Updated 2026-09-20.

Queued Eve locomotion prototype (user suggestion): inspect animation and pose assets from `/mnt/eins0fxE/SteamLibrary/steamapps/common/StellarBlade` for idle, jog, run and related transitions. Establish source skeleton, root motion, scale and retarget compatibility before an isolated prototype. Keep current grip acceptance against Mortal Shell default animations as the active task; do not substitute a new locomotion set as evidence that stock-animation grip is repaired. Preserve original body proportions and accepted dynamics.

Queued combat defect (user observation, no priority change): enemies detect and attack the character, but hits appear to pass through, damage seems absent and parry fails. Read-only observation found `bCanBeDamaged=true`, with both mesh and capsule reporting `QueryAndPhysics` and `Custom` profiles. The probe failed at the unavailable reflected `GetPhysicsAsset` function; this is an incomplete snapshot, not a diagnosis. V42 cooked mesh has a null PhysicsAsset, but runtime overrides and the game hit/parry path remain unverified. Preserve the observation and inspect these after the active grip work. Evidence: `work/grip-grounding-v1/combat-collision-observation.requests.jsonl`.

Current grip progress: the stock same-pose control supports promoting the existing isolated V43 hand binding. V43 is cooked with unchanged V42 materials and controls. Packaging source references were corrected to reuse the verified original material bytes; independent cooked readback and live review remain pending.

Completed compatibility repair: [nine game virtual bones](../../CSS-Mod-Authoring/docs/next-gen-game-virtual-bones.md) are restored and verified in the editor, cooked package and running game after the approved normal restart. All four hand IK targets resolve; the 379 authored bones, geometry, textures, animation exports and accepted hair remain unchanged. The user reports unchanged grip, confirmed by sampled in-world close-ups. Do not mark grip fixed. Next perform the same-pose stock-hand control using the newly extracted Genessa mesh in `work/grip-grounding-v1/stock-hand-reference`, then choose the next justified correction. V43 remains isolated. Full goal and broader defects remain active.

Priority steering: the user requested online research because repeated revisions have not delivered a verified fix and consume too many tokens. [Research findings](../../CSS-Mod-Authoring/docs/next-gen-hand-retarget-primary-sources.md) are saved. Pause further mesh/tooling iterations; first establish the active animation path and compare the affected pose against the stock mesh. Use existing captures and evaluator evidence, with one discriminating check before choosing the correction. Full goal and queued work remain active.

Active task: grip/left-finger, grounding and missing heel-support defects. [Preview coverage](../../CSS-Mod-Authoring/docs/next-gen-preview-layers.md) is installed and verified through zoom/pan, reset, tab and full-menu lifecycle with reviewed Steam stills. Hair 200/24 is user-accepted and survives restart. Preserve body dynamics/settings, original proportions, all 379 bones and disabled CSSX. The user may resume gameplay; no injected movement or wardrobe changes. Grip acceptance must span weapons and default animation postures. Lighting-control mode is queued later. Full goal remains active.

Hand diagnosis: [captured-pose evidence](../../CSS-Mod-Authoring/docs/next-gen-live-grip-grounding.md) reproduces the reported deformation. Hand reference axes differ substantially, and eight source palm groups had been collapsed into the hand bones. V43 now saves the combined correction in isolation and passes a bounded editor retarget evaluation. It is not cooked or deployed; penetration, other weapons and actual game graph behavior remain unverified. The 20-second clip covers scythe idle only.

Completed preview request: repaired the CSS character preview foreground and background layers at maximum zoom-out. The supplied Steam image shows sharp horizontal rectangular boundaries crossing the character and a backdrop that no longer fills the view. Inspect both layers' geometry, UV/material coverage and camera range; extend or replace them so the full zoom range has continuous coverage. The user suggested generating replacement artwork for both layers if needed. Preserve the current character scale/proportions and preview controls.

New user-reported appearance defects (2026-09-20): weapon grip is offset, left fingers curl/deform incorrectly, feet appear above the floor, and narrow rear stiletto heel supports are missing. User supplied three annotated in-world images. Inspect hand bind/pose/weights and sockets separately; compare grounding against `CSS-eins0fx-collections/seductressV2-eins0fx-CSS/v202` as requested; compare shoe geometry/material masks with original source. Do not infer causes from screenshots or change body proportions. User expanded grip acceptance to all available weapon types, postures and their default game animations. Validate a weapon/pose matrix, including idle, movement, attacks and transitions, rather than correcting a single socket for the pictured weapon. Queue these with the preview repair after the active hair revision.

Queued CSS lighting controls (user request 2026-09-20): inspect the fixed top-left-biased preview lighting and its game components. Add a lighting-control mode toggled with the controller Select/Back/View button and a verified nonconflicting keyboard key. While active, lock current camera view and route the existing stick/mouse interaction to moving the light source; toggle back to camera controls. Inspect existing mappings and decide which light(s) to move from actual scene behavior. Keep clear mode feedback and restore game-owned light state on exit. This is a later feature, not a priority switch.

Footwear diagnostic: reviewed side renders of the V40 authoring source (used by V42) and untouched `reference/body-type-variant-EVE/eve_beta10.blend` both show the same absent narrow rear heel supports. Evidence: `footwear-v42-source-review-side.png`, `footwear-original-review-side.png`, `original-shoe-objects.json`. The Black Pearl combined stockings/shoe source has 8190 vertices; the separate optional pumps have 31668. Do not re-enable those overlapping pumps as a heel fix. The missing-support appearance exists in the source; resolving the user's request may require authoring heel geometry, not reverting a cooking regression. No source asset changed yet.

Active contact work: [ellipsoid contact prototype](../../CSS-Mod-Authoring/docs/next-gen-body-ellipsoid-contacts.md). Isolated BodyAngularV9 passes all existing timing/plane checks plus 21 contact fixtures, independent weighted-skin travel and sampled diagnostic renders. Fitted neutral ellipsoids use 9.62% to 40.60% of the former capsule volumes. This constrains region-center spheres, not full skin. Twelve additional editor-fed morph/geometry-reset fixtures and corrected maximum-morph renders also pass their bounded checks. [Morph-aware centers and inertia](../../CSS-Mod-Authoring/docs/next-gen-body-morph-layout.md) now preserve neutral inputs exactly and pass 128 direct geometry cases, 21 graph fixtures, weighted-skin travel and sampled maximum-morph renders. [Independent V10 region controls and combined V2](../../CSS-Mod-Authoring/docs/next-gen-body-region-controls.md) now pass direct and actual-component checks, including complete hair-pose isolation and reviewed clips. Next add body-only reset, native capture/apply/restore, preview/profile/UI and shipped morph evaluation, then finish opposing-skin and cooked/live validation. V2 cost remains unmeasured. Original proportions, 379 bones and the V41 blend remain unchanged. Installed V40/V37 and CSSX-disabled state are preserved.

Latest solver checkpoint: [body center planes](../../CSS-Mod-Authoring/docs/next-gen-body-planes.md). V8 passes 37 timing/20 plane fixtures, independent skin-travel verification and 36 paired garment-clearance cases. The scale-dependent acceleration guard found in V7 is fixed with unchanged tolerances; the reviewed clip has exactly matching V8 poses. Original proportions, 379 bones and the V41 blend are preserved. V8 remains isolated; combined graph/cost still use V4. Continue fitted anatomical/full-skin contacts and independent native body controls, then combined/cooked/live validation. V40/V37 remain installed and CSSX disabled.

Latest live checkpoint: [hair controls and profiles](../../CSS-Mod-Authoring/docs/next-gen-live-hair-controls.md). Eleven actual player/preview input cases pass, including disabled four-channel profile restoration, reset and tab re-entry. All seven preview chain roots move during the enabled orbit and have zero measured local rotation span when disabled. Steam stills and both 12-second game-window clips are reviewed. Original settings are restored; CSSX remains absent and the 379 bones/body proportions are unchanged. Continue fitted body contacts and independent body controls, then combined/cooked/gameplay acceptance. Direct mouse/keyboard/controller input, travel/death, weapon contacts and performance remain open.

Latest live fix: [paused preview camera](../../CSS-Mod-Authoring/docs/next-gen-preview-camera.md). Core `css_core-preview-camera-cfa0dfec3dcf0f0f.dll` is installed after normal restart. Preview entry, pan, zoom, tab re-entry and restoration of both original paused-update flag values pass. Six comparisons have zero actual camera pose/FOV error; the world stays paused. Steam stills and a 12-second game-window orbit are reviewed. CSSX remains absent and V40/V37 assets, original proportions and 379 bones are unchanged. The camera defect below is resolved. Continue live hair-control/profile checks and the unfinished body contacts/native controls.

Latest live checkpoint: [Steam capture and game entry](../../CSS-Mod-Authoring/docs/next-gen-steam-live-validation.md). The development screenshot action now works before Inventory exists. Steam stills visually confirm the title menu, gameplay with V40 equipped, Inventory and the CSS page. Startup-video skip and Continue used verified normal UI input handlers without desktop focus changes. Core `css_core-steam-shot-7f16c0fb5dd71b2e.dll` is loaded; CSSX remains absent. New open defect: CSS reports its camera active while the effective view remains the player actor. Resolve this before preview appearance/motion acceptance. This supersedes the no-player statements in earlier checkpoints below; V41 and body V5 remain offline.

Latest solver update: [body travel bounds](../../CSS-Mod-Authoring/docs/next-gen-body-travel.md). V5 adds per-helper weighted skin displacement limits, passing the existing 37 timing fixtures, 14 travel controls and independent mesh evaluation across all 64 morph corners. Geometry, weights, 379 bones and original proportions remain unchanged. Combined AnimBP/cost evidence still uses V4. Next implement fitted anatomical/planar constraints and the per-region native/UI contract, then update combined/cooked/live validation. No new game deployment.

Latest measurement: [combined component cost](../../CSS-Mod-Authoring/docs/next-gen-dynamics-cost.md). Four balanced blocks give 1.438 ms median / 1.875 ms p95 for both rigs in the editor; body adds 0.058 to 0.114 ms. Raw samples, moving/settling subsets and disabled/upstream controls are preserved. No game FPS or CSSX conclusion follows from this. Continue fitted body contacts and the independent native body-control contract.

Active offline work: [combined body and hair post-process](../../CSS-Mod-Authoring/docs/next-gen-combined-dynamics.md). Actual two-rig component execution now passes 15 fixtures with independent controls, shared reset, body re-enable and instance recreation; its measured skinned clip is reviewed. V41 remains a separate garment-only candidate with original proportions preserved. Next complete fitted body contacts, per-region/native controls, broader angle/morph coverage, component cost, cooked and live acceptance. Current native `RigSettings` capture/restore is one four-channel hair group and must gain an independent body contract. V40 remains installed with the full V37 hair graph and currently omits body dynamics. CSSX remains disabled. The fresh player query still reports no playable character.

Latest asset update: [V40 foot UV repair](../../CSS-Mod-Authoring/docs/next-gen-stocking-uv.md) is cooked, independently validated and installed with the unchanged V37 hair rig/core. CSSX remains absent and settings are preserved. The new game process responds at the title menu. Original proportions, 379 bones, weights and morphs are unchanged; all cooked UVs match and all 90 textures remain pixel-identical to V37. Hidden-component tick/reset/disable/re-enable and independently confirmed cleanup pass; visible character/UI/FPS/lifecycle checks still await Continue. Older upper-foot mapping and toe-silhouette defects remain. The full Next-Gen goal is active.

Hair checkpoint: [full seven-chain hair](../../CSS-Mod-Authoring/docs/next-gen-controlrig-full-hair.md). V37/V10 and native rig controls are cooked and installed, with V40 now supplying the mesh. 467 direct checks, 216 AnimBP checks, exact cooked geometry and bounded in-game full-rig/hidden-actor execution pass. Visible hair/UI/FPS, native control/profile/preview recovery, travel/death, cap residual contact and broader motion coverage remain pending. The original proportions and 379-bone standard remain preserved; the full goal stays active.

1. **Active: CSS Next-Gen v1.0.0.** Continue the full asset/runtime/UI/distribution goal, preserving original body proportions and the append-only 379-bone skeleton. Installed V40 retains the verified full V37 hair rig and core. Body dynamics is the current offline task. Visible player/UI/profile/lifecycle/FPS checks, remaining garment/contact coverage and release packaging remain open. The checkpoints above and their linked evidence supersede earlier V30 and intermediate probe states.
2. **Parked: CSSX FPS and DLL reload follow-up.** Retain the implemented idle-work fix and evidence in [the FPS investigation](development/cssx-fps-investigation.md), and the supported reload behavior in [the reload guide](development/dll-reload.md). Do not continue this investigation or re-enable CSSX ahead of the original goal. Historical ABI-1 performance and the animation reload crash remain unresolved.

CSSX disabled for Next-Gen: normal QuitGame/start completed, both CSSX startup paths are absent, and the restarted process (PID 2701637 at verification) maps only CSS core `css_core-frame-demand-8183ded19bd89cdc.dll`, with no CSSX or Cheat Menu DLL. Both extension folders remain installed but inactive. CSS core selection and all saved CSS/cheat settings are unchanged. Backup/audit pointer: `work/cssx-disabled-for-nextgen-latest.json`; live module verification: `work/cssx-disabled-for-nextgen-loaded.json`.

Preserve the original untracked `catalog/dev-items.disabled`. Steam screenshots remain the still-capture preference; avoid changing desktop focus for inspection. All authoring, builds, scratch and backups stay inside the workspace.

Completed capture-tool limitation: the development-only Steam action returns before player/Inventory lookup. Windows build, loader-contract checks, acknowledged replacement, module mappings and actual inspected Steam images pass. Evidence: `work/steam-title-capture-v1/`. The subsequently observed preview-camera defect is resolved in `work/preview-camera-v1/revision2/`; see the latest checkpoint above.


### Eve Holiday cloth integration investigation (2026-09-26)

Eve authoring has a separate panel-cloth structural prototype, but its single-active-morph limit does not satisfy independent combined body sliders. Two private AnimDynamics skirt candidates were also rejected, most recently for 18.30 cm jog displacement and visually confirmed hip clipping. Next adapt the existing constrained ControlRig approach to cloth-specific anchors and collisions. F11 fitting is preserved. No CSS core edits, game changes or integration requests yet. Before runtime integration, coordinate component ownership, outfit switching, preview, morph/color controls and cleanup with the agent working on CSS UI/performance. Details: `docs/development/eve-outfit-goal.md`.

## Queued October 15: optional Genessa revival override

Investigate a temporary, explicitly confirmed Faithful override when Alteration
blocks native revival. Preserve the upgrade and progression; keep normal game
rules as the default and provide Confirm/Cancel. Verify autosave and form-state
persistence before claiming no save impact. Finish the active clone cleanup and
native-blocker message check first. Details and acceptance remain in the
[three-outfit goal](development/three-outfit-rig-physics-goal.md#deferred-follow-up-genessa-form-after-switching-save-games).

## Current priority, October 7: restore gameplay interruption

User confirmed Commander White eyes and Faithful/Stray custom doubles work.
User explicitly deferred the After Brute Battle crash investigation ("forget the crash").
Current blocker: new idle animations fail to yield to aiming, attacks, doubles,
and MISC Genessa Form; controls can remain broken after selecting Default Idle.
Inspect live input/ability state and repair cosmetic animation ownership without
resetting gameplay montages. Do not disable CSS or alter saves. Evidence:
work/cine-animation-port/gameplay-cancel-regression-1. Keep full existing animation
scope, but restore gameplay controls first. No claim of live fix before validation.

## Low priority, October 8: Trebuchaxe sidearm visibility

Queue the reported hovering axe with **Only when in use**, including the charged-shot pose and Tarred Fragment / Monarch's Vestige / Squall Stone loadout. Reproduction, cause and fix remain pending. Handle after the primary rig, facial and animation work. Full report and acceptance criteria: [three-outfit goal](development/three-outfit-rig-physics-goal.md#low-priority-follow-up-trebuchaxe-sidearm-visibility).

## Queued, October 8: Genessa rear-panel menu/world mismatch

The user supplied two new rear-view images: glowing fitted panels cover the
glutes in the player/CSS menu, while much of that coverage disappears in the
world. These are different poses, so the images alone do not establish clipping,
visibility state, material state or a CSS synchronization fault. Compare the
same variant, pose, body settings, part visibility and simulation state first.
Determine whether the repair belongs to CSS or the outfit, then fix that layer
and verify menu/world transitions, body sliders and secondary motion. Preserve
the accepted design, glow and sheer controls. This reinforces the existing
Genessa fitted-ornament requirement in the
[three-outfit goal](development/three-outfit-rig-physics-goal.md#phase-3-collision-and-secondary-motion).
The user requested this when convenient, not as an interruption of the current
shared-pack integration and validation.

## October 15 scope correction: Zhirelle deferred to CSS v1.0.1

The user explicitly requested skipping Zhirelle for now and deferring her to
CSS v1.0.1. Stop current Zhirelle trials and retain sources and findings.
Merrick and native shell shades remain in the current v1.0.0 release scope.
This supersedes the earlier requirement to finish both NPC targets now.
See the [active goal](development/three-outfit-rig-physics-goal.md) and
[NPC findings](research/2026-10-15-npc-appearances-and-shades.md).

## October 15: complete bugs and straightforward boss adaptations before release

Finish the active goal and bug fixes before release preparation. Include bosses
that can use a straightforward adaptation like Merrick; inspect and validate
each before exposing it. Defer bosses requiring substantial repairs like
Zhirelle to CSS v1.0.1, recording the reason individually. This expands the
previous NPC scope. No public release while current-scope work is unfinished.

## October 19: UUEPBS compatibility and body motion feel

Nexus feedback (questman): transitions much smoother; asks for UUEPBS compatibility, finds chest parameters limited, and says the new physics feels stiff even after tuning. Findings and plan: `docs/research/2026-10-19-uuepbs-compatibility-and-motion-feel.md`.

- Bone naming: SKEL_Base chest helpers `brust001/002` never bind UUEPBS's Breasts slider (its patterns are breast/boob/bust...). Ship the `BodyGroups` snippet for `MortalShell2.json` in the CSS and package READMEs; propose `brust` upstream to XTGMods.
- Morph depth: each eins0fx body carries six morphs; expand with a chest shaping set and more regions per package (Astra lanes, brief `aaa-prompts/ASTRA_UUEPBS_BODY_SLIDERS_BRIEF.md`), decide which join the measured solver corners.
- Motion feel: the Eve 1.5.0 angle/travel limits are rig defaults the sliders cannot exceed; add a live `travel` channel to `angular_body` controls, soften the clamp, retune the four templates from measured traces, recook rigs; CSS 1.0.2 (branch `feature/uuepbs-body-sliders`).
- Live UUEPBS test beside CSS on all four outfits once the user approves installing it; then a doc section "Using UUEPBS with CSS outfits".
- 19 Oct 19:24: Eve lane sent on step 6 (travel rows, templates Firm 4/8 to Earthquake 20/18, SoftZone 0.5, motion_amount max 5, solver_axis on Implants and Cleavage, V11 rig and patched AnimBP, 8-axis geometry, 24 morphs, cook, package, verify, no install). Run `CSS_SeduXtress_eins0fx/work/agents/astra-20261019-192424`, report expected at `work/uuepbs/REPORT-step6.md`. Genessa, then Commander White, recook after it (one editor at a time).
- 19 Oct 20:33: Eve step 6 done (trio 1.5.1-bodyfeel.1, 24 morphs, V11, 8 axes, verified, not installed). Schema facts: no `travel.step` (core steps 0.5 degrees), `solver_axis` is an index 0..7, built-in presets carry travel/SoftZone, package templates do not. Eve repackaging with thigh default 4; Genessa launched on step 6 at 20:36 with these corrections. Shared Assets 1.0.2 metadata: `tools/authoring/shared-assets/append_revision_pins.py` appends each lane's revision pins (trial on Eve: 10 rows, 82 KiB manifest); final run after Eve's repackage.
- 19 Oct 21:35: installed with the user's go (game closed): core css_core-1.0.2-rc1.dll (selector backup `core.json.pre-1.0.2-rc1`), Eve 1.5.1-bodyfeel.1 (1.5.0 trio backed up under `work/body-feel-install-20261019/backup/`), Shared Assets 1.0.2 pak with Eve pins (1.0.1 pak backed up there too), UUEPBS 2.7.1 with the seeded profile. Record: `work/body-feel-install-20261019/install.json`. Live feel verdict and UUEPBS check: the user's.
- 19 Oct 21:11: Genessa step 6 done (0.3.1-bodyfeel.1, verified, staged, not installed). Shared Assets build with Eve plus Genessa pins: `work/shared-assets-1.0.2/eve-genessa/trio`. Commander White launched on step 6 at 21:39.
- Follow-up (core side): the Shared Assets cloth DeformerGraph reapplies only the five original morph deltas (`CSS_Morph_PBMGlutesSize/PBMHipSize/PBMThighsTone/PBMWaistWidth/FBMBodyTone`) after cloth simulation, so the 18 new shapes do not move cloth-simulated garments (Genessa skirt). Extend the graph to the 24 morphs in a Shared Assets recook (Linux editor) once the morph set is final across the four outfits.
- Staged for the live check: `docs/research/uuepbs/MortalShell2.profile.json` (Breasts = brust001/002, one morph slider per CSS morph); it goes to `ue4ss/Mods/UUEPBS/Scripts/GameProfiles/` with the install go.
