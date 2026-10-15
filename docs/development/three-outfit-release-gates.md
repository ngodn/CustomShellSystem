# Stable release and bug audit

Updated 2026-10-15. Retains the user's October 12 release-scope correction.

**Not release-ready.** Targets remain CSS v1.0.0, Eve v1.5.0, Unholy Genessa
v0.3.0 and Commander White v0.1.0, with a separate compatible shared-assets ZIP.
The current shared candidate is staged as 1.0.0, including its nested animation-library version.
The shipping runtime candidate is now `dist/v1.0.0/MSII-CSS-v1.0.0.zip`, with
verified payloads and exact shared dependency hashes. All five local candidates
are listed in `dist/coordinated-v1.0.0.json`. Final tagged delivery remains open.
The source VERSION is restored to 1.0.0; that is a target, not a completed build.

The agent incorrectly treated "skip this outfit and just deliver new Peony"
as permission to narrow the full release and use beta.12. Only Slipstream was
deferred. The old Peony delivery rehearsal is retained under
`dist/withdrawn/eve-v1.5.0-beta12-rehearsal` and is withdrawn from public
delivery. The user requested replacing `dist/eve-v1.5.0` with the current
seven-outfit package; its own status and verification files describe that build. Keep their hashes and test evidence, without calling them upload-ready.
Nothing was uploaded or installed from them. The local beta.12 tag records that
historical rehearsal and is not the stable release tag.

## Current decision rule

The October 15 user correction stops expanding tests after acceptance. The
reported-issue table below distinguishes closed repairs from original release
requirements. A limitation in a research note does not create another gate.
Do not repeat accepted fits, motion judgements, completed gameplay matrices,
provider removal/reinstall, or the attachment-cap check without a new failure
or a relevant implementation change.

The detailed evidence below remains available. Outstanding original work is
still tracked: remaining continuous
visual/contact and physics-cost requirements, clean/legacy installation
coverage, and final coordinated provenance/versions/packages. Resolve each
against existing evidence first. Do not turn each untested combination into
its own experiment. The earlier failed fresh-cache preparation remains recorded;
the later fresh shipping-runtime installation now passes with original settings
and appearance restored. [Evidence](../research/2026-10-15-clean-shipping-runtime.md).

The user explicitly accepts leaving the unreproduced October 13 developer
reload/rescan incident unresolved. It no longer blocks release. Preserve its
evidence, stop further reproduction work unless it recurs, and do not describe
it as fixed or as an established current gameplay failure.

The sunlight feature is now included in the CSS BBCode draft with an actual
MISC screenshot and its sharper-shadow tradeoff. Default (game) is retained.
Adding documentation does not itself establish visual or performance acceptance.

## Reported defects and requested features

