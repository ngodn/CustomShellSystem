# Stable release and bug audit

Updated 2026-10-12 after the user's release-scope correction.

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
| Peony excessive jiggle despite Firm/Natural | Restoration correction and tighter limits installed; user accepts walking/Iris Out motion, live limit readback passes | Other six Eve variants and lifecycle/performance regression |
| Peony harsh lighting | User subsequently reports the unchanged build looks fine | No lighting fix established; monitor recurrence separately from sunlight report |
| Customize shows only Ground Height after form switch | Mirrored-selection fix installed; user accepts recovery | Preserve through remaining lifecycle checks |
| First Stray summon produces no doubles | User confirms first-switch summon works after selection fix; live clone capture exists | Original cause was not isolated; do not claim a proven independent spawn fix |
| Native Black Hole/Iris Out marked Unavailable | Native alternatives installed; user accepts both playing and interrupting | Full library contact, continuous playback and CINE validation |
| Genessa face and visibility after outfit changes | Owner-restoration/visibility fix installed; cross-character Iris Out and interruption accepted | Both Genessa variants, CSS/CINE, both forms and doubles matrix |
| Genessa rear cloth | World correction accepted; preview clipping explicitly accepted for now | Other poses/variants and cost, without reopening accepted preview limitation |
| CW hair | User accepts installed strand trial and tested face/eyes/interruption | CINE, doubles, full lifecycle and measured cost |
| CW inward/sideways walking feet | Four-bone reference-alignment trial installed; booted/bare short walks restore state; six editor gait cases improve mean bank but some phases worsen; sparse world-contact measurements retained | Full-cycle gameplay, terrain and visual acceptance; preserve native IK and original rollback |
| CW belt physics and janky skirt | Queued after Eve, not completed | Author belt response, diagnose skirt motion, verify contact/recovery/cost |
| CW dev/Development display qualifiers | Requested cleanup, final stable metadata not delivered | Audit names/descriptions/archive filenames; retain IDs and asset paths |
| More than 128 attachments | Fixed caps removed in source; production snapshot fixtures pass through 4,097 children | Live recovery/facial behavior above 128 and repeated-switch leak/lifetime checks |
| Vistani moving sunlight reflection noise | Default-off MISC source-angle workaround installed; live 4 to 0.5357 to 4 restoration, actual UI clicks and persistence pass; cold-load ownership failure fixed and tested; sharper shadows visible; user acceptance pending | Verify maps, continuous motion and cost before public delivery; no CVar mutation; do not describe workaround as a proven engine correction |
| Local core reload temporarily loses shared animation/facial availability | Reproduced during CW checks; subsequent rescan experiment crashed in an animation subsystem; cold recovery passes, root cause unconfirmed | Diagnose class data/lifetime before further reload stress; retain crash evidence; loader activation alone does not pass this gate |
| Genessa Stray to Faithful after another save | Reported, not reproduced or diagnosed | Save/world/player-state investigation and valid-transition regression |
| Naylshotte hidden Tarstone effect | Reported with Barrage/Emberseed, not diagnosed | Identify effect owner and correct hiding/restoration without changing infusion |
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
- Finish demonstrated CW foot, belt and skirt work and other non-deferred
  defects. Preserve accepted Genessa cloth and Peony motion/design.
- Complete the goal's Phase 4 matrix: actions, preview/world, lifecycle,
  both-form doubles, customization, attachments/gameplay and existing UI fixes.
- Finish Genessa's two-variant CSS/CINE facial coverage and preserve Eve/CW.
- Finish locomotion review. All 59 additional native alternatives were authored
  and installed; validation remains. Do not redo conversion as if it were absent.
- Complete matched performance measurements, repeated-switch lifetime checks
  and investigation of any new hitch, crash or regression.
- Finish shared migration's live CSS/CINE and clean/upgrade coverage. The four
  passing offline upgrade cases do not deliver the new Genessa/CW versions.
- Prepare coordinated stable artifacts only after applicable gates pass or the
  user explicitly accepts a remaining limitation. Verify final versions,
  dependency fingerprints, source/tag/build provenance, checksums and notes.

The low-priority follow-ups remain explicitly queued, not silently fixed or
discarded. They retain their original scheduling in the full goal. This audit
does not invent new deferrals for the primary repairs or release gates.

## Immediate continuation

The older Eve saved-graph input/update check now passes all 36 cases with each
outfit's actual post-process graph, including Bikini/Knit feet graphs, and both
normal/facial meshes. See the
[regression record](../research/2026-10-12-eve-existing-motion-regression.md).
This is not full visual or cloth acceptance. Continue CW's demonstrated foot
defect from the installed four-bone ankle trial. Native rig reference replacement
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
