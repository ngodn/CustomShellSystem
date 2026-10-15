# Stable release and bug audit

Updated 2026-10-15. Retains the user's October 12 release-scope correction.

**Not release-ready.** Targets remain CSS v1.0.0, Eve v1.5.0, Unholy Genessa
v0.3.0 and Commander White v0.1.0, with a separate compatible shared-assets ZIP.
The shared package's final version and metadata still need coordinated staging.
The source VERSION is restored to 1.0.0; that is a target, not a completed build.

The agent incorrectly treated "skip this outfit and just deliver new Peony"
as permission to narrow the full release and use beta.12. Only Slipstream was
deferred. The Peony-only archives in `dist/eve-v1.5.0` are withdrawn from public
delivery. Keep their hashes and test evidence, without calling them upload-ready.
Nothing was uploaded or installed from them. The local beta.12 tag records that
historical rehearsal and is not the stable release tag.

## Reported defects and requested features

| Item | Evidence and actual status | Remaining work |
| --- | --- | --- |
| Peony oversized/detached/broken outfit | Corrected V2 source, fit accepted in CSS and world | Preserve accepted candidate; remaining regression matrix |
| Peony pockets, robotic ornaments, neckline | Included in the accepted V2 design, with requested gloves/collar/heels/ponytail retained | Preserve accepted appearance |
| Peony palettes, removable pieces, flowing trim, profiles | User accepts four palettes, flow in CSS/world and restored profiles | Both-form and broader lifecycle coverage |
| Profile saved at Default visibility retains later overrides | Empty MISC snapshot correction installed; offline regression and actual world/preview profile loads pass. Exact saved settings and post-menu world visibility restore | Broader profile/lifecycle coverage; preserve missing legacy fields. Intermediate inactive-world-actor differences inside the menu are documented |
| Loading an unavailable outfit profile partly changes settings | Catalog preflight installed; missing outfit, variant and incompatible-shell rejection pass offline without changing saved state. Normal repeated world loads pass on the new core | Actual missing-provider/reinstall lifecycle and runtime asset failures remain outside this check |
| Profile loading uses a separate Darkform choice while mirroring | Selection snapshot restoration and normal mirror reconciliation installed. Offline tests and repeated live Stray loads preserve Peony and the separate CW choice; Faithful/settings/view restore | Single-save coverage only; independent-form activation, other outfits and cross-save report remain distinct |
| Hidden item actor flag drifts after form checks | Enforcement repair installed. Three live actor-only faults, Default control, form/profile round trip and exact world-flag restoration pass. Attached Burn rendering also follows repaired owner visibility in controlled checks | Actual draw/fire/stow now passes; original Tarstone triggering sequence and effect replacement remain unproven; sequential timings do not close performance acceptance |
| Peony excessive jiggle despite Firm/Natural | Restoration correction and tighter limits installed; user accepts walking/Iris Out motion, live limit readback passes | Other six Eve variants and lifecycle/performance regression |
| Peony harsh lighting | User subsequently reports the unchanged build looks fine | No lighting fix established; monitor recurrence separately from sunlight report |
| Older Eve tangent basis | All six regular/facial pairs now pass source, cook, cloth, customization and combined package preservation checks at permanent paths; source and native-material inputs are verified | Candidate installed offline with rollback and unchanged core/settings. Runtime shading/cloth/facial and performance regression remain; not a proven sunlight-noise fix |
| Customize shows only Ground Height after form switch | Mirrored-selection fix installed; user accepts recovery | Preserve through remaining lifecycle checks |
| First Stray summon produces no doubles | User confirms first-switch summon works after selection fix; live clone capture exists | Original cause was not isolated; do not claim a proven independent spawn fix |
| Native Black Hole/Iris Out marked Unavailable | Native alternatives installed; user accepts both playing and interrupting. All 60 native idle choices now pass live discovery and actual carrier/sequence binding on original Genessa, with exact restoration | Full library contact, continuous playback, other native meshes and CINE validation |
| Genessa face and visibility after outfit changes | Owner-restoration/visibility fix installed; cross-character Iris Out and interruption accepted. Both UG variants pass numeric Iris Out/Come Closer tracks, explicit-stop restoration and full Iris Out CINE takes after the natural-end repair | Broader facial/lighting visuals, other directed clips, gameplay interruption, both forms and doubles matrix |
| Genessa rear cloth | World correction accepted; preview clipping explicitly accepted for now | Other poses/variants and cost, without reopening accepted preview limitation |
| CW hair | User accepts installed strand trial and tested face/eyes/interruption | CINE, doubles, full lifecycle and measured cost |
| CW inward/sideways walking feet | User accepts the walking-only correction after the boots-on/off prompt; jog/sprint already accepted. Permanent paths now installed with exact accepted compressed animation data and passing combined catalog/profile checks | Final paths now resolve live (36 matching samples/eight clips); terrain/lifecycle and performance regression checks remain; preserve native IK and rollback |
| CW belt physics and janky skirt | User accepts both while walking/turning. Live Belt20 execution and nine shape-input routing cases pass, with exact saved-state restoration | Extreme-shape visual checks, lifecycle and matched cost; preserve accepted physics and stop solver tuning |
| CW dev/Development display qualifiers | Clean Commander White manifest/catalog name and description installed; packaging recipes updated; assets and IDs unchanged | Final stable version/archive filenames and release text; historical releases remain immutable |
| More than 128 attachments | Production snapshot fixtures pass through 4,097 children. Three live Peony facial/normal cycles pass at 129 direct children; 73 temporary empty components removed and original handles/parents/sockets/settings restored | Live snapshot-cap check closed. Rendered-accessory/follower/offset stress and broader long-session leak/lifetime checks remain distinct |
| Vistani moving sunlight reflection noise | Default-off MISC source-angle workaround installed; live 4 to 0.5357 to 4 restoration, actual UI clicks and persistence pass; cold-load ownership failure fixed and tested; sharper shadows visible; user acceptance pending | Verify maps, continuous motion and cost before public delivery; no CVar mutation; do not describe workaround as a proven engine correction |
| Local core reload temporarily loses shared animation/facial availability | Reproduced during CW checks; later rescan experiment crashed in an animation subsystem; cold recovery passes; three controlled reload cycles with class inventories did not reproduce it | Root cause remains unconfirmed; preserve instrumented reproducer and crash evidence; the existing dump lacks the failing class memory |
| Genessa Stray to Faithful after another save | Upgraded-save failure explained by Alteration's native revival blocker. Immediate explanatory message installed and verified with state/resources/view unchanged | Complete two-save/fresh-launch and valid-transition regression. Optional confirmed override queued, with autosave impact unresolved |
| Naylshotte hidden Tarstone effect | Controlled mesh-only hiding now reproduces visible orange Burn; the installed owner-visibility repair removes it while preserving the active infusion. Front/rear rendering controls and exact restoration pass | Actual draw/fire/stow passes with infusion preserved and a 28-point Resolve drop. Original triggering sequence, effect replacement, Barrage and damage remain untested |
| Trebuchaxe hovering axe | Reproduced as CSS revealing native hidden `SM_Projectile_Preview`. Nested-visibility repair installed; 13 draw/stow/mode/drift phases and charged firing on the original idle pass, with a recorded 30-point Resolve drop | Verify other sidearms, Tarstone combinations, uncharged firing, damage and fresh launch; retain earlier failed measurement receipts |
| Zhirelle/Merrick and matching-skeleton NPCs | Explicit low-priority follow-up, not implemented | Asset/skeleton compatibility investigation and validated selector support |
| Native shell shades for appearance overrides | Explicit low-priority follow-up, not implemented | Supported shade selection, persistence and unlock-preserving validation |
| Slipstream | Explicitly deferred by user | Retain sources/evidence; no new experiments |
| Minor unobtrusive clipping | Explicitly deferred by user | Record visible limitations for later polish |

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
- Finish the remaining runtime checks and other non-deferred defects. Preserve accepted
  belt/skirt motion, Genessa cloth and Peony motion/design; complete their separate
  regression and performance requirements without reopening accepted minor flaws.