| Item | Evidence and actual status | Reported-issue disposition |
| --- | --- | --- |
| Peony oversized/detached/broken outfit | Corrected V2 source, fit accepted in CSS and world | Closed by user acceptance. Preserve the V2 fit; do not repeat fit approval. |
| Peony pockets, robotic ornaments, neckline | Included in the accepted V2 design, with requested gloves/collar/heels/ponytail retained | Closed by user acceptance. Preserve the accepted design. |
| Peony palettes, removable pieces, flowing trim, profiles | User accepts four palettes, flow in CSS/world and restored profiles | Closed by user acceptance. Later both-form Peony doubles and ten-outfit menu checks are recorded below; do not repeat palette/profile approval. |
| Profile saved at Default visibility retains later overrides | Empty MISC snapshot correction installed; offline regression and actual world/preview profile loads pass. Exact saved settings and post-menu world visibility restore | Reported defect closed by installed fix and world/preview profile results. Legacy migration remains a separate original requirement; inactive preview/world actor differences are not a new reported defect. |
| Loading an unavailable outfit profile partly changes settings | Direct-shell and mirrored preflight fixes are installed. Actual cold removal/reinstall now preserves all saved choices, rejects an unavailable profile without state changes, and recovers CW/profile loading after the provider returns | Reported defect closed, including actual missing-provider rejection and reinstall recovery. Clean/legacy upgrade remains a separate original requirement. [Physical cycle](../research/2026-10-15-shared-provider-lifecycle.md) |
| Profile loading uses a separate Darkform choice while mirroring | Selection snapshot restoration and normal mirror reconciliation installed. Offline tests and repeated live Stray loads preserve Peony and the separate CW choice; Faithful/settings/view restore | Reported defect closed at the recorded profile scope. The two-save form/shade check is recorded separately below; do not require every outfit/save combination to close this repair. |
| Hidden item actor flag drifts after form checks | Enforcement repair installed. Three live actor-only faults, Default control, form/profile round trip and exact world-flag restoration pass. Attached Burn rendering also follows repaired owner visibility in controlled checks | Actual draw/fire/stow, native effect replacement and Barrage/Emberseed stowing visibility pass; original triggering sequence remains unproven; sequential timings do not close performance acceptance |
| Peony excessive jiggle despite Firm/Natural | Restoration correction and tighter limits installed; user accepts walking/Iris Out motion, live limit readback passes | Closed by user acceptance and rig readback. The older Eve graph checks and matched body measurements are recorded below; no additional Peony motion approval. |
| Peony harsh lighting | User subsequently reports the unchanged build looks fine | No further change requested after user reported it looks fine. No lighting repair claimed; the separately reproduced sunlight report remains distinct. |
| Older Eve tangent basis | All six regular/facial pairs now pass source, cook, cloth, customization and combined package preservation checks at permanent paths; source and native-material inputs are verified | Candidate installed offline with rollback and unchanged core/settings. Runtime shading/cloth/facial and performance regression remain; not a proven sunlight-noise fix |
| Customize shows only Ground Height after form switch | Mirrored-selection fix installed; user accepts recovery | Closed by installed selection fix and user acceptance; preserve it in final delivery. |
| First Stray summon produces no doubles | User confirms first-switch summon works after selection fix; live clone capture exists | Behavior accepted by user; later both-form clone checks completed. Original cause was not isolated, so do not claim a separate spawn repair. |
| Native Black Hole/Iris Out marked Unavailable | Native alternatives installed; user accepts both playing and interrupting. All 60 native idle choices now pass live discovery and actual carrier/sequence binding on original Genessa, with exact restoration | Reported availability defect closed. Remaining native-library review is a separate original requirement; representative native CINE/playback checks below are already complete. |
| Genessa face and visibility after outfit changes | Owner-restoration/visibility fix installed; cross-character Iris Out and interruption accepted. Both UG variants pass numeric Iris Out/Come Closer tracks, explicit-stop restoration and full Iris Out CINE takes after the natural-end repair | Broader facial/lighting visuals, other directed clips, gameplay interruption, both forms and doubles matrix |
| Genessa rear cloth | World correction accepted; preview clipping explicitly accepted for now | Reported world defect closed by user acceptance. Preview clipping is an accepted limitation. Any remaining measured cost belongs to the original performance gate. |
| CW hair | User accepts installed strand trial and tested face/eyes/interruption | CINE, doubles, full lifecycle and measured cost |
| CW inward/sideways walking feet | User accepts the walking-only correction after the boots-on/off prompt; jog/sprint already accepted. Permanent paths now installed with exact accepted compressed animation data and passing combined catalog/profile checks | Reported tilt closed by user acceptance; permanent bindings verified in 36 samples across eight clips. Preserve native IK and accepted jog/sprint. Do not request another walking judgement. |
| CW belt physics and janky skirt | User accepts both while walking/turning. Live Belt20 execution and nine shape-input routing cases pass, with exact saved-state restoration | Reported motion defect closed by user acceptance. Preserve Belt20 and skirt. Supported-shape visuals and measured physics cost remain original requirements, not permission for more solver tuning. |
| CW dev/Development display qualifiers | Clean Commander White manifest/catalog name and description installed; packaging recipes updated; assets and IDs unchanged | Final stable version/archive filenames and release text; historical releases remain immutable |
| More than 128 attachments | Production snapshot fixtures pass through 4,097 children. Three live Peony facial/normal cycles pass at 129 direct children; 73 temporary empty components removed and original handles/parents/sockets/settings restored | Closed: production snapshot coverage exceeds 128 and actual live recovery/facial cycles pass at 129. No additional arbitrary attachment-count stress is required. |
| Vistani moving sunlight reflection noise | Default-off MISC source-angle workaround installed; live 4 to 0.5357 to 4 restoration, actual UI clicks and persistence pass; cold-load ownership failure fixed and tested; sharper shadows visible; user acceptance pending | Verify maps, continuous motion and cost before public delivery; no CVar mutation; do not describe workaround as a proven engine correction |
| Local core reload temporarily loses shared animation/facial availability | Reproduced during CW checks; later rescan experiment crashed in an animation subsystem; cold recovery passes; subsequent reload/rescan checks, including the installed readiness core, did not reproduce it | User accepts the historical developer-test incident unresolved on October 15. No release blocker or further reproduction work unless it recurs. Preserve evidence; no crash repair claimed. |
| Genessa Stray to Faithful after another save | Native blocker explanation and optional confirmed one-time override installed. Live Confirm/Cancel, return to Stray, unchanged upgrade/blocker ownership, native restriction restored, and F/Escape pass. Upgrades/progression preserved; native form/health saving is explicitly allowed | Fresh-launch and normal two-save round trip now pass: valid transitions work on slot 2, slot 1 retains its native restriction, and original appearance/settings restore. Do not claim saved form/health stay untouched. [Override evidence](../research/2026-10-15-genessa-override.md) |
| Naylshotte hidden Tarstone effect | Controlled mesh-only hiding now reproduces visible orange Burn; the installed owner-visibility repair removes it while preserving the active infusion. Front/rear rendering controls and exact restoration pass | Burn draw/fire/stow passes with infusion preserved and a 28-point Resolve drop. A reloaded-save Bleed draw/stow case also passes with resources/settings restored. Native Burn/Bleed replacement now passes six stowed/drawn/restowed phases with full loadout and infusion restoration. Barrage/Emberseed now passes actual secondary fire: 40 Resolve, three groups of eight projectiles still visible after the weapon and attached Burn are hidden, then native projectile cleanup. Original triggering sequence and damage remain open |
| Trebuchaxe hovering axe | Reproduced as CSS revealing native hidden `SM_Projectile_Preview`. Nested-visibility repair installed; 13 draw/stow/mode/drift phases and charged firing on the original idle pass, with a recorded 30-point Resolve drop | Fresh equip after restart/save loading and all eight sidearms now pass seven visibility phases each (56 total). Final equipment, infusion, settings, resources and view restore; the retained exact-angle comparison failure is independently resolved as floating-point rounding. Fresh-equip early-release firing now passes: native minimum-charge completion, 30 Resolve, preview hidden throughout and actual projectile visible after stow, with full restoration. Trebuchaxe-specific Tarstone combinations and damage remain open; retain earlier failed measurement receipts |
| Merrick and straightforward boss adaptations | Merrick and four isolated boss rigs installed. All four pass player-sized scaling, native weapon sizes and restoration. Droeg's CSS preview passes. All five complete short forward/back walking. Permanent boss paths are integrated into the installed shared pack; normal restart verifies all four mesh/Skeleton bindings and restoration. The 283 prior shared packages remain unchanged. Orrem's multipart forms are deferred | Feature integration complete; include the new shared container hashes in coordinated delivery metadata. Short walking is verified, not a full combat matrix. Individual v1.0.1 deferrals are recorded in the boss research note |
| Zhirelle | Explicitly deferred to CSS v1.0.1 on October 15 | Preserve sources and rejected trial evidence; no current release gate |
| Native shell shades for appearance overrides | Installed: sixteen shades across eight shells, native unlock checks, Original restoration and world/menu materials verified. Obsidian Eredrim survives a normal restart and profile save/change/load/repeated-load with preview/world material parity and exact settings restoration | Implemented and verified, including the normal two-save/form round trip, selected shade and independent appearance restoration. Preserve unlock restrictions and the controller chooser. |
| Slipstream | Explicitly deferred by user | Retain sources/evidence; no new experiments |
| Minor unobtrusive clipping | Explicitly deferred by user | Record visible limitations for later polish |
| Expression list and controller shade selection | Corrected core installed: expressions show directly with left/right cycling; Choose shade opens the native picker and respects unlocks. Selection, cancellation and saved-settings restoration verified with keyboard inputs; native controller mappings inspected | Closed at the verified UI scope. Final shell-list follow-up passes ten choices, no embedded shade rows, visible scrollbar and exact restoration. Earlier failed geometry guards remain historical evidence. Physical gamepad input was not exercised; keyboard equivalents and native mappings were checked. [Evidence](../research/2026-10-15-shade-expression-ui.md) |

