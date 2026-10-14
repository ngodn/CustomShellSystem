# Stable release and bug audit

Updated 2026-10-14. Retains the user's October 12 release-scope correction.

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
| Profile saved at Default visibility retains later overrides | Empty MISC snapshot defect reproduced and corrected offline; legacy missing-field behavior preserved. 26 host suites, three archived states, four catalog cases and Windows build pass | Core-only candidate not installed; verify actual profile action and world/menu visibility restoration |
| Peony excessive jiggle despite Firm/Natural | Restoration correction and tighter limits installed; user accepts walking/Iris Out motion, live limit readback passes | Other six Eve variants and lifecycle/performance regression |
| Peony harsh lighting | User subsequently reports the unchanged build looks fine | No lighting fix established; monitor recurrence separately from sunlight report |
| Older Eve tangent basis | All six regular/facial pairs now pass source, cook, cloth, customization and combined package preservation checks at permanent paths; source and native-material inputs are verified | Candidate installed offline with rollback and unchanged core/settings. Runtime shading/cloth/facial and performance regression remain; not a proven sunlight-noise fix |
| Customize shows only Ground Height after form switch | Mirrored-selection fix installed; user accepts recovery | Preserve through remaining lifecycle checks |
| First Stray summon produces no doubles | User confirms first-switch summon works after selection fix; live clone capture exists | Original cause was not isolated; do not claim a proven independent spawn fix |
| Native Black Hole/Iris Out marked Unavailable | Native alternatives installed; user accepts both playing and interrupting | Full library contact, continuous playback and CINE validation |
| Genessa face and visibility after outfit changes | Owner-restoration/visibility fix installed; cross-character Iris Out and interruption accepted. Both UG variants pass numeric Iris Out/Come Closer tracks, explicit-stop restoration and full Iris Out CINE takes after the natural-end repair | Broader facial/lighting visuals, other directed clips, gameplay interruption, both forms and doubles matrix |
| Genessa rear cloth | World correction accepted; preview clipping explicitly accepted for now | Other poses/variants and cost, without reopening accepted preview limitation |
| CW hair | User accepts installed strand trial and tested face/eyes/interruption | CINE, doubles, full lifecycle and measured cost |
| CW inward/sideways walking feet | User accepts the walking-only correction after the boots-on/off prompt; jog/sprint already accepted. Permanent paths now installed with exact accepted compressed animation data and passing combined catalog/profile checks | Final paths now resolve live (36 matching samples/eight clips); terrain/lifecycle and performance regression checks remain; preserve native IK and rollback |
| CW belt physics and janky skirt | User accepts both while walking/turning. Live Belt20 execution and nine shape-input routing cases pass, with exact saved-state restoration | Extreme-shape visual checks, lifecycle and matched cost; preserve accepted physics and stop solver tuning |
| CW dev/Development display qualifiers | Clean Commander White manifest/catalog name and description installed; packaging recipes updated; assets and IDs unchanged | Final stable version/archive filenames and release text; historical releases remain immutable |
| More than 128 attachments | Fixed caps removed in source; production snapshot fixtures pass through 4,097 children. Thirty live outfit/facial cases preserve identical component/attachment handles at 56 direct children | Live recovery/facial behavior above 128 and broader long-session leak/lifetime checks |
| Vistani moving sunlight reflection noise | Default-off MISC source-angle workaround installed; live 4 to 0.5357 to 4 restoration, actual UI clicks and persistence pass; cold-load ownership failure fixed and tested; sharper shadows visible; user acceptance pending | Verify maps, continuous motion and cost before public delivery; no CVar mutation; do not describe workaround as a proven engine correction |
| Local core reload temporarily loses shared animation/facial availability | Reproduced during CW checks; later rescan experiment crashed in an animation subsystem; cold recovery passes; three controlled reload cycles with class inventories did not reproduce it | Root cause remains unconfirmed; preserve instrumented reproducer and crash evidence; the existing dump lacks the failing class memory |
| Genessa Stray to Faithful after another save | Reported, not reproduced; offline native flow mapped and passive capture prepared | Actual two-save capture, same-save/fresh-launch comparison and valid-transition regression |
| Naylshotte hidden Tarstone effect | Reported with Barrage/Emberseed; permanent Burn system identified. Live capture finds an active effect owned by the hidden weapon, not recently rendered; visible residue is not reproduced | Aim/fire/stow and effect-replacement capture must identify visible residue before a repair; preserve infusion |
| Trebuchaxe hovering axe | Reported, not diagnosed | Identify object and test visibility/Tarstone transitions |
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
- Finish Genessa's two-variant CSS/CINE visual and gameplay coverage and preserve Eve/CW.
  Both variants now pass [numeric facial-track and explicit-stop checks](../research/2026-10-14-genessa-live-face-tracks.md).
  [Actual Iris Out CINE takes](../research/2026-10-14-cine-natural-end.md) also
  finish with exact view/settings restoration after repairing the false
  interruption at natural end. Six close-ups do not close the lighting gate.
- Finish locomotion review. All 59 additional native alternatives were authored
  and installed; validation remains. Do not redo conversion as if it were absent.
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
- Finish shared migration's live CSS/CINE and clean/upgrade coverage. The four
  [current-container compatibility cases](../research/2026-10-14-coordinated-upgrade-audit.md)
  now pass with current/mixed legacy Genessa and CW. They do not test save
  migration or deliver the new stable Genessa/CW versions.
  The [profile Default visibility correction](../research/2026-10-14-profile-default-restore.md)
  now passes offline settings restoration with three archived user states.
  Installation and live profile application remain pending; this does not close
  save migration or missing-mod/reinstall coverage.
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