- Complete the goal's Phase 4 matrix: actions, preview/world, lifecycle,
  both-form doubles, customization, attachments/gameplay and existing UI fixes.
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
  sections also pass with explicit actor ownership. Faithful UG/CW, cloth/contact
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
  Other native clips, targets, facial support and gameplay remain separate.
- Complete matched performance measurements, repeated-switch lifetime checks
  and investigation of any new hitch, crash or regression.
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
  sustained 60/higher-rate, other-outfit and matched comparisons remain open.
  The [broader outfit cap check](../research/2026-10-15-outfit-frame-caps.md)
  verifies nine captures each for Black Pearl, Planet, Skin Suit and Bikini.
  Earlier Skin Suit and later Knitwear attempts record resets and stop their
  matrices, with complete restoration. Long-frame recovery is a hypothesis;
  every reset is not yet explained. Observation-mode collection is running and preserves
  reset events as unresolved without stopping later timing coverage. Strict
  stability verification and accepted solver thresholds remain unchanged.
  The [paired editor body-cost fixture](../research/2026-10-15-body-cost.md)
  now verifies 48 enabled/bypassed cases and 28,800 measured updates at
  30/60/120 Hz. It estimates incremental body work with substantial variance;
  it does not substitute for actual game frame-rate, cloth/GPU or other-rig checks.
- Finish shared migration's live CSS/CINE and clean/upgrade coverage. The four
  [current-container compatibility cases](../research/2026-10-14-coordinated-upgrade-audit.md)
  now pass with current/mixed legacy Genessa and CW. They do not test save
  migration or deliver the new stable Genessa/CW versions.
  The [profile Default visibility correction](../research/2026-10-14-profile-default-restore.md)
  now passes offline settings restoration with three archived user states and
  actual world/preview profile loads on the installed core. It does not close
  broader save migration or missing-mod/reinstall coverage.
  [Unavailable-profile preflight](../research/2026-10-15-profile-dependencies.md)
  now prevents catalog rejection from partially restoring global settings.
  Offline rejection/reinstall cases and live normal profile loads pass; real
  installation transitions remain open. [Mirrored profile restoration](../research/2026-10-15-profile-forms.md)
  now passes repeated Stray loads and a Faithful return with independent saved
  choices preserved. This single-save test does not close the cross-save report
  or the full lifecycle matrix.
  The [hidden-item actor repair](../research/2026-10-15-item-visibility-drift.md)
  also closes the observed actor-flag drift after that sequence. It does not
  close the original Tarstone gameplay report. Follow-up [Burn rendering controls](../research/2026-10-15-sidearm-effect-visibility.md)
  reproduce a visible lingering effect under mesh-only hiding and remove it
  through the owner path without disabling infusion. [Actual draw/fire/stow](../research/2026-10-15-sidearm-aim.md)
  also passes with preserved infusion; effect replacement, Barrage and damage
  remain open. Its short
  sequential timing comparison remains part of the open performance review.
- Prepare coordinated stable artifacts only after applicable gates pass or the
  user explicitly accepts a remaining limitation. Verify final versions,
  dependency fingerprints, source/tag/build provenance, checksums and notes.

The low-priority follow-ups remain explicitly queued, not silently fixed or
discarded. They retain their original scheduling in the full goal. This audit
does not invent new deferrals for the primary repairs or release gates.

## Immediate continuation

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