These rows summarize current evidence in the
[implementation status](three-outfit-rig-physics-status.md),
[Peony gameplay record](../research/2026-10-12-peony-v2-gameplay.md),
[form-selection investigation](genessa-form-selection-regression.md),
[attachment investigation](../research/2026-10-11-attachment-limits.md), and
[sunlight record](../research/2026-10-12-sunlight-noise.md).
Accepted observations apply to their recorded test scope, not every variant.

## Requirements still preventing completion

- Account for all nine original repair variants plus accepted Peony, including
  normal/facial sources, final cooked assets and required morph/pose checks.
  [Genessa's two facial packets](../research/2026-10-15-genessa-facial-source-replay.md)
  now reproduce byte-for-byte from retained inputs. Original W3/editor-module
  hashes and current installed facial geometry/morph comparisons pass. Five
  Blender authoring stages now also regenerate the facial data, allowing only
  the verified new preview provenance; both final packets remain byte-exact.
  The [installed Genessa package lineage](../research/2026-10-15-genessa-package-lineage.md)
  now verifies 1,769 retained files through the namespace, cloth and final hair
  stages, with exact installed trio hashes and 261 preserved non-graph payloads.
  Genessa's [original material and rig source chains](../research/2026-10-15-genessa-material-rig-sources.md)
  also pass: 55 authored material/texture sources, 13 original rig/mesh sources,
  and exact replay of all 33 preserved material instances. The
  [W3 editable baseline link](../research/2026-10-15-genessa-w3-sources.md)
  now verifies the original creation inputs and exact identity at both later
  consumers; the original full-data wing comparison reruns byte-exactly.
  Genessa's retained W3-to-current-package chain is accounted for. Recreating
  the outfit from its earliest Blender scene or repeating an unchanged cook is
  not an extra gate. Remaining full-goal source coverage and final coordinated
  artifact provenance still apply.
  [CW's accepted repair lineage](../research/2026-10-15-cw-repair-lineage.md)
  now verifies 578 retained files through hair, ankle, belt and walking stages.
  The installed permanent walk preserves all 445 other payloads, and all nine
  renamed assets match the accepted decoded trial. The
  [CW original baseline](../research/2026-10-15-cw-baseline-sources.md) now also
  verifies 1,622 files, 172 editable sources and all 413 baseline payloads.
  Fresh relocation reproduces the 78 textures and two preserved body graphs
  exactly. CW baseline and repair provenance are accounted for; coordinated
  final artifact provenance and runtime requirements remain open.
  [Peony V2's retained source chain](../research/2026-10-15-peony-source-chain.md)
  now verifies 610 files, all 294 Peony payloads and twelve shared doubles
  material exports through the installed Eve/shared packages. Frozen recipes,
  editable cook inputs and original shared-container identity are accounted
  for. Historical game-container links are recorded separately. No new cook
  or gameplay acceptance is claimed; coordinated final artifact provenance
  and the existing runtime requirements remain open.
- Finish the remaining runtime checks and other non-deferred defects. Preserve accepted
  belt/skirt motion, Genessa cloth and Peony motion/design; complete their separate
  regression and performance requirements without reopening accepted minor flaws.
- Complete the goal's Phase 4 matrix: actions, preview/world, lifecycle,
  both-form doubles, customization, attachments/gameplay and existing UI fixes.
  [Ten-outfit CSS-menu recovery](../research/2026-10-15-menu-recovery.md) now
  passes preview/world body settings, morph and section-visibility parity, plus
  active primary-rig update recovery after close in Stray. All saved state,
  resources, view, actual idle and nested weapon flags restore. Continuous
  visuals, attached cloth, forced LODs and other-form coverage remain separate.
  [Twenty actual aim interruptions](../research/2026-10-15-outfit-aim-interruption.md)
  now pass for Iris Out and Come Closer across the ten release variants in
  Faithful form, with preserved settings, infusion and view.
  [Twenty actual secondary-attack interruptions](../research/2026-10-15-outfit-attack-interruption.md)
  now pass both clips on all ten variants in Stray form, with raw native
  ability activation and full state/view restoration independently verified.
  [Twenty actual stationary dodge interruptions](../research/2026-10-15-outfit-dodge-interruption.md)
  now pass both clips on all ten variants in Stray, with native activation,
  complete restoration and retained failed downhill preflight/recovery evidence.
  [Twenty actual two-hit combo interruptions](../research/2026-10-15-outfit-combo-interruption.md)
  now pass both clips on all ten variants in Stray. Raw native records confirm
  B1-to-B2 progression, released facial ownership and complete restoration.
  [Twenty normal Genessa ability interruptions](../research/2026-10-15-shell-ability-interruption.md)
  pass both clips on all ten variants in Stray. Both upgraded-save clones appear
  and expire, native input blocks clear, and settings/resources/view restore.
  Independent raw-log verification rejects seven corrupted records; final live
  restoration is confirmed without changing the accepted core or assets.
  [Twenty parry interruptions](../research/2026-10-15-outfit-parry-interruption.md)
  now pass both clips on all ten variants in Stray, with forty raw native parry
  observations, six rejected controls and complete restoration. This covers
  parry initiation, not incoming-hit deflection, riposte or damage response.
  Moving/directional dodge, interaction, damage, other-form cases and
  the rest of this matrix remain open.
  [Peony's four normal summon cases](../research/2026-10-15-peony-doubles.md)
  verify Default/Use CSS selection, body-input copying and cleanup for Faithful
  and Stray primary. Focused follow-ups also verify both forms' six Peony ghost
  families and the current material parameters, morphs and hidden sections.
  [CW's two Stray clones](../research/2026-10-15-outfit-doubles.md) now pass
  Default and Use CSS; the completed CSS check restores full settings, CINE,
  idle and player/view. Six older Eve variants pass Faithful copy binding and
  cleanup. Peony and both UG variants also pass both Stray clones in Use CSS,
  with full restoration across the three-case sequence. All six older Eve
  secondary cases now pass too, completing Stray mesh-copy/cleanup coverage
  for all ten appearances. Peony secondary ghost parameters, morphs and hidden
  sections also pass with explicit actor ownership. The three remaining Faithful UG/CW cases now pass, completing Faithful
  copy binding and cleanup across all ten appearances. A separate guarded
  refund and read-only recovery restore the 56.25 Resolve consumed by those
  summons; the original resource-restoration failure is retained. Cloth/contact
  and other material/customization combinations remain open.
- Finish Genessa's two-variant CSS/CINE visual and gameplay coverage and preserve Eve/CW.
  Both variants now pass [numeric facial-track and explicit-stop checks](../research/2026-10-14-genessa-live-face-tracks.md).
  [Actual Iris Out CINE takes](../research/2026-10-14-cine-natural-end.md) also
  finish with exact view/settings restoration after repairing the false
  interruption at natural end. Six close-ups do not close the lighting gate.
- Finish locomotion review. All 59 additional native alternatives were authored
  and installed; validation remains. Do not redo conversion as if it were absent.
  The [60-choice live idle check](../research/2026-10-15-native-idle-library.md)
  passes discovery and actual carrier bindings on original Genessa. It does not
  substitute for visual/contact, gait, full-clip or gameplay-interruption checks.
  [Native Iris Out CINE](../research/2026-10-15-native-cine.md) now passes a full
  take and explicit exit, with the correct montage released and state restored.
  [Native playback controls](../research/2026-10-15-native-playback-controls.md)
  also pass representative loop, pose hold, pause/resume and actual parry
  interruption on original Genessa in Stray. Other native clips/targets, contact,
  facial support and broader gameplay remain separate.
- Complete matched performance measurements, repeated-switch lifetime checks
  and investigation of any new hitch, crash or regression.
  The [matched visibility-repair comparison](../research/2026-10-15-visibility-repair-cost.md)
  completes nine candidate/baseline/candidate episodes, 162 owner-drift repairs
  and 4,873 frames. MISC timings are similar; whole-game FPS, exclusive event
  cost, cloth/GPU and broader transition performance are not established by it.
  The repaired core and all original user state restore, including live idle
  playback. Its failed body-weapon fixture and recovery remain recorded.
  The [30-case stationary outfit/facial check](../research/2026-10-14-outfit-switch-lifecycle.md)
  passes with exact component identities and state restoration; broader lifecycle
  and frame-time measurements remain separate.
  A [three-capture Peony timing reference](../research/2026-10-14-peony-stationary-cost.md)
  now has 3,133 verified records and unchanged boundary contexts/settings.
  It has substantial tail variation, no matched comparator and no isolated
  rig/cloth or GPU timings. The game's trace interface exposes no channels.
  Performance acceptance, movement/transition and frame-rate coverage remain open.
  [Nine native-cap Peony captures](../research/2026-10-15-native-frame-caps.md)
  now preserve console priority and settings, with 4,151 verified timing records
  and advancing finite body state. Actual cadence is about 30 and 53 to 55 Hz;
  sustained 60/higher-rate and matched comparisons remain open. The broader
  observation check below covers the other outfits.
  The [broader outfit cap check](../research/2026-10-15-outfit-frame-caps.md)
  now completes stationary observation coverage for all ten release variants:
  90 captures and 40,763 raw records, with identical graphics context and full
  parent/live restoration independently verified. Alice records two hair and
  two body resets in a cap-60 condition with two long frames. Earlier Skin Suit
  and Knitwear failed attempts remain retained. The receipt verifies observation
  integrity, not an overall stability pass or a proven cause for every reset.
  Active primary counter coverage includes CW's strands/body/Belt20; UG hair is
  disabled in the sampled setups, and separate cloth cost remains unmeasured.
  Accepted assets, solver thresholds and production core remain unchanged.
  The [paired editor body-cost fixture](../research/2026-10-15-body-cost.md)
  now verifies 48 enabled/bypassed cases and 28,800 measured updates at
  30/60/120 Hz. It estimates incremental body work with substantial variance;
  it does not substitute for actual game frame-rate, cloth/GPU or other-rig checks.
  The [live body enabled/bypassed comparison](../research/2026-10-15-live-body-cost.md)
  now verifies four pairs each for Peony, Genessa and CW: 24 captures
  and 12,380 frames, with preserved body inputs, no sampled secondary-rig resets
  and complete restoration. Median paired frame-interval changes are +0.061 ms,
  -0.082 ms and +0.025 ms respectively, with variable tails. The combined
  evidence verifier passes; failed captures and diagnostic recovery are retained. This is whole-frame observation, not exclusive rig/GPU timing or
  full performance acceptance.
- Finish shared migration's live CSS/CINE and clean/upgrade coverage. The four
  [current-container compatibility cases](../research/2026-10-14-coordinated-upgrade-audit.md)
  now pass with current/mixed legacy Genessa and CW. They do not test save
  migration or deliver the new stable Genessa/CW versions.
  [Eight current shared-dependency cases](../research/2026-10-15-current-shared-dependencies.md)
  also pass production catalog checks for missing, outdated, duplicate and
  retained standalone providers, with actionable messages and all ten facial
  matches in valid cases. Export inventories contain no divergent overlaps.
  This closes those offline current-package cases. The later
  [cold missing-provider cycle](../research/2026-10-15-shared-provider-lifecycle.md)
  verifies physical removal/reinstall and preserved saved choices on the current
  Stray save. A [fresh shipping-runtime installation](../research/2026-10-15-clean-shipping-runtime.md)
  now also passes default-state creation, current-provider discovery and Peony
  facial playback, followed by exact original CSS-state and appearance
  restoration. The [executed October 8 upgrade](../research/2026-10-16-upgrade-evidence.md)
  already replaced six old trios with four providers and preserved 59 protected
  files. Its plan and eighteen retained original files verify. Do not repeat
  that physical replacement. Exact pre-migration profile replay is not
  established by its retained hash alone; keep profile coverage distinct from
  the completed installation and catalog cases.
  The [profile Default visibility correction](../research/2026-10-14-profile-default-restore.md)
  now passes offline settings restoration with three archived user states and
  actual world/preview profile loads on the installed core. It does not close
  broader save migration or missing-mod/reinstall coverage.
  [Unavailable-profile preflight](../research/2026-10-15-profile-dependencies.md)
  now prevents catalog rejection from partially restoring global settings.
  Offline rejection/reinstall, live normal loads and actual missing-provider
  cold-start rejection/recovery now pass. [Mirrored profile restoration](../research/2026-10-15-profile-forms.md)
  now passes repeated Stray loads and a Faithful return with independent saved
  choices preserved. This single-save test does not close the cross-save report
  or the full lifecycle matrix.
  The [hidden-item actor repair](../research/2026-10-15-item-visibility-drift.md)
  also closes the observed actor-flag drift after that sequence. It does not
  close the original Tarstone gameplay report. Follow-up [Burn rendering controls](../research/2026-10-15-sidearm-effect-visibility.md)
  reproduce a visible lingering effect under mesh-only hiding and remove it
  through the owner path without disabling infusion. [Actual draw/fire/stow](../research/2026-10-15-sidearm-aim.md)
  also passes with preserved infusion. Native effect replacement and
  [Barrage/Emberseed firing and stowing](../research/2026-10-15-sidearm-barrage.md)
  now pass with full restoration; damage and the original natural trigger
  remain open. Its short
  sequential timing comparison remains part of the open performance review.
- Prepare coordinated stable artifacts only after applicable gates pass or the
  user explicitly accepts a remaining limitation. Verify final versions,
  dependency fingerprints, source/tag/build provenance, checksums and notes.
  Supply four simple BBCode changelogs (CSS, Eve, Genessa and CW), with
  shared-assets changes included in the CSS changelog and post thread. No separate
  shared-assets BBCode file. Put additions/new features first with actual-build
  screenshots uploaded to the project Git repository and embedded through raw
  GitHub URLs; fixes/improvements afterward without screenshots. Follow the
  user's compact version heading and bullet-list example. Screenshot uploading
  is authorized, but release publication remains gated by the requirements above.

The low-priority follow-ups remain explicitly queued, not silently fixed or
discarded. They retain their original scheduling in the full goal. This audit
does not invent new deferrals for the primary repairs or release gates.

## Immediate continuation

Clean-runtime installation remains unverified: preparation detected movement,
but a mistaken normal quit followed. The unchanged original setup was relaunched
and Merrick plus exact CSS state restored. No install/package move occurred.
The retained recipe now rejects quit without successful preparation. Do not
count this as an installation pass or repeat it while the game may be in use.

Release source validation now rejects lightweight tags as well as mismatched
HEAD and dirty worktrees. Twelve packaging tests pass. Final annotated-tag
build and coordinated artifact provenance remain separate delivery steps.

The [asset-readiness candidate](../research/2026-10-15-asset-loading-readiness.md)
closes an unchecked return of partially loaded/destroying assets. Its actual
loader regression, 27 host suites and both Windows builds pass. The development
core is now installed with verified mapping and unchanged saved state. The
changed loader also passes CW facial loading, one catalog rescan and advancing
Iris Out playback, with exact Merrick/settings/view restoration. The original
reload/rescan incident is now an explicitly accepted limitation and no longer
blocks release. Do not label this candidate as its repair or repeat already
accepted outfit checks.


The [runtime review candidate](../research/2026-10-15-stable-runtime-review.md)
fixes a missing-provider bypass for mirrored profiles and unsafe timeout cleanup
in a developer reference-pose diagnostic. Host and Python regressions, archived
profiles and development/shipping builds pass. Guarded development-core activation
and repeated normal live profile loads now pass, with full saved-state, actual
idle and nested weapon-visibility restoration. Missing-provider uninstall/reinstall
now also passes the linked physical cycle above. The developer trial cleanup
itself has no new live coverage. Continue the
remaining lifecycle and coordinated-release requirements below.

The [combined surface candidate](../research/2026-10-14-eve-surface-tangents.md#combined-eve-package)
now passes all twelve mesh-preservation checks, 1,545 catalog checks and the
1,008-package/272-reference audit. Customization targets and the guarded
installation plan pass. The [source/package chain](../research/2026-10-14-eve-source-package-chain.md)
accounts for the repaired editor meshes and 273 older material/texture packages.
The combined candidate is installed with rollback and exact post-install
readback. The user has since opened the game; Peony's live bindings and tighter
motion limits pass a read-only capture. Black Pearl/Planet adapter and rig reads
pass with exact state restoration; their shaded stills do not establish sunlight
or continuous-motion acceptance. Preserve accepted Peony/CW motion and the separate MISC workaround.
Runtime checks and the full stable-release gates above remain open.

All 46 parts across the six original Eve outfits now have passing editable-source
comparisons linked to their accepted imports and installed geometry checks.
Black Pearl's complete export also reproduces byte-for-byte. Recovered Alice,
Planet and Skin Suit parts are explicitly marked as import-derived sources.
See [source coverage](../research/2026-10-14-eve-six-source-coverage.md).
The repaired mesh/material assembly and cooked surface checks now pass offline.
Eve's body source/cook and four supporting rig source/cook chains now also pass,
and the current writer reproduces the preserved secondary graph exactly. See
[motion provenance](../research/2026-10-14-eve-motion-preservation.md).
Full goal source coverage and live regression remain open; source verification
does not require another install.

The older Eve saved-graph input/update check now passes all 36 cases with each
outfit's actual post-process graph, including Bikini/Knit feet graphs, and both
normal/facial meshes. See the
[regression record](../research/2026-10-12-eve-existing-motion-regression.md).
This is not full visual or cloth acceptance. CW's new walking-only trial is
installed with the developer core `acab6363906f0e88`; the user now confirms
walking looks right after the booted/bare prompt. Its source records, fourteen directional
editor replays, standalone cooked readback and combined catalog checks are in CW
`work/foot-walk-orientation3`. Existing jog/sprint and accepted belt/skirt assets
are unchanged. The permanent-path follow-up is now installed from CW
`work/walk-final-paths1`, with exact accepted compressed data and passing combined
package checks. [Live path resolution](../research/2026-10-14-cw-release-live-bindings.md)
now passes for the final BlendSpace and all eight directional clips. Native rig reference replacement
did not establish an improvement; phase matching shows the active rig preserves
foot orientation while adjusting position. Keep native grounding. The narrower
ankle alignment improves mean reference bank in walk/jog/sprint but not every
phase. Sparse boot contact has approximately 2.2 cm penetration in both the
captured trial and its fixed-parent original-ankle counterfactual. Do not apply
a global height offset from that evidence. See CW `docs/walking-foot-report.md`,
`docs/ankle-gait-regression.md` and `docs/foot-contact-readback.md`. Complete the
remaining full-cycle and terrain checks, then continue the
other repairs and stable-release gates above. Do not
repackage a beta bundle or treat Peony's acceptance as completion of the others.

Actual original-package booted/bare straight walks now also confirm pre-existing
boot penetration and bare-foot clearance. Trial samples show lower mean bank,
but phases differ and do not establish a contact change. Original rollback and
all comparison evidence are retained; trial restoration/health passes. See CW
`docs/straight-walk-contact.md#actual-original-package-comparison`.
