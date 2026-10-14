# Goal: Eve, Unholy Genessa and Commander White rig and physics repair

**October 15, Peony secondary customization verified:** the explicit secondary
Stray actor preserves all six ghost families, 58 source parameters, two morphs
and seven hidden sections. Both clones clean up, and saved settings, CINE,
appearance, exact player/view and prior window focus restore. Independent
verification rejects incorrect actor/component controls. No core or assets
changed. [Evidence and limits](../research/2026-10-15-outfit-doubles.md#peony-secondary-ghost-materials).

**October 15, all six older Eve Stray pairs verified:** Black Pearl, Planet
Diving Suit, Skin Suit, Bikini, Casual Knitwear and Midsummer Alice now pass both
clones, mesh matching, finite sampled poses and complete two-group cleanup.
All six restore Peony, full saved CSS state, CINE and player/view. Two diagnostic
restoration failures were retained and corrected: unavailable saved idle choices
are preserved, and CSSX request clients now serialize their shared channel.
Four new concurrency/failure tests and 15 existing tooling tests pass. Core
content and cooked assets are unchanged. Stray mesh-copy coverage now includes
all ten appearances; remaining visual/material and Faithful UG/CW checks stay
open. [Evidence](../research/2026-10-15-outfit-doubles.md#six-older-eve-secondary-clone-checks).

**October 15, Peony and both UG secondary clones verified:** three normal
Stray Use CSS summons pass on `4a4402a210f4d1e8`, each observing primary and
secondary with matching source meshes and complete two-group cleanup. Each
case restores CW, saved customization/idle, full CSS state, CINE and exact
player/view geometry. The complete sequence also passes an independent
state/geometry comparison. No core or cooked assets changed. Visual
cloth/contact, secondary material parameters and remaining outfit/form cases
stay open. [Evidence](../research/2026-10-15-outfit-doubles.md#peony-and-both-genessa-variants-completed-secondary-clone-checks).

**October 15, native revival restriction explained:** the upgraded save's active
Alteration blocks Faithful through `State.ReviveBlock`. Core `4a4402a210f4d1e8`
now reports this immediately; the live blocked request preserves form, resources,
appearance, saved settings and view. The requested optional confirmation-based
override is queued below, with autosave impact to investigate before promising
no save changes. [Diagnosis and verification](genessa-form-save-switch.md).
The configured native session-save class does serialize the current Darkform
tag. A nonpersistent CSS option alone cannot meet that promise; save scheduling,
loading and scoped restoration require investigation. No save or ability state
was modified by the [override research](../research/2026-10-15-genessa-override.md).
Further cooked readback identifies the loader: the player controller consumes
the saved form flag only when the game mode permits session resume. Resume also
gates saved Resolve. Its current live getter returns false; other loads remain
unverified. Native save scheduling and a persistence-preserving override are
still under investigation. The retained `native-save1` whole-executable job
timed out; a focused no-autoanalysis fallback completed six native decompilations
and corroborated the static branch findings without touching the game.
Independent static disassembly now links SaveSession, SaveToDisk and AutoSave
to a common routine. Live reflection confirms GetSessionSave returns the runtime
session object, and exposes an outdated SaveToDisk signature in the older native
header dump. The exact serialization boundary and lifecycle remain unverified;
no override or persistence hook is installed.

**October 15, latest requested shot confirmed:** Peony fires Naylshotte with a
100-to-72 Resolve drop; stowing hides its permanent Burn effect. Current view,
settings and input restore. Earlier attempts with the temporary `eve` idle and
Casual Sweater registered the button without observed expenditure; isolate that
idle/outfit interaction in the remaining gameplay checks. See the
[shot record](../research/2026-10-15-sidearm-aim.md#user-requested-repeat-shot).
The wider Faithful doubles checks now pass all six older Eve outfits, including
Casual Knitwear and Midsummer Alice. Both interrupted parent receipts remain.
The upgraded save confirms both Stray clones in Default mode with CW selected.
Use CSS also passes on core `4a4402a210f4d1e8`: both clones copy CW's mesh,
two groups clean up, and the parent restores settings, CINE, Iris Out and exact
player/view geometry. These checks do not establish visual cloth/contact. See
[the outfit doubles evidence](../research/2026-10-15-outfit-doubles.md).

**October 15, Peony double customization verified in both forms:** Faithful
and Stray primary each match six ghost-material families, 58 explicit source
parameters, two nonzero morphs and seven hidden sections. The focused captures
pass after retaining a failed overlong read. Settings, CINE and player/view
restore. [Evidence and remaining scope](../research/2026-10-15-peony-doubles.md#current-customization-follow-up).
The original save's secondary eligibility getter returned false. The later
upgraded save returns true and CW's Default/Use CSS two-clone checks pass.
Secondary-copy coverage for other outfits and visual/contact checks remain.

**October 15, Peony normal doubles checked in four form/mode cases:** Faithful
and Stray primary select the native mesh in Default and Peony V2 in Use CSS.
CSS group cleanup, body-input copying and Stray ghost-template bindings pass.
The parent position guard stops after cumulative ability displacement; separate
guarded recovery restores the original position, facing and saved state.
[Evidence and limits](../research/2026-10-15-peony-doubles.md).
Secondary Stray, Faithful ghost materials, visual cloth/contact, customization
combinations and the other outfits remain open. No game assets or core changed.

**October 15, twenty actual aim-interruption cases pass:** Iris Out and Come
Closer release facial playback across all seven Eve outfits, both Unholy Genessa
variants and Commander White. Drawn/stowed Burn visibility, infusion, health,
Resolve and input cleanup pass. Peony, saved settings and the CINE model restore;
player/view restoration passes with only actor-rotation roundoff tolerance.
[Evidence, failed preflights and scope](../research/2026-10-15-outfit-aim-interruption.md).
This covers aiming in Faithful form, not the remaining attack/dodge/interaction,
Stray/doubles, contact or performance gates. The user-requested repeat normal shot
also passes the corrected firing fixture.

**October 15, Naylshotte actual draw/fire/stow verified:** Ctrl aiming exposes
Naylshotte and its permanent Burn effect; a primary shot produces a 28-point
Resolve drop; stowing hides both weapon and attached effect. Infusion, saved
settings and CINE restore. Player position/view restore exactly and actor yaw
within floating-point roundoff. Original failed fixtures and separate final
verification are retained. [Evidence and limits](../research/2026-10-15-sidearm-aim.md).
Damage, Barrage, effect replacement and the original owner-drift trigger remain
open. No production binary or asset changed.

**October 15, native CINE end and explicit exit pass:** original Genessa plays
the full Iris Out take through the correct native montage and also releases it
on an early CINE exit. Peony, saved CSS/CINE settings and exact player/view
restore. [Evidence and remaining scope](../research/2026-10-15-native-cine.md).

**October 15, all 60 native idle bindings pass live:** original Genessa lists
every choice and selects the intended native sequence/carrier. All 240 carrier
samples match, and 1,080 sampled bone positions are finite. Peony, full saved
state, CINE and exact player/view transforms restore. [Evidence and limits](../research/2026-10-15-native-idle-library.md).
The three gait choices, native CINE, visual/contact and gameplay-interruption
checks remain distinct. No assets or installed binaries changed.

**October 15, Naylshotte Burn visibility mechanism verified:** controlled
mesh-only hiding leaves the orange Burn effect visible; the installed owner
visibility repair removes it while preserving the active infusion. Front/rear
six-phase runs pass 36 rendering observations, and settings, CINE, appearance,
weapon flags and exact player/view transforms restore.
[Evidence and scope](../research/2026-10-15-sidearm-effect-visibility.md).
The original gameplay trigger and effect replacement remain open; actual
aim/fire/stow now passes in the follow-up above.

**October 15, hidden item actor drift repaired and installed:** the old core
fails controlled actor-only visibility changes on Naylshotte, Infinite Seal and
Genessa's catalyst. The candidate repairs all three, preserves Default mode and
passes the natural form/profile sequence plus exact world visibility teardown.
Saved settings, appearance, position/view and temporary-profile cleanup pass.
[Evidence and timing limits](../research/2026-10-15-item-visibility-drift.md).
The six short timing captures do not prove FPS non-regression. Visible Tarstone
residue, aiming/firing and the broader gameplay/performance gates remain open.

**October 15, mirrored profile restoration installed:** the failing Darkform
selection regression is repaired. Profiles restore their saved selection table
and use normal mirror reconciliation, preserving independent form choices.
Twenty-six host suites and archived-profile checks pass. The second live fixture
passes repeated Stray loads, return to Faithful, exact settings/CINE restoration
and guarded restoration of displacement during the form round trip. The first
fixture's position failure remains recorded. [Evidence and scope](../research/2026-10-15-profile-forms.md).
The cross-save form report, doubles and broader gameplay/release matrix remain open.
The subsequent visibility regression finds actor-only hidden-flag drift on three
items after the form checks. Meshes remain hidden, but enforcement does not repair
an owner actor whose hidden flag was cleared. Repair and retest this lifecycle
gap next; a connection to the Tarstone report is not yet proven.

**October 15, unavailable-profile preflight installed:** missing outfits, removed
variants and shell incompatibility now reject before changing profile settings.
The failing regression passes, along with 26 host suites, 12 archived profiles,
four real catalog combinations and the Windows build. Guarded installation and
repeated live Default-profile loads preserve the user's complete saved state,
visibility, CINE model and player/view. [Evidence and limits](../research/2026-10-15-profile-dependencies.md).
Missing-package/reinstall rejection is verified offline, not through live package
removal. Mirrored profile-slot ownership, broader save/form lifecycle and stable
release gates remain open.

**October 15, paired body-cost fixture completes:** 48 enabled/bypassed cases
cover normal/facial Peony at 30/60/120 updates per second, with 28,800 measured
updates and verified 120 Hz solver counters. Six existing input-handoff cases
also pass (3,366 frames). [Evidence and timing limits](../research/2026-10-15-body-cost.md).
Paired median added time is 0.21-0.59 ms in the editor fixture, with substantial
variation. This does not close runtime FPS, cloth/GPU, other rigs or the full
release performance gate. No game assets/settings changed; temporary build
intermediates and DDC removed after successful verification.

**October 14, live attachment check above 128 passes:** Peony completes three
facial activation/restoration cycles with 129 direct children and 166 owned
components. All 73 temporary empty components are removed, returning the exact
original 56 children/93 components, parents and sockets. Saved settings, CINE,
appearance and player/camera state restore. [Evidence and scope](../research/2026-10-14-attachment-limit-live.md).
This closes the live snapshot-cap check, not high-count rendered-accessory cost,
follower/offset stress, broader lifetime or the full stable-release matrix.

**October 14, profile visibility fix installed and verified live:** guarded
stationary core reload passes. Actual Default-profile loads restore sidearm/seal
visibility in the world and wardrobe preview, including repeated loads. Original
settings, appearance, CINE model, player/view and post-menu world visibility
restore; menu closed and controls unblocked. Two inactive world actor flag
differences inside the menu are retained in the evidence, with exact world flags
verified after close. [Evidence](../research/2026-10-14-profile-default-restore.md).
No outfit asset changes. Broader migration, lifecycle and release gates remain.

**October 14, Default visibility profile defect corrected offline:** restoring
an explicitly empty MISC snapshot now clears later visibility overrides, while
legacy profiles without MISC retain current settings. The failing reproduction
now passes, as do 26 host suites, 12 profile restore cases across three archived
states, four package catalog cases and the Windows core build.
[Evidence](../research/2026-10-14-profile-default-restore.md).
Core-only candidate retained, not installed. Live profile application, broader
save/CINE migration and the full stable-release matrix remain open.

**October 14, stationary Peony timing reference recorded:** three 15-second
captures contain 3,133 verified records with equal appearance/view boundaries
and unchanged saved settings, process and core. Interval medians are
12.23-13.12 ms; measured CSS tick medians are 0.153-0.174 ms. Tail variation is
substantial. The game exposes no channels through its trace-library interface.
[Evidence and measurement limits](../research/2026-10-14-peony-stationary-cost.md).
This is not a matched regression pass or isolated rig/cloth/GPU timing.
Movement, transitions, frame-rate coverage and full stable-release gates remain.

**October 14, repeated outfit/facial lifecycle check passes:** all ten scoped
appearances complete three rounds (30 cases, 90 snapshots), retaining the same
93 owned components and 56 direct mesh attachments. Facial mesh/adapter
activation and restoration pass; full CSS state, CINE model and player/camera
state restore exactly. [Evidence](../research/2026-10-14-outfit-switch-lifecycle.md).
Both Genessa variants also retain genuine CINE appearance-interruption reporting.
This does not close live counts above 128, long-session leaks, action coverage,
world/save/form transitions, doubles, rendering or frame-time gates.

**October 14, CINE natural-end defect repaired and verified live:** normal
face-mesh cleanup no longer relabels a completed clip as interrupted. Full Iris
Out photo takes finish on both Genessa variants, with exact player/view and
saved CSS/CINE restoration. The new development core is installed; rollback
is retained. [Evidence](../research/2026-10-14-cine-natural-end.md).
Six close-ups show facial poses and visible eyes, but harsh shadow patches
remain. Gameplay, both-form doubles, broader lifecycle and release gates stay open.

**October 14, both Genessa variants pass live facial-track checks:** Iris Out
and Come Closer match all 115 controls at three sampled poses each (1,380
weight comparisons), with synchronized body/face clocks and exact restoration
on explicit stop. Peony, full CSS saved state and the CINE model restore unchanged.
[Evidence](../research/2026-10-14-genessa-live-face-tracks.md).
Actual CINE takes, close-up visual checks, gameplay interruption, both forms,
doubles and remaining lifecycle/performance gates are still open.

**October 14, CW permanent walking paths resolve live:** the already-loaded
final BlendSpace has 36 matching samples referencing all eight accepted final
animation paths. The facial performance adapter and one Belt20 rig are active;
shape inputs agree. Peony and the entire saved CSS state restore exactly.
[Evidence](../research/2026-10-14-cw-release-live-bindings.md).
No movement or CINE control was sent. Terrain/lifecycle/performance gates remain.

**October 14, repaired Black Pearl and Planet live bindings pass:** both facial
meshes resolve the shared animation adapter and accepted tighter body rig in
CINE. Temporary clothing/variant changes restored Peony and the entire saved
CSS state exactly. Shaded screenshots show the clothing, but do not settle the
moving sunlight artifact or full motion/lifecycle checks.
[Evidence](../research/2026-10-14-eve-live-variant-capture.md).

**October 14, current coordinated upgrade combinations pass offline:** the
new Eve/shared pair passes import and production catalog checks with current
Genessa/CW, either legacy outfit, and both legacy outfits. All four cases have
zero import failures; current packages have zero overlaps. The legacy pair's
one shared skeleton has identical export-chunk hashes. Facial-match counts are
10/8/9/7 as expected. [Evidence](../research/2026-10-14-coordinated-upgrade-audit.md).
No game changes or release promotion. Save migration, live CSS/CINE, rendering,
performance and coordinated stable version/package gates remain open.

**October 14, supporting Eve rig provenance verified:** original and migrated
sources, successful reload/cook receipts and eight supporting payloads match for
hair, hand and both feet graphs. The current writer reproduces both preserved
secondary-graph payloads exactly, with unchanged/inverse header checks passing.
[Evidence](../research/2026-10-14-eve-motion-preservation.md#supporting-cook-sources-and-compiled-graph-replay).
Only two duplicate replay files were removed (113,796 logical bytes). No game
changes. Peony remains selected; the repaired older-outfit visual check and
full lifecycle/performance/stable-release gates remain open.

**October 14, first live capture after launch:** the game is running with
Peony selected. Her mesh, post-process owner and one body rig resolve correctly;
public motion inputs and accepted tighter limits match. The accepted CW core is
mapped in the process. Sunlight shadows are at Game Default, with zero modified
lights. [Live capture](../research/2026-10-14-eve-motion-preservation.md#first-live-capture-after-launch).
No character, camera or settings changes were sent. Black Pearl/Planet visual
checking is pending; this does not close the older-outfit shading or full release gates.

**October 14, accepted Eve motion preserved in the shading candidate:** all
six body/hair/hand/secondary/feet packages retain their accepted twelve payloads.
The tighter body source, reload, Windows cook and decoded limits match; retained
supporting sources match the earlier 36-case preset fixture. Current installed
Eve hashes match the verified candidate. [Evidence](../research/2026-10-14-eve-motion-preservation.md).
No assets changed. The user asked about launching and was told the installed
candidate is ready for a sunlight/shade check; no result has been received yet.
Full source-rebuild, gameplay, performance and stable-release gates remain open.

**October 14, combined Eve repair candidate installed:** the game was closed
throughout installation and independent readback. Eve's trio and Shared Assets
metadata now match the verified candidate. Previous trios are retained outside
`Paks` for rollback. The accepted CW walking core, CW/Genessa assets, CSS/CSSX
saved state and CINE files are unchanged; no duplicate or retired providers were
found. [Installation receipt](../research/2026-10-14-eve-surface-tangents.md#installed-candidate).
The game remains closed. No launch requested; this is an installed local trial,
not live acceptance or the stable public release. Full regression gates remain.

**October 14, mesh/material source chain verified:** all twelve repaired final
editor meshes match their successful cook inputs and combined shipping payloads.
The 273 older material/texture packages remain exact. Missing original material
extraction inputs were recovered from the hash-matching rollback trio; the
current writer reproduces all 745 migrated files exactly. Only 514,512 bytes of
unique headers remain, with duplicate payloads linked to accepted sources.
[Source-chain evidence](../research/2026-10-14-eve-source-package-chain.md).
No game files changed. Continue offline; full release and live gates remain.

**October 14, customization and install preflight pass:** all twelve repaired
Eve meshes preserve accepted material-slot identities and visible garment
coverage. All shape controls resolve; palettes/defaults are unchanged. The
checker rejects 48 deliberately invalid targets/data changes. The prepared
Eve/shared installation uses the accepted CW core and protects CW/Genessa and
saved state; 24 transaction tests pass, including partial-install rollback.
[Evidence and install recipe](../research/2026-10-14-eve-surface-tangents.md#customization-preservation-and-installation-plan).
Nothing installed. The user still prefers offline work; no launch request.
Full gameplay, performance, upgrade, source and stable-release gates remain.

**October 14, combined Eve package verified offline:** all twelve repaired
meshes preserve their verified geometry, facial bindings and cloth/configs in
one candidate. Catalog checks pass 1,545; dependency checks pass 1,008 packages
and 272 references. Exactly 24 mesh payloads change and 1,087 remain unchanged,
including accepted Peony content. Current shared metadata retains CW's accepted
walking changes. [Combined evidence](../research/2026-10-14-eve-surface-tangents.md#combined-eve-package).
Nothing installed and no launch requested. Full runtime, performance, upgrade
and stable-release gates remain open. Duplicate staging cleanup reports root
90G and theark 20G free before and after; sources and evidence are retained.

**October 14, Alice permanent paths verified offline:** the last older Eve
regular/facial pair now passes reload, Windows cook and independent exact
readback at its release paths, preserving the repaired shading, cloth/configs,
bindings and 22/137 morphs. No diagnostic dependencies remain.
See [Alice final paths](../research/2026-10-14-eve-surface-tangents.md#permanent-alice-paths).
All twelve older Eve mesh repairs are ready for combined packaging with the
current shared metadata. No installation or launch request. The full goal's
integration, gameplay, performance and release gates remain open. Final cleanup
immediately reports root/theark 92G/4.7G free; new cook scratch is on root.

**October 14, Alice private pair verified offline:** ordered source, import,
fresh reload, tangent repair, cloth transfer and paired Windows cook/readback
pass. Normal/facial bindings, 22/137 morphs, original simulation/configs and all
1,224 garment triangles are preserved. Both meshes share matching render/cloth
mapping digests. The 284 known collapsed-UV fallbacks remain.
See [Alice evidence](../research/2026-10-14-eve-surface-tangents.md#alice-cloth-repair).
New cook scratch is on root through scoped links, keeping the relocated work
path intact. Cleanup immediately reports root/theark 92G/4.9G free.
No installation or launch request. Alice's permanent paths,
combined Eve integration and all full-goal live/performance/release gates remain.

**October 14, Knitwear permanent paths verified offline:**
both regular/facial meshes now use their existing release paths. Fresh reload,
Windows cook and independent IoStore readback preserve the verified private
pair's geometry, shading, cloth/configs, bindings and 22/137 morphs exactly.
The reader excludes old Eve mesh providers; no diagnostic dependencies remain.
See [permanent Knitwear evidence](../research/2026-10-14-eve-surface-tangents.md#permanent-knitwear-paths).
No installation or launch request. Alice is the last older Eve pair needing
this repair, followed by combined packaging and the full live/performance/
release gates. Prelisted cleanup immediately reports root/theark 94G/5.4G free.
Check available space before Alice's cook; retain sources and the canonical cache.

**October 14, Knitwear private cloth pair verified offline:**
the regular/facial pair now passes source, tangent, cloth transfer, fresh
reload, Windows cook and independent readback. Original simulation/settings,
38,792 garment triangles, bindings and 22/137 morphs are preserved. The pair
shares matching render/mapping digests. See [Knitwear evidence](../research/2026-10-14-eve-surface-tangents.md#knitwear-cloth-repair).
The 284 known collapsed-UV fallbacks remain. No installation or launch request.
Next: Knitwear permanent paths, Alice, combined Eve packaging and the full
runtime/performance/release gates. Prelisted duplicate cleanup immediately
reports root/theark 94G/5.3G free. Six identical old cook caches now retain their
paths through links to the canonical Knitwear cache; keep that cache.

**October 14, Planet Diving permanent paths verified offline:**
both regular/facial meshes now use their existing release paths. Creation,
fresh reload, Windows cook and independent IoStore readback preserve the
verified private pair's geometry, shading, cloth/configs, bindings and 22/137
morphs. No diagnostic dependencies remain; the reader excludes the old Eve
mesh providers. See [permanent Planet evidence](../research/2026-10-14-eve-surface-tangents.md#permanent-planet-diving-paths).
No installation. Knitwear/Alice repairs, combined Eve packaging and all full-goal
runtime/performance/release gates remain. Continue offline. Prelisted duplicate
cleanup leaves root/theark at 94G/6.9G immediately afterward.

**October 14, Planet Diving private cloth pair verified offline:**
the reordered normal/facial pair now passes tangent repair, cloth transfer,
fresh reload, Windows cook and independent readback. Original simulation,
cloth settings, geometry, bindings and 22/137 morphs are preserved; both meshes
share matching render/mapping digests. The first transfer failed because a
rebuild ran before persistent section data was written; using UE's editor
sequence fixes that failure. See [Planet evidence](../research/2026-10-14-eve-surface-tangents.md#planet-diving-cloth-transfer).
The 294 known collapsed-UV fallbacks remain a shading limitation. No installation.
Continue offline: Planet permanent paths, Knitwear/Alice, combined Eve packaging
and all full-goal runtime/performance/release gates remain. Prelisted duplicate
cleanup leaves root/theark at 94G/7.0G free.

**October 14, Skin Suit/Bikini permanent paths verified offline:** the four
regular/facial meshes now use their existing release paths. Fresh reload,
Windows cook, IoStore roundtrip and independent decoded comparison preserve
the verified private repairs exactly, including tangent basis, 22/137 morphs,
bindings and original bounds. No diagnostic dependencies remain. The reader
excludes the old Eve mesh providers. See
[final-path evidence](../research/2026-10-14-eve-surface-tangents.md#permanent-skin-suit-and-bikini-paths).
The 284 existing collapsed-UV fallbacks per mesh remain a shading limitation.
Nothing installed; continue offline. The three cloth-bearing outfit repairs,
combined packaging and full runtime/performance/release gates remain.
Cleanup leaves root/theark at 94G/8.8G free.

**October 14, Skin Suit and Bikini paired cooks checked offline:** both repaired
normal/facial assemblies preserve source geometry, weights, UVs, colors, morphs
and existing installed bindings through reload/cook/readback. The overall
quality check remains failed on one zero-length tangent per mesh. Isolated
source triangles reproduce it in UE's own MikkTSpace library, separately from
import/cooking; neither existing-corner tracing nor normal normalization clears
it. See [paired evidence](../research/2026-10-14-eve-surface-tangents.md#skin-suit-and-bikini-paired-cook).
No installation or launch request. Preserve the failed quality receipts; do not
call these pairs accepted or the lighting report fixed. Final paths, three
cloth-bearing outfit repairs and full release gates remain. Duplicate cook
cleanup leaves root/theark at 94G/11G free; current sources and payloads remain.

**October 14, Black Pearl permanent paths verified offline:** the repaired
normal/facial pair is saved and cooked at the existing Eve paths. Fresh reload,
IoStore roundtrip and independent decoded comparison preserve the verified
private pair exactly, including the tangent basis and 22/137 morphs. Neither
mesh references a diagnostic package. The first registry lookup failed after
saving; explicit rescanning, recovery and separate reload now pass, with the
failed-run evidence retained. See
[final-path evidence](../research/2026-10-14-eve-surface-tangents.md#permanent-black-pearl-paths).
No installation. Full Eve container integration, the other five older outfits,
live checks and stable-release gates remain. Continue offline as requested.
Accounted cleanup leaves root 95G/theark 13G free.

**October 14, Black Pearl bound pair verified offline:** the repaired normal
and facial meshes now pass fresh editor reload, Windows cook, two-package
IoStore roundtrip and independent geometry/surface/skin/morph checks. Their
render data matches each other; material slots, rig/Skeleton references,
non-morph cooked properties and respective bounds match the installed pair.
The normal/facial meshes retain 22/137 morphs and 150,791 vertices each. Both
have only the 488 known collapsed-UV tangent fallbacks. See
[assembly evidence](../research/2026-10-14-eve-surface-tangents.md#bound-black-pearl-normal-and-facial-pair).
No installation or game check. Continue offline: permanent-path integration,
the other five older Eve pairs, and existing live/performance/release gates
remain. Duplicate cook inputs/output and disposable Wine prefix are removed
after verification; root/theark now read 95G/13G free.

**October 14, continue offline as requested:** the user deferred the next live
CW binding check. Do not request another launch or repeat accepted walking
judgment while this preference is active. Offline Black Pearl ordered-corner
import, Windows cook, IoStore roundtrip and independent geometry/surface/skin/
morph comparisons now pass through the relocated `work` symlink. The candidate
has zero unexpected nonorthogonal frames under the established Peony check,
compared with 540,075 in the installed Black Pearl mesh. Its 488 remaining
exceptions are unit-X fallbacks on collapsed UV0 triangles, still a shading
limitation. Current Peony has 430 such exceptions and zero unexpected ones.
See [the cooked results](../research/2026-10-14-eve-surface-tangents.md#private-windows-cook-and-readback).
No game assets were installed. Full material/rig/cloth/facial assemblies and
visual/performance checks remain open. The finished cook's disposable Wine
prefix was removed after verification; sources, recipe and readbacks remain.
The work location is `/run/media/eins0fx/theark/CSS/CustomShellSystem-work`.

**October 14, older Eve tangent repair now identified:** the six original
outfits pass cooked UV/color/imported-normal correspondence, but have widespread
tangent alignment errors matching the earlier CW/Peony corner-order defect.
A private ordered Black Pearl import reloads with 150,791 render vertices
instead of 558,243, retaining 193,589 triangles, 379 bones and 22 morphs.
See [surface evidence and repair sequence](../research/2026-10-14-eve-surface-tangents.md).
The candidate is not cooked or installed. Verify its basis and preservation,
then carry the correction through normal/facial assemblies with cloth and
materials intact. Do not call this a proven fix for Vistani's sunlight report.
The game remains closed; final CW path resolution and other live checks remain
pending. No accepted installed assets or saved settings changed.

**October 14, all six original Eve part sources accounted for:** current
comparisons pass all 46 parts (803,379 points; 1,160,081 triangles), including
fresh Skin Suit checks. Black Pearl's full exporter replay is byte-identical to
its accepted import, and all six imports share that verified 379-bone payload.
Planet Diving's missing tail morphs are recovered in a separate import-derived
library, with the original source/failure preserved. See
[six-outfit source coverage](../research/2026-10-14-eve-six-source-coverage.md).
No installed files changed. The regenerated duplicate was removed after exact
comparison; root/theark settle at 95G/15G free. Full assembly/material recipes,
remaining cooked surface fields, gameplay and performance are still open.
The game is closed; a request to reopen CW for runtime checks is pending.

**October 14, three Eve editable-source chains checked:** all 21 parts of
Bikini, Knitwear and Alice now reproduce their accepted imports for geometry,
weights, morphs and surface corner data. Alice's suit triangulation/normals and
ribbon weights failed against the old libraries; two import-derived editable
parts now pass fresh reload/export comparisons, with original sources and
failures preserved. The manifest ties all parts to the unchanged current Eve
container through the earlier cooked comparisons. No game files changed.
See [source recovery](../research/2026-10-14-eve-editable-source-recovery.md).
This does not close source rig/material assembly, remaining cooked surface
fields, gameplay or performance gates. The game remains closed at this checkpoint.

**October 14, accepted walk finalized and installed:** CW's nine permanent-path
source assets are saved under `Animations/Locomotion/Walk`. Fresh reload preserves
all raw animation data, 395 reference transforms and 36 sample bindings; all
fourteen recorded directional captures match the accepted trial exactly after
path mapping. Windows cook, standalone IoStore roundtrip and independent
readback now pass, including exact compressed animation digests against the
accepted installed trial. Receipts are under CW `work/walk-final-paths1`.
Combined CW/shared assembly, 924-package/272-reference import checks, 434 native
checks and 1,406 outfit checks pass. The final-path package is installed with the
game closed, preserving the accepted motion, core, saved settings and other
outfits. Receipt: CW `work/walk-final-paths1/assembly/installed.json`; rollback:
`MortalShell2/Content/CSS-cw-walk-final-20261014`. The permanent paths still need
a runtime resolution check on the next game session. Do not ask for subjective
walking acceptance again unless behavior changes. Temporary cook/extraction
trees are removed; immediate final df reports root 92G and theark 15G available.
See CW's walking report and October 14 cleanup receipt. The full stable-release
goal remains active, including lifecycle, performance and final delivery gates.

**October 14, CW walking accepted:** in response to the installed walking-only
trial's boots-on/off check, the user says "Walking looks right now". Close the
reported obvious walking tilt at this tested scope. Jog/sprint and belt/skirt
motion were already accepted. Preserve these assets; finalize private walking
paths before stable packaging, and retain the separate terrain/lifecycle and
performance requirements.

**October 14, current facial preservation checked:** all ten pairs retain bind
references and original morphs. Eight pairs also retain exact decoded render
digests; Genessa's two intentionally extended facial meshes instead pass the
complete retained-editor versus current-cook geometry and 137-morph comparison.
Decoded clothing properties/configurations match across all sixteen clothing
assets. Expected CW optical/graph changes are accounted for. See
[facial preservation](../research/2026-10-14-facial-preservation.md). No game
files changed; runtime facial/CINE/doubles and performance gates remain open.

**October 14, six original Eve imports checked against current installation:**
Black Pearl, Planet Diving, Skin Suit, Bikini, Knitwear and Midsummer Alice all
pass LOD0 position/UV0/material-slot, topology, bind, bounded weight and positional
morph correspondence. Each retains 379 bind bones and 22 positional morphs.
This closes those import-to-cooked fields for the pinned current normal meshes;
Blender source chains, facial/surface fields and gameplay remain separate. No
assets were changed or installed. See
[comparison record](../research/2026-10-14-eve-import-cooked-provenance.md).

**October 14, installed inventory refreshed:** readback pins all ten appearances
(the original nine plus Peony), twenty normal/facial meshes and 1,021 unique
exported packages across the four installed containers. Facial-adapter source
fingerprints and mesh skeleton/physics/graph ownership pass. No duplicate
providers occur within these containers; retired Astral/Locomotion TOCs are
absent. Installed files and the selected core remained unchanged. This is an
identity audit, not new gameplay or source-provenance acceptance. See
[installed inventory](three-outfit-installed-inventory.md). CW walking still
awaits the user's response to the installed trial. Root has 97G available and
theark 15G after the audit; no full asset trees were copied.

**October 14, CW walking-only trial installed:** private eight-direction clips
and a 36-sample blend now serve CW's existing Eve walk selection. The paired
development core supports manifest aliases so both `eve` and `css.eve.walk`
resolve to the correction without duplicate choices. Existing CW binary assets,
jog/sprint, accepted belt/skirt, shared animation binaries, Eve and Genessa are
preserved. Saved state and CINE files are unchanged. Developer mode remains on.

Fresh-process directional replay and Windows cook/readback pass. A strict
compressed local-preservation check fails at small differences despite unchanged
raw non-foot keys; the separate surface audit bounds residual foot displacement
below 0.247 mm and the neutral full-mesh residual below 1.666 mm. This qualifies
the private trial, not a visible walking fix. Combined imports pass for 924
packages and 272 references; native and outfit catalog/profile checks pass.
The user has been asked to reopen and judge boots-on/off walking. Keep the foot
bug open until that response, and finalize private study paths before release
if accepted. Full stable-release scope and other outstanding gates remain active.

Receipt: CW `work/foot-walk-orientation3/assembly/installed.json`; core
`css_core-cw-walk-acab6363906f0e88.dll`; rollback
`MortalShell2/Content/CSS-cw-walk-20261014`. CW `docs/walking-foot-report.md`
records both rejected authoring/fixture checks and the successful final checks.
Accounted-for rejected captures, temporary Wine prefix, full extracted package
trees and obsolete validation caches were removed after the cleanup list was
written. The current trial/source/evidence uses 2.9G on root. `df -h` across this
work is root 99G to 97G available and theark unchanged at 15G. See CW
`docs/cleanup-2026-10-14.md` for exact deletion receipts and logical byte totals.

**October 14, private CW walking candidate:** a copied forward-walk sequence
changes only raw foot rotations and preserves the imported retarget reference
through a procedural save. Normal/facial 241-frame engine captures preserve
translations and scale; other compressed rotations differ by at most 0.00511
degrees. Fresh-process data and playback comparisons pass. The first ordinary
save cleared all 395 reference transforms and was rejected; this does not
diagnose the installed walk. No game assets changed. See CW
`docs/walking-foot-report.md` and `work/foot-walk-orientation3`. Actual compressed
surface review, directional blend integration, cook/package verification and
gameplay acceptance remain. Preserve accepted jog/sprint and belt/skirt behavior.

**October 14, CW gait feedback:** the user reports "jog and sprint are fine.
walk still obvious tilt." Walking remains an unresolved visible defect with
the installed ankle trial. Preserve accepted jog/sprint and belt/skirt physics;
investigate a walking-specific correction. Lower aggregate banking measurements
are not walking acceptance. Boots-on/off remains part of the original report.

The subsequent retained-pose component-orientation screen is mixed: maximum
bank decreases but mean and median increase. No candidate is installed from
that screen. Evidence and reproduction are in CW `docs/walking-foot-report.md`
and `work/foot-walk-orientation3/screen.json`. Next isolate the visible walking
orientation error before choosing a correction; do not repeat global IK bypass
or full-leg retargeting, both already rejected.

**October 14, CW live shape routing:** neutral, each of six individual
maximum controls, their combined maximum and restored neutral all match
across mesh morph weights, post-process inputs and Belt20 bridge inputs.
The rig is enabled, initialized and scale-supported in each of nine cases.
The initial driver's immediate saved-state check fails; a later independent
check confirms exact state and live neutral restoration. Raw failure and
follow-up receipt are retained under `cw-belt-live1/shapes`; the driver now
waits for the state write. This establishes live routing/recovery, not visual
clearance at every shape or matched performance.

**October 14, CW practical movement accepted:** after walking and turning
with the belt, then enabling the skirt and repeating, the user reports
"Both look fine in motion." This accepts the current installed belt/skirt
under the practical normal-motion threshold. Preserve the candidate and stop
skirt solver tuning. The earlier automated walking attempt remains untested;
the acceptance evidence is the user's direct game check. Shape extremes,
matched performance, facial/lifecycle regression and the other stable-release
gates remain separate requirements.

**October 14, CW live Belt20 check:** the user reopened the game with CW.
The live mesh is SK_Facial20 and its scene adapter is
ABP_FacialPerformanceBelt20. Hair, body and belt ControlRig nodes are enabled
at full alpha. Belt20 is initialized with supported scale; three sequential
readbacks advance TotalSteps from 43,612 to 44,688 with ResetCount unchanged
at 314. These property reads are not an atomic contact snapshot. One reports
four unresolved bridge entries, so this does not establish clipping-free
collision. A 12-second Iris Out recording and 24 valid final-pose snapshots
over 6.018 seconds are retained under
`work/three-outfit-rig-physics/cw-belt-live1`.

The user's coat and skirt are hidden, and all six shape inputs are zero.
No skirt gameplay acceptance follows from this capture. The walking attempt
stopped before input because the game was no longer visible to the recorder;
the subsequent visibility wait expired. No movement was sent. The first
driver's camera guard also interrupted its preference restoration; an explicit
follow-up restored Iris Out and verified the complete saved state. The retry
driver now restores that preference independently of spatial guards. Current
player is unchanged and no walk trial is active. Belt/skirt walking, body
shape checks and the full stable-release gates remain pending. No installed
files or physics settings changed.

**October 14, Tarstone live observation:** Naylshotte has Burn/OnCrit tags
and an active PS_Nailshot_Burn component owned by the hidden weapon. Actor
and both meshes remain hidden in three captures and five observations after
Iris Out releases; the effect reports not recently rendered. The visible
residue is not reproduced. Keep the bug open and avoid diagnosing it from
the component-active flag alone. See `sidearm-tarstone-visibility.md`.

**October 14, storage maintenance:** removed 1,530,592,127 bytes of generated
DDC from five terminal Genessa cooks after recording the removal list and
checking open files. Source, cooked outputs and evidence outside those caches
retain their metadata (5,582 files). Root remains 101G free; theark increases
from 14G to 15G (`df -h`). Exact inventory and result are in
`work/three-outfit-rig-physics/cook-cache-cleanup-20261014`. No installed changes.
Game remains closed, so pending gameplay checks are not marked passed.

**October 14, sidearm effect ownership:** seven shipped weapon/VFX packages
identify the persistent Naylshotte Burn system and its `SpawnedPermanent`
component lifecycle. CSS hides the actor and meshes but rechecks only the
tracked mesh flag; this is a candidate gap, not a confirmed cause. A bounded
read-only capture now follows the equipped sidearm, effect and attachment
flags. Help and the closed-game guard pass; live checks are pending. No core,
asset or installation changes. See `sidearm-tarstone-visibility.md`. CW image
approval remains provisional under the practical collision/movement target.

**October 14, native skirt contact check:** baseline42 repeats exactly with
read-only native contact capture in CW `work/skirt-contacts43`. The worst
particle switches sides of the same leg collider. A single two-substep test
in `work/skirt-substeps44/verified` reduces that witness but retains an 11.6 cm
jump at its neighbor and almost doubles measured editor cloth-step cost.
Rejected as a repair; no saved asset or installed files changed. All poses,
rig values and asset hashes are preserved. The probe now checks actual solver
substeps because UE's simulation-level statistic retains its initialization
value. Do not repeat CCD, broad collider enlargement or increasing-substep
trials; prior failures remain recorded. Current Belt20 game acceptance is
still pending, with game closed. Full stable-release gates remain active.

**October 14, lower-skirt localization:** the user's acceptance of the images
is provisional, with usable physics/body collision and no obvious movement
clipping as the target. Baseline42's worst particles are in the lower skirt,
at source heights 17.90 to 48.28 cm. The largest jump is 12.16 cm versus
0.677 cm of animated target motion; neighboring edges stretch up to 7.496
times their 1.57 to 2.45 cm rest lengths. All pinned points match independent
skinning within 0.000029 cm, supporting the coordinate/index correspondence.
The largest-jump particle is body-outside at both endpoints; that does not
establish a clear trajectory or native contact cause. Four physical-mesh/body
diagnostic renders and the hashed report are in CW baseline42 `jump-views`
and `jump-contact.json`. No mesh, solver, collider or installed files changed.
Prior C13/C15 and self-collision investigations remain authoritative: do not
restart broad parameter or custom-solver work. The Belt20 game trial remains
pending; use practical movement evidence to judge further repair needs.

**October 14, skirt baseline:** the final CW mesh/graph now has a 241-frame
native cloth capture at 60 Hz using its unchanged saved solver and collider.
It confirms 4,893 particles with active simulation. Individual particle steps
reach 12.16 cm outside clip loop boundaries, while the root remains stationary.
This is a lead for the reported jerk, not a rendered-surface diagnosis or fix.
Locate the worst particles and compare their body/contact trajectories before
changing solver settings. CW `work/skirt-baseline42` retains the capture,
recipes, source hashes and setup failures. Authoring assets are unchanged;
the existing cook project/DDC were reused and nothing new was installed.
See CW `docs/belt-and-skirt-motion.md`. Game remains closed; full live and
stable-release gates remain active. The prior crash record already exhausted
what its small dump can establish; another offline stack inspection is not
a next step without new evidence.

**October 14, CW display-name cleanup:** the installed package and outfit
catalog now use `Commander White`, and the package description no longer
calls it a development build. Only those three metadata fields changed;
asset containers, selection IDs, controls, adapters and the developer core
remain unchanged. Package/resource readback and both combined native catalog
checks pass. CW `work/display-name41/installed.json` records installation and
rollback at `MortalShell2/Content/display-name41`. Its metadata pak SHA256 is
`db6fad85b424b5fea377a1ad09b161e459a2adcf3b4426a800567285335763d4`.
The previous assembly40 receipts still describe the preceding pak. Its asset
payload checks continue to apply. Build version remains an internal trial
version until the stable release is actually prepared; historical release
artifacts were not rewritten. Current packaging recipes preserve the clean
name. Game remains closed, so CW movement/collision acceptance is pending.

**October 14, belt package integration:** the user provisionally accepts the
images, with working physics, body collision and no obvious normal-motion
clipping as the practical target. This is not gameplay acceptance. Belt20's
Normal/Facial scene adapters now pass six fresh editor captures: scene blend
starts/releases, non-belt poses and body/hair rig state match the baselines,
and test facial/body morphs apply and clear. Actual Iris Out/game input remains
untested. The final eight private roots cooked in the reused cook33 project.
Independent readback in CW `work/belt-pack36` passes geometry, all 16/206
morphs, materials, cloth data, four ankle modes and rig input bindings. The
388-bone installed CW bind prefix and its retarget/virtual definitions are
preserved, with one waist helper added.

The first merged private-path package was rejected before installation:
CSS requires scene adapter targets in `Animations/Adapters`, not `Studies`.
Its 915-package/271-reference import check passed, but catalog loading failed.
Accounted-for heavy assembly37 intermediates were removed; failure logs,
manifests and recipes remain. CW `work/belt-final38` migrated the eight
finished roots to normal asset folders in the existing cook project, without
another project copy. Save/fresh reload, final Windows cook and independent
readback now pass. CSS explicitly allows the audited final waist skeleton;
host skeleton/library tests and its developer Windows core build pass.
The final assembly40 passes combined catalog checks and is **installed** with
hash verification. Core SHA256 is
`bdcef63385cf0d28b101022cc5cc0a164f172612465cc10c4f8e689caa8e4a35`.
Rollback is `MortalShell2/Content/CSS-cw-belt-20261014`. Peony/Genessa packages,
the four ankle modes and saved settings are preserved. User has been asked to
reopen and leave CW in the world for live checks. Physics/collision, shape
restoration, scene/face behavior and performance are **not game-accepted** yet.
The full stable public release goal remains active.
See CW `docs/belt-and-skirt-motion.md` and `docs/cleanup-2026-10-12.md`.

**October 14, practical CW belt candidate:** Belt20 adds up to 1 cm of
shape-dependent clearance to the saved Belt19 contact margins, preserving
neutral poses exactly. Twelve Normal/Facial captures pass morph readback,
length/cone bounds and non-belt pose preservation. A fresh-load maximum-size
capture passes hair toggle, body epoch, reinitialization and teleport resets.
Maximum-size Sprint frame 51 now has 26.21 mm distal clearance instead of
11.96 mm penetration. The full centerline screen has one startup contact
at frame 3; mixed-size Sprint samples have contacts at 3 and 51. These are
known brief early-motion issues to judge in the practical trial, not a claim
of zero clipping. Maximum-size Jog samples clear. Keep minor cosmetic bends
deferred under the user's threshold. Six candidate assets and their dependency
closure are staged for Windows cooking in `work/belt-cook33`; no installation.
The first cook could not connect to the stopped project Docker daemon. It
was restarted using the existing wrapper, and the pinned image verified;
the retry uses the same staged files. That retry completed with zero errors;
all six expected cooked roots have matching package names and export sidecars.
Four warnings concern the placeholder optical material, which must not be
packaged. Separately, `work/belt-bind34` saves and freshly reloads Normal/Facial
meshes bound to Belt20, preserving bones, morphs, materials and cloth. These
bindings are newer than cook33. Next integrate the scene adapters and bound
meshes into the same cook staging area, independently read the cooked assets,
update the private catalog/core trial and check it in game. Preserve the ankle
trial and accepted Peony. CW `work/belt-padding32/REPRODUCE.md` records evidence
and remaining integration. Full stable release remains active.

**October 14, CW morphed contacts:** private Belt19 connects all six body
coefficients to ten contact envelopes and 24 waistband support points.
Twelve fresh-load Normal/Facial captures cover neutral, mixed and maximum
body sizes in Jog/Sprint, with 4,332 frames checking actual mesh morphs and
rig inputs. Length/cone bounds pass and contact modes do not oscillate.
Neutral poses remain identical to Belt18. Maximum-size Jog surface samples
clear the body, but maximum-size Sprint frame 51 still has 11.96 mm distal
penetration. Fix that functional contact before the practical trial; minor
bends remain deferred. Image approval is provisional, not gameplay approval.
No cook or installation. All 162 authoring assets are hash-verified. Corrected
captures replace twelve rejected captures whose legacy diagnostic pulse
overrode mesh morph weights. Recipes and limits: CW
`work/belt-shape31/REPRODUCE.md`. Full stable release remains active.

**October 14, CW body-input wiring:** CSS now carries the six model-ordered
body coefficients through geometry capture/apply/restore, exposing optional
`CSSBodyShape0` through `CSSBodyShape5` floats. Missing all six preserves
older AnimBPs; partial or mistyped inputs are rejected. Windows core builds
and 128 geometry fixtures pass. Private CW Normal/Facial AnimBPs route the
inputs into Belt18 and use the existing body reset epoch. Four 361-frame
Jog90 captures confirm evaluated input values, exact neutral-pose preservation
and no changes outside belt bones. This is editor propagation evidence;
mesh morphs and fallback envelopes were neutral in those captures. No install.
The ten fallback-envelope center/basis morph deltas now match a direct fit
in 128 corner/interior cases. Next connect those data to the rig, account
for waistband support shape, verify actual morphed moving surfaces and live
CSS restoration, then prepare the practical game trial. Sources, receipts
and limits: CW `work/belt-shape30/REPRODUCE.md`. Full stable release stays
active, including all other unresolved bug and gameplay gates.

**October 14, stable combined CW contacts:** private Belt18 preserves the
clear Jog bridge and uses the earlier physical contact solve when the bridge
has no feasible path. A 0.125-second hold removes repeated mode switching
in Sprint. Jog120 matches Belt16 and Sprint60 matches Belt15 exactly. All
sampled front-chain points clear the actual body over full Sprint60/90
captures; selected full-garment checks remove the previous large distal
penetrations. Small waistband overlap and sharp bends are deferred under
the user's practical threshold. Six native Sprint views were reviewed.
Fresh-load Normal/Facial Jog90 comparisons preserve all non-belt bones;
length/cone and clock/reset checks pass, including fractional display steps.
Next connect the six live CSS body coefficients to the AnimBP/rig and adapt
both the bridge and ten fallback contact shapes. Then check representative
sizes and motion transitions, package the practical trial and verify game
motion/cost. No cook or install. CW `work/belt-hybrid29/REPRODUCE.md` retains
recipes and limitations; the full stable release and other bug gates remain
active. Do not reopen minor cosmetic refinement as an independent blocker.

**October 14, first native CW collision bridge:** private Belt16 now executes
in Unreal using built-in RigVM units. Jog120 (481 frames) and Jog60 (241)
clear the measured lower-body envelope, and two whole-garment pose checks
have no inside vertices. Six native-pose views were reviewed; cosmetic bends
remain deferred. Sprint exposes a functional failure: the conservative hull
can cover the root or leave no valid link path, and the current fallback
penetrates the distal mesh by 34.79 mm at frame 0 and 19.93 mm at frame 24.
Do not install this candidate. Preserve the jog improvement while repairing
that fallback/contact route, then wire the six live body coefficients and
validate the usable result. Fresh reload/reset checks pass, non-belt bones
and the previous rear-belt poses are unchanged. Editor cost is measured but
game performance remains open. All 150 prior assets are preserved plus two
private outputs. No cook or install. CW's `work/belt-bridge28/REPRODUCE.md`
records recipes, limits and failures. The stable public release and all
remaining CW skirt/feet, Peony/regression and bug-report gates stay active.

**October 14, CW collision bridge:** `work/belt-collision27` completes 240
moving Jog steps outside a conservative lower-body hull, avoiding the closing
leg gap. The old step-204 distal overlap of 14.47 mm becomes 21.04 mm clearance.
The saved waist helper clears the remaining waist overlap in all five checked
reference pose/shape cases. A lighter 98-direction separating-plane method
also clears the sampled trajectory and three checked neutral mesh poses.
Nine steps exceed a diagnostic travel guard; this is recorded for continuous
motion review, not treated as an automatic visual failure under the user's
practical acceptance direction. A 451-point runtime support candidate matches
full-surface plane extrema in 1,446 retained gait/shape cases, but coverage of
unseen motions and mixed body values remains to be checked. Next wire the
collision data and live body values into the private rig, preserve the waist
helper and reset behavior, then prepare a usable game trial. All 150
protected/private assets are unchanged. No cook or installation; the complete
stable release, CW rear/skirt/feet and other bug reports remain active.

**October 14, user acceptance clarification:** prioritize a usable outfit with
working physics, proper body collision and no obvious clipping in movement.
Perfect bends, smooth weighting and minor cosmetic distortion are not release
blockers. Earlier acceptance of tiny, unobtrusive clipping still applies.
Stop expanding cosmetic strain studies unless the distortion breaks the
outfit visibly or prevents collision from working. The current CW priority is
the demonstrated belt/leg penetration in Jog, followed by a practical in-game
trial. Do not misreport offline support checks as a fixed gameplay collision.

The user provisionally considers the subsequent `belt-collision27` preview
images acceptable ("that should be fine i think"), while explicitly noting
they are judging images. Preserve this as the visual target and defer minor
bends. It is not in-game motion or collision acceptance. The conservative
lower-body hull trial clears 240 captured jog steps and keeps the distal belt
outside the body in three whole-vertex spot checks. Porting that result into
an affordable native rig, retaining the authored waist helper, is still open.

**October 14, CW sequential waist-contact checkpoint:** `work/belt-root26`
completes 241 captured sprint frames at each of neutral and maximum body size.
The refined 49-point support meets the 0.5 mm contact margin within the existing
5 cm root bound. All ten selected full-waist audits and four distal audits
clear the actual posed body. This is an offline translation correction, not
an integrated runtime solver or gameplay acceptance. Whole-belt strain checks
expose a preexisting 45.95 mm edge extension at Sprint frame 35. The adjacent
F05/F06 skinning rotations differ by 114.69 degrees while their joint remains
connected to within 4e-7 cm. The sharp weight transition amplifies that bend;
no repair is yet validated. Six final front/side views still show the pendant
distortion. Next constrain and validate lower-belt deformation together with
contact before native integration. No cook or install; accepted Peony and the
full stable-release scope remain active. See CW's motion document and
`work/belt-root26/REPRODUCE.md` for evidence and limitations.

**October 14, CW authored-waist checkpoint:** `work/belt-waist25` authors the
separate waist helper in private Normal/Facial meshes, skeleton and Belt15 rig.
Original geometry, morphs, cloth, raw/virtual binds and animation mapping are
preserved. Fresh reload, walking/reset checks and all six Walk/Jog/Sprint
constraint comparisons pass. All 397 original bones match the preceding paired
captures exactly. Actual editor render buffers verify the intended weighting;
eight pose/shape surface comparisons and four reviewed sprint images confirm
the helper implementation. Editor/cooked morph differences reproduce the
recorded compression precision exactly, not a new morph defect. Actual sprint
waist penetration still reaches 3.775 mm, and sharp pendant bends remain.
Next integrate dynamic root contact and repair the hanging belt's path before
the legs close. Do not treat editor constraint checks as contact, performance
or game acceptance. No new cook or installation. Accepted Peony, CW skirt/feet,
reported bugs and the complete stable public release remain active. CW's
motion document and `work/belt-waist25/REPRODUCE.md` retain the evidence.

**October 14, CW separate-waist checkpoint:** `work/belt-waist24` rejects the
previous static-boundary transfer for 18.06 to 18.52 mm sprint thigh penetration.
Keeping a separate animated waistband rotation improves fit without rotating
the pendant, and slightly lowers maximum attachment strain over 1,446 captured
gait/body-shape cases. A frozen correction of the six front-bone translations
clears all measured waist and distal vertices in four sprint/body-size cases
within the existing 5 cm root limit. The weakest clearance is only 0.021 mm;
this is not a dynamic contact fix. Twenty-four views were reviewed. An
unapplied `CSS_Belt_Waist` binding plan reproduces eight full-belt skinning
comparisons, with geometry and helper-remapped weight amounts preserved.
Next author and validate the private waist helper, then integrate dynamic
contact and repair the pendant route before the closing leg gap. The earlier
Jog failure remains. All 142 protected assets are unchanged, no new cook or
installation, accepted Peony and the entire stable-release goal remain active.
CW's motion document and `work/belt-waist24/REPRODUCE.md` retain the evidence.

**October 14, CW actual-surface and waist checkpoint:** `work/belt-surface23`
identifies the earlier weight transition as the cause of two waist witnesses
moving inside the body. Preserving static weights alone restores a severe edge
stretch, so that attempt is rejected. A transition from the static boundary,
with original non-front mass preserved, reduces worst attachment-edge extension
to 1.161 mm over all 481 captured Jog poses, versus 46.820 mm for the cap alone.
All 8,295 measured waist vertices clear the two diagnostic body poses; six final
views were reviewed and the complete weight plan is ready for further tests.
Actual distal clipping remains, including 4.132 mm at replay step 2 and 2.725 mm
at the quarter of step 5. No moving trajectory or game acceptance follows.
Next check sprint/enlarged-body waist witnesses and repair the distal route
before the closing leg gap. All 142 protected assets are unchanged, no new
rig/cook/install, accepted Peony and the entire stable release remain active.
The CW motion document records the rejected weight alternatives, fixed tiny-
influence amplification, full-strain audit, recipes and source snapshots.

**October 14, CW substep/contact-branch checkpoint:** `work/belt-continuation22`
tests four smaller intervals with eight validated exterior meshes. Both equal
movement allocation and a flexible 5 cm per-node path budget fail the first
interval. A seeded retry exposes a nondifferentiable nearest-surface switch.
Separate triangle constraints verify a local pinch at one point on link 5;
the certificate uses only four contact faces, not angle or movement bounds.
The best feature-restoration pose misses the centerline margin by 1.313 mm,
but actual garment fit at that intermediate pose is not measured. Next inspect
the actual belt and its approach to the closing gap before the fourth endpoint,
then test an exterior route before it becomes pinched. Do not lower a proxy
margin merely to pass; small clipping can be deferred only when garment views
show it is inconspicuous. No new rig/cook/install, all 142 protected assets
unchanged, and every accepted result and full stable-release requirement stays
active. Source/evidence and cleanup receipts are retained in CW. Root 112G and
theark 848M were available after cleanup; other sessions' files were untouched.

**October 14, CW contact continuity checkpoint:** `work/belt-active21` clears
the fourth-step distal belt with a diagnostic removal of only the fifth cone.
The waist still has a visible gap, and the fresh replay fails at step five.
The next exterior-reference experiment exposed invalid artificial hip caps;
using the connected body repairs the diagnostic surface at all eight sampled
poses, but still does not solve step five. The preceding chain is independently
clear, then the moving body enters it by 2.743 cm over one 1/120-second step.
Next test bounded continuation through intermediate body poses with the
corrected exterior boundary and the actual piecewise belt path. Retain
original contact/length/motion requirements and distinguish the fifth-cone
diagnostic from original-limit behavior. No new rig, cook or install. All 142
protected assets remain unchanged; accepted Peony, waist/body shapes, rear
chains, CW skirt/feet, reported bugs and the full stable release remain active.
The CW motion document records the rejected trials and exact source snapshots.
Space at cleanup: root 113G free, theark 6.3G free. This small diagnostic is on
root; other sessions' outputs are unknown, not touched.

**October 14, CW separate-body and bounded-motion tests:** the offline contact
model now distinguishes each leg from internal surfaces of the other leg.
This changes twelve initial search directions at the fifth-step failure, but
does not repair the fourth-step failure. An angular reference keeps all link
lengths exact. One bounded output reduces measured actual front-belt
penetration from 14.595 to 5.338 mm, with two reviewed renders, but still fails
contact. Combining both methods also fails the requested motion bound. No
failed state is propagated or accepted, and no new rig/cook/install follows.
Next inspect separate region inequalities near the preceding valid pose,
keeping original clearance/angle limits and explicit motion rejection.
CW `work/belt-partition20` and `docs/belt-and-skirt-motion.md` retain the
evidence. All 142 protected assets are unchanged. Accepted Peony, CW skirt,
feet, waist/body shapes, outstanding bug reports and the entire coordinated
stable-release scope remain active.

**October 14, CW offline contact-sign repair:** the actual-body reference's
nearest-normal rule falsely labels points inside one leg as outside when the
other leg's surface is nearer. Reusing the earlier Eve solid-angle method
repairs that demonstrated classification error. An overlap regression and
28 saved search-path checks pass. The corrected solver still fails the motion
and contact requirements; no belt candidate is accepted or installed. Keep
separate leg contact information and exclude internal inter-leg surfaces as
escape boundaries in the next contact experiment. CW `work/belt-search19` and
`docs/belt-and-skirt-motion.md` record the correction and failed replays. This
is a diagnostic-tool fix, not a game-engine fix. Accepted Peony and every full
coordinated stable-release requirement remain active.

**October 14, CW calf and actual-body contact:** rigid proximal calf proxies
miss blended knee deformation by up to 5 cm. Banded proxies, smaller steps and
velocity changes remain rejected. A new actual-body reference independently
confirms penetration between the four-sample reference's contact points. Denser contact
and sampled paths still fail; removing angular bounds helps one step, then
produces extreme movement at the next failed solve. This is not an installed
repair or proof that the garment cannot fit. Inspect that local solve's active
contacts/search trajectory before another rig candidate. Evidence is in CW
`work/belt-calf17`, `work/belt-body18` and `docs/belt-and-skirt-motion.md`.
Four new full-belt views were reviewed, analytical checks/default regressions
pass, and all 142 protected inputs are unchanged. No new rig/cook/install.
Accepted Peony and the entire stable-release scope remain active, including
waist/body shapes, rear chains, skirt, feet and outstanding bug reports.

**October 14, CW swept-contact findings:** endpoint and sampled-segment sweeps
remain rejected. Solving static lengths, cones and contacts together clears
240 Jog steps but still jumps by up to 16.1 cm per step. A separate linear
relaxation proves that the first failed entry-plane set conflicts with the
length/cone bounds; more iterations cannot satisfy that set. Direct swept paths
also fail the tested solve. Posed-body comparisons identify both real calf
intersections and excess collision-proxy volume. Next inspect the implicated
right-calf supports against those witnesses, preserving the separate waist,
body-shape, skirt and foot requirements. Evidence is in CW
`work/belt-swept16` and `docs/belt-and-skirt-motion.md`. All 142 pinned inputs
remain unchanged. No new rig, cook or install; accepted Peony and the full
coordinated stable-release scope remain active.

**October 13, CW contact continuity:** developer-only internal traces at
60/120 Hz reproduce the prior Belt14 poses and preserve all 142 pinned inputs.
The calf penetration occurs at the solver step rate. Additional iterations,
larger limits and coupled contact trials do not repair the trajectory. A
rest-direction restart clears the tested centerlines but still jumps, so it is
not accepted or installed. Next test swept contact and retention of the
approach side, including moving colliders and later chain constraints. Evidence
and rejected approaches are in CW `work/belt-direction15` and
`docs/belt-and-skirt-motion.md`. Keep all waist/body-shape, skirt, foot, bug-report
and coordinated stable-release requirements active. No game assets changed.

**October 13, CW distal contact:** the full Belt13 screen finishes 1,452 posed
surface cases and identifies visible jog/sprint calf intersections. Belt14's
60-degree front limit clears the tested sprint contact, but jogging still has
14.5 mm overlap and the known waist overlaps persist. Normal/facial motion and
reset checks, six paired gait checks and fresh graph reload pass, with all 137
protected inputs unchanged. It is not accepted, cooked or installed. Direction
sampling finds a clearer jog direction inside the existing cone; next diagnose
contact selection/propagation and continuity rather than raising the limit
again. The blended-waist experiments and rejected envelope fits are recorded in
CW `docs/belt-and-skirt-motion.md`, `work/belt-blended14` and
`work/belt-contact14`. Preserve the installed ankle trial, accepted Peony and
the entire coordinated stable-release scope. No required defect is waived.

**October 13, CW moving front attachment:** Belt13 now integrates the repaired
waist meshes with bounded front-root contact before chain solving. Normal and
facial walking/reset checks and all six paired gait checks pass; previous
Belt12 checks reproduce exactly. Four sprint renders show the buckle outside
the raised thigh. Full-surface checks still find 5.07 mm neutral and 10.86 mm
enlarged-body overlap in Sprint 24. Those sprint witnesses have only 52-72%
front-root weight and are excluded by the initial greater-than-75% contact
footprint. Next account for the blended waist weights and retest the same
witnesses, then broader motion, shape, lifecycle and cost. Observed added
editor cost is 3.15-4.73 ms, not game-FPS acceptance. No belt cook or install.
Evidence: CW `work/belt-contact13` and `docs/belt-and-skirt-motion.md`.
All coordinated stable-release requirements remain active.

**October 13, CW waist-weight repair:** private normal/facial mesh stages and
fresh-process reloads pass. A gradual transition replaces the abrupt
pelvis/spine-to-front-bone weight boundary on 4,454 source points; geometry,
morphs, other slots, cloth and 135 protected source files remain unchanged.
Offline root translation now avoids the earlier severe waist-edge stretch.
Four neutral poses and eight reviewed views do not establish full acceptance:
with all six body controls at one, Sprint 24 still has 7.92 mm front overlap.
No belt mesh or rig is cooked or installed. Next integrate moving-root contact
into the chain solve using the saved meshes, then test the recorded failures
and broader shape/motion/lifecycle coverage. Evidence: CW
`work/belt-attachment2`, including `stage-report.json`, `fresh-reload.json` and
`transition16-maxbody.json`, and `docs/belt-and-skirt-motion.md`. The complete
stable release and all outstanding bug reports remain active.

**October 13, CW segment contact and attachment:** Belt12 fixes an identified
endpoint-only collision miss near the calf. Its sampled Jog 60 front overlap is
now 0.71 mm at maximum body shape. Six paired normal/facial gait captures pass
constraints, but sprint still has roughly 3 cm front intersections and about
1 cm enlarged-shape rear intersections. Four diagnostic renders were reviewed;
the upper front attachment still crosses the raised thigh. Nearest-skin donor
following does not fix that attachment. A topology audit confirms its root also
affects the waist band, requiring attachment-aware repair. No new belt asset is
cooked or installed. Preserve the ankle trial, accepted Peony and full stable
release scope. Evidence: CW `work/belt-contact11`, `belt-contact12`,
`belt-attachment1` and `docs/belt-and-skirt-motion.md`.

An offline bounded root-translation test clears the two measured sprint front
witnesses but stretches an actual waist-band edge from 1.312 to 5.734 cm.
That shortcut is rejected, with front/side renders and edge measurements saved
in CW `work/belt-attachment1/root-translation-*`. Repair the attachment's
deformation before integrating root contact movement. No runtime or install
change follows from that counterfactual.

**October 13, CW body-shape and gait surface check:** cooked morph positions
and triangle indices match the retained skin audit exactly. All 64 body-control
corners plus a midpoint preserve the minor walking overlap at frame 84, at most
1.765 mm. Full 60 Hz walk/jog/sprint surface reconstruction at minimum/maximum
controls exposes larger jog/sprint intersections, so Belt9 is not ready to
install. Sprint's front witness reaches 4.359 cm; enlarged-shape rear witnesses
also fail. Open-boundary false signs are separated using ray witnesses and
visual review. The front root's locked rotation contributes to the upper-belt
failure; Belt10 now implements that pivot. Normal/facial walking/lifecycle and
six paired gait captures pass constraints and preserve all bone positions.
The remaining sprint anchor is itself inside the raised thigh, so pivot alone
does not close contact. Continue with attachment/contact repair and enlarged-shape
rear coverage before cooking. Evidence:
CW `work/belt-morph1` and `docs/belt-and-skirt-motion.md`. No new installed asset,
no completion claim, and the full stable-release scope remains active.

**October 13, CW belt CPU work reduced:** Belt9 adds conservative collider
early-outs, caches transformed contact data per substep and stops insignificant
contact iterations. Both normal/facial rigs pass all 14 paired cases across
30/60/120 Hz walking and 60 Hz jog/sprint in a fresh editor process. Its reviewed
walking surface differs from Belt6 by at most 0.16 mm. Repeated 60 Hz walking
median added CPU cost is lower overall; system-load variation remains in the
raw reports. No in-game FPS acceptance or install is claimed. Evidence: CW
`work/belt-motion1`, `belt-motion2`, `belt-convergence1` and `belt-contact9`.
Continue with dynamic body-shape/contact coverage, wider posed surface checks
and the cook/game gates. Full release scope, skirt repair and other reported
bugs remain active.

**October 13, CW belt contact candidate 6:** measured hip/leg envelopes and
mesh-length rear endpoints remove the observed intersections in the four
reviewed walking poses. Normal/facial 241-frame motion and reset checks pass,
including terminal rotation limits; both saved graphs reload with identical
hashes/properties and the 135 original inputs remain unchanged. The full
241-frame walking surface comparison in CW `work/belt-contact6` reduces the
front intersection from 5.139 cm to 1.634 mm; both rear chains remain clear.
The worst-pose renders show no obvious detached or disappearing belt. Retain
the minor front clipping under the user's stated tolerance. This is an editor
candidate, not a cooked or installed fix. Morph coverage, broader motion,
performance, gameplay and the separate skirt defect remain open. Preserve the
installed ankle trial and the full stable-release scope. See the
[belt contact record](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/belt-and-skirt-motion.md#measured-contacts-and-terminal-ornaments).

**October 13, CW bounded belt candidate:** normal/facial walking and lifecycle
replays pass attachment, output isolation and reset checks using the existing
Eve fixed-step chain implementation. Maximum segment-length error is below
0.000001 cm, and no other local bone changes. Saved graphs reload correctly.
Surface review still shows belt/body clipping, so this candidate is not
installed or accepted. Next add measured body contacts to this bounded rig;
do not resume the rejected native-chain or historical cloth-solver approaches.
Evidence: CW `work/belt-cloth2` and the
[belt/skirt record](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/belt-and-skirt-motion.md).
The game process exited before the final read-only probe; no new live-health
claim is made. Saved state is unchanged, no trial installed this turn. Root and
theark remain at 118G and 36G available. Full stable-release scope remains active.

**October 13, CW belt isolated:** cooked weights and retained walking poses
show 18 undriven belt-chain bones. They affect only belt slot 14. A private
native AnimDynamics prototype changes only its 15 intended descendants but
loses attachment and diverges in the 241-frame replay, so it is rejected and
not installed. Sources/evidence retained, 526 MB of accounted-for intermediates
removed. Next reuse the bounded existing Eve/Genessa chain implementation.
Skirt config is recorded but unchanged and its jerk is not diagnosed. See
[belt/skirt record](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/belt-and-skirt-motion.md).

**October 13, actual CW baseline compared:** original packages were cold-loaded
and tested in booted/bare straight walks, then the ankle trial restored.
Sampled mean bank is lower with the trial, consistent with the matched editor
study. Original packages also show boot penetration and bare-foot clearance;
those are pre-existing contact limitations. Separate phases prevent claiming
planted-contact improvement. Final health passes in process 3263279, with full
saved state and camera/facing restored. Normal Continue leaves a 9.255 cm position
difference. Original rollback retained, redundant comparison hardlinks removed.
Evidence: `cw-baseline1`, CW `work/foot-baseline1` and the
[actual baseline record](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/straight-walk-contact.md#actual-original-package-comparison).
The full stable-release goal remains active.

**October 13, straight walking and bare-foot contact:** pre-orienting the
stationary pawn removes the initial turn from bounded booted/bare passes.
Each records eight straight moving samples over 1.9 seconds. Both return safely;
final position is 0.072 cm from initial preflight, facing/camera and complete
saved state restored. Native IK, facial playback and Reduced noise remain active.
Contact remains open: boot landmarks reach -2.484 cm, bare landmarks stay at
least +3.091 cm. Separate phases and neutral geometry limit that comparison.
Do not apply a blanket grounding offset. Evidence: `cw-cycle1` and CW
`work/foot-cycle1`, [straight walk record](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/straight-walk-contact.md).

**October 13, full retained gait comparison:** the ankle trial lowers mean and
maximum reference bank across all six normal/facial walk, jog and sprint editor
captures. Some phases worsen; sprint median increases. Worst-regression boot
and bare-foot renders show no obvious new collapse in the reviewed views.
This preserves the trial for further testing without closing the full-cycle,
terrain or gameplay acceptance gates. No runtime or asset changes in this study.
Evidence: CW `work/foot-gaits1` and [gait regression](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/ankle-gait-regression.md).

**October 13, CW contact measurements:** four recorded straight-walk poses now
have cooked boot reconstruction and collision readback. The trial reduces mean
absolute reference banking from 2.978 to 1.422 degrees compared with undoing its
ankle rotation at fixed captured parent/IK poses. That comparison is not a live
baseline run. The worst sampled forefoot point is 2.244 cm below collision;
the counterfactual is 2.136 cm below. Keep the trial and leave grounding
unchanged. Full-cycle, bare-foot, slope and other gait acceptance remain open.
Both readbacks preserve the game and saved settings. Evidence: CW
`work/foot-contact1` and [contact readback](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/foot-contact-readback.md).

**October 13, bounded gait follow-up:** CW's installed ankle trial now has live
walk, jog and sprint activation evidence. A developer input defect prevented
Left Shift from reaching the game; explicit scan-code injection fixes it.
The short sprint captures still include turning, so straight-cycle/contact
acceptance remains open. A native sprint camera turn triggered the return guard;
the captured orientation and surveyed position were restored separately.
Final follow-up position is within 9.23 cm of the survey origin, with keys
released and full saved settings restored. Evidence: `cw-gait1` under the
three-outfit work directory and the [native rig record](../research/2026-10-13-cw-native-foot-rig.md).
The development core now starts `ccf2940b2158a109`; asset trial and full stable
release scope remain unchanged. No new foot-fix acceptance or crash closure.

**October 13, CW reference-alignment candidate:** four matched editor timelines
and cooked-surface reconstruction reject full-leg retargeting. The narrower
foot/ball AnimationRelative candidate reduces mean absolute reference banking
from 3.783 to 2.427 degrees across the walk capture; selected boot and bare-foot
renders support a game trial. This is not floor-contact or gameplay acceptance.
Private save, fresh-process reload, Windows cook and full package readback pass.
Only four skeleton modes change; other outfit payloads are preserved. Combined
checks cover 907 packages, 271 references and 1,822 animation-library assertions.
The trial is installed, with rollback in game Content
`css-cw-ankle-trial-20261013`. Live readback confirms all four modes, and short
booted/bare-foot walking passes complete with keys, boots, Iris Out, facial
playback and full saved state restored. Native IK and Reduced noise remain
active. Evidence: CW `work/foot-retarget2/live-ankle1`, `work/ankle-cook1` and
`work/ankle-pack1`. This is an improvement trial, not final acceptance of all
gaits/slopes. Preserve the full stable-release scope and separate reload-crash
gate. See CW `docs/walking-foot-report.md`.

**October 13, walking comparison completed:** bounded autonomous walking now
captures pose and controls in the same callback, with automatic key release.
The mesh-reference trial does not show a walking improvement. A temporary native
rig bypass removes the local leg-rotation difference, but the phase-matched
component-space check shows that the active rig already preserves foot rotation
within 0.0021 degrees of the imported animation. It changes foot position instead.
Do not mistake compensating local knee/ankle rotations for the user's sideways
tilt or ship a blanket IK bypass. Next compare animation-to-body reference
alignment and the actual boot/bare-foot surfaces at matched phases. Captures,
clips and corrected diagnosis are in the [native rig record](../research/2026-10-13-cw-native-foot-rig.md),
`cw-walk-reference1`, `cw-walk-bypass2` and `cw-rig-solver1`. All overrides are
off, keys released, Iris Out/facial playback and complete saved state restored.
Reduced noise remains active. Current core SHA-256 starts `7a764e26757d2a6d`.
CW feet, reload-crash and full stable-release gates remain open.

**October 13, native rig reference trial:** fresh native AnimInstance/ControlRig
recreation now restores all 13 checked initial transforms exactly. The temporary
mesh-reference flag imports CW's audited bind transforms within 1.61e-6 cm and
1.71e-6 degrees. Explicit restoration and a separate automatic timeout both
pass, with the selected Iris Out, facial playback and complete CSS state restored.
The stationary pose differences overlap ordinary idle variation, so this is
not an accepted walking repair. The override is inactive. Next compare actual
walking with the verified restoration path. Six final health checks retain all
four selected animations and active Reduced noise; no new crash occurred.
See the [native rig record](../research/2026-10-13-cw-native-foot-rig.md) and
`work/three-outfit-rig-physics/cw-rig-reference1`. Stable-release and crash gates
remain open.

**October 13, reload crash and recovery:** a subsequent rescan experiment
ended in an animation subsystem access violation. The exact PDB/minidump points
to missing constant class data as a hypothesis, not a confirmed root cause.
Normal Continue recovered the existing save in a fresh process; both live
animation classes have sparse data, all four selected locomotion assets are
available, and CSS state matches the backup exactly. Three normal idle-transition cycles and one fresh-process catalog rescan
also pass class-state checks. Further DLL reload stress is held while this
defect is investigated. See the
[crash record](../research/2026-10-13-cw-reload-crash.md). A later cold-installed
class inventory adds component/class addresses for dump correlation. Two
instrumented rescans also pass, including CW facial playback and all four
selected animation choices. This is diagnostic coverage, not a crash fix. Three controlled DLL reloads,
six subsequent rescans and three explicit collection requests also pass 51
class/availability checkpoints with exact CSS state restoration. The crash gate
remains open; resume the native foot-rig reference comparison from its recorded
restoration plan.
Cold loading also exposed a sunlight ownership bug: Reduced noise stayed selected
but the game restored its four-degree preset, leaving the workaround inactive.
The recovery candidate recognizes a settled game preset and preserves unrelated
custom values. Portable tests, the Windows build, cold loading and live
Default/Reduced restoration pass. A simulated game-preset reset recovers in
about four seconds. Reduced stays enabled; CSS state is preserved exactly.
See the [load-recovery record](../research/2026-10-13-sunlight-load-recovery.md).

**October 13, overnight checks:** real MISC mouse clicks pass for Default,
Reduced noise, then Default/Reduced restoration and persistence. Reduced noise
is left selected. The truncated choice label is shortened to "Reduced noise";
the description still explains sharper shadows. Screenshots and click results:
`work/three-outfit-rig-physics/sunlight-misc-ui1`. The CW full-body-rig multiplier
control now passes explicit and timed restoration, but does not establish a
walking fix. A separate repeated-reload asset-resolution failure was found and
is being investigated before further rig changes. Two later controlled reloads
restore all selected animations and facial playback, but do not establish a fix
for that intermittent failure. The final 60-sample extended check also passes;
the temporary rig override is inactive. See the
[native rig and reload record](../research/2026-10-13-cw-native-foot-rig.md).

**Overnight authorization, October 13:** the user is leaving the game open
and explicitly authorizes independent fixing, improvement and testing while
away. Preserve their save/settings, use established reload limits, and continue
the full stable-release goal without waiting for routine manual checks.

**October 13, CW foot localization:** the actual shared walk blend now has
six editor-component timelines (1,446 poses), with all installed sample paths,
coordinates and rates checked. Forward-only baseline matches the prior lower
body exactly. Small directional blends do not explain all live8 differences.
Read-only graph inspection establishes that CTRL_Human_FullbodyIK remains
active despite zero correction inputs. Its initial reference differs from CW's
mesh, including about 15.94 cm at the foot IK helpers; its "Set Initial
Transforms From Mesh" flag is false. Next test this reference mapping with
animation synchronization and full restoration, not an arbitrary foot rotation.
No CW repair trial is installed yet. Evidence: CSS
`work/three-outfit-rig-physics/cw-foot-blend1` and CW `work/foot-blend1`;
CW commit `2ca79c2`. The extra live9 capture contains jog starts/stops, not a
steady walking test. Regenerable probe compiler/DDC files were removed after
the completed capture; source, built helper, commands and results remain.

**October 13, sunlight MISC trial installed:** the development core
`eb558e626f2979a7c64e50992dd3254bd632fd2e7e661889d19e9a3023f3d3a6`
adds MISC > Lighting > Sunlight shadows with Default (game) and Reduced noise
(sharper shadows). New installations retain game lighting. The optional mode
uses the same 0.5357-degree source angle as the retained phase-8 trial, with no
CVar writes. Live readback and saved-state checks pass: 4 to 0.5357 to 4 degrees.
Portable ownership/restoration tests and the Windows development build pass.
The controlled core reload preserves outfit/customization and restores Iris Out.
Receipt: `work/three-outfit-rig-physics/sunlight-misc1/`.
After the user reported no effect, the live setting read Default, which the
validation had restored. Reduced noise was then enabled and left enabled for
the user; the follow-up screenshot shows cleaner, sharper shadow regions.
Do not mark the reported rendering bug fixed or user-accepted. UI click-through
now passes as recorded above. Cross-map behavior, continuous motion and
performance remain release checks.

Prepared 2026-10-08. **Status: active, baseline and migration audit in progress.** See [implementation status](three-outfit-rig-physics-status.md) for current evidence, completed work and remaining checks.

**Release correction, October 12:** the user reaffirmed the agreed stable
milestone: **CSS v1.0.0, Eve v1.5.0, Unholy Genessa v0.3.0 and Commander White
v0.1.0**, plus the matching separate shared-assets package. Deferring Slipstream
did not defer the other outfits, reported bugs or release gates. The earlier
Peony-only beta.12 delivery claim was incorrect. Its local archives are withdrawn
from public delivery and retained as packaging evidence only. Nothing was
uploaded or installed from those archives. See the
[release and bug audit](three-outfit-release-gates.md) for outstanding work.

Continue the full goal and finish the repairs and required regression checks
before preparing the coordinated stable release. Preserve the accepted
`peony-v2-motion-package1`, including fit, four palettes, removable pieces,
flowing trim, facial playback, doubles and corrected motion. Slipstream remains
explicitly deferred; retain its sources and evidence without further experiments.

**Local developer mode, October 13:** the user reaffirmed that CSS must remain
in developer mode during this work. `tools/css.py` now explicitly builds with
`CSS_INVENTORY_DEV=ON`; the public release builder retains its explicit OFF.
The development core `2028dcd15e7669a8de1ff3bebe6381b9668d7ead58e3427b7b7672ec3783d9f3`
is installed and live CSS player/mesh probes pass. Controlled stationary reload
passed loader, process and actual DLL mapping checks. Iris Out was temporarily
replaced with the original idle, then restored; saved state matches exactly.
Receipt: `work/three-outfit-rig-physics/css-development-20261013/result.json`.
Keep local diagnostics enabled in subsequent trial installations and verify the
chosen bridge before asking the user for a timed capture. This does not extend
reload safety to active performances or mounted asset packages.

The [public upgrade audit](../research/2026-10-12-peony-public-upgrade.md) now
checks the final Eve/shared files alone and alongside the released Genessa
v0.2.0 and CW v0.0.6-dev. All four cooked-import and native-catalog cases pass.
Old outfit revisions do not gain the newer trial facial/playback adapters;
the rehearsal instructions state that limit. This is useful upgrade evidence,
not a substitute for delivering Genessa v0.3.0 and CW v0.1.0 or completing their
live regression matrix.

The [older Eve preset regression](../research/2026-10-12-eve-existing-motion-regression.md)
passes 36 saved-mesh/graph cases across the six original outfits, normal/facial
meshes and 30/60/120 fps. All 20,196 frames preserve preset inputs, single
animation updates and the expected solver/reset timing with the accepted tighter
rig. This is an offline input/update check, not visual motion, cloth, gameplay
or performance acceptance. CW's October 13 read-only capture now has 14 valid
poses with native foot-correction/blend values, including ten steady-walk samples.
Both named correction inputs stay zero while the shared Eve walk blendspace is
active. Saved state is unchanged. Continue the pose/blendspace comparison;
this is diagnostic evidence, not an accepted foot repair. See CW's
`docs/walking-foot-report.md` and `work/walking-feet-live8/controls-summary.json`.

**October 12 continuation:** Peony motion remains user accepted. The user
initially reported the blocky sunlight symptom only in Vistani's report.
On October 13 the user also reports intermittent broken shadows on this setup,
resembling that report. Keep the high-priority investigation queued after the
active CW foot work. The subsequent user screenshot and live CW comparison
reproduce it: directional virtual-shadow softening is implicated by a restored
4-to-0-to-4 ray-count test. Cause and acceptable workaround remain under
investigation; all temporary renderer settings were restored.
The user requests delivery as a MISC bug-fix control and asks whether it should
default on. Target a verified correction that can default on and be disabled.
A provisional workaround that hardens shadows or raises rendering cost should
default off and disclose that tradeoff. This default policy is the current
recommendation, not proof that an acceptable correction exists yet. The new
original-Genessa comparison also shows speckled shadows with CSS still active;
native-model evidence does not establish a completely unmodded-game cause.
Preserve the user's video settings and restore console-variable priority as well
as values when disabling a future fix. See the sunlight record for the additional
sampling controls and restoration limitation of console-based diagnostics.
Slipstream's private continuous-cloth isolation is recorded in
[constraint results](../research/2026-10-12-slipstream-constraint-isolation.md).
Its normal configuration still fails the stretch screen. Disabling backstops
or self-collision clears that screen in two runs, but does not establish fit or
justify removing collision from the shipped outfit. No Slipstream trial is installed.

The subsequent matched contact replay rules out removing backstops: penetration
worsens despite the strain pass. A private radius-only change from 3 to 16 cm
preserves geometry, weights, morphs and mapping and visibly improves rear skirt
coverage. It still fails strain in 52/145 frames in both captures, with remaining
front-leg contact. Doubling iterations does not resolve the failure. Continue
from these contact and constraint witnesses, retaining both collision systems;
do not ship the radius trial as a completed repair. Evidence and small staging
recipes are in `work/three-outfit-rig-physics/slipstream-backstop16` and the
constraint results linked above. The installed, accepted Peony package is unchanged.

The full replacement of backstop normals is also rejected after two captures.
Ordered constraint checks at frames 38 and 119 show different local conflicts.
The next private candidate, `slipstream-splitfans2`, repairs the original proxy's
49 invalid vertex fans while preserving its cutouts and retained triangle
geometry. Only three tiny unanchored simulation triangles are omitted; the
visible outfit is unchanged. All 18 components have native pins. See the same
constraint record for evidence, recipes and the following native checks.

The split-fan binding now saves/reloads successfully with unchanged visible
geometry, weights and morphs. Its automatic mapping remains inaccurate; an
offline coupled correction resolves all 703 identified rows within the existing
1 mm rest tolerance, but is not yet a saved/cooked correction. Two captured
sprints still fail strain and show substantial hip penetration in mapped
renders. Radius 16 cm on the same repaired topology worsens the strain screen
to 81/145 frames in both runs and does not resolve contact. These are rejected
repair candidates, not installation candidates. Continue from the captured
collision witnesses, including render vertices 353/434 that map directly onto
particles 153/200, rather than repeating global density or travel sweeps.
The startup crash preceding the first binding was in Unreal's trace worker;
the verified retry disables threaded tracing only. See the constraint record.

At the user's request, the shared `ue-modding` skill has also been revised to
cover the actual extraction, patched CUE4Parse, Blender, editor, cook and runtime
workflows. Unsupported generalizations about editor/source availability, reload,
hook cost and physics were replaced with scoped checks. This documentation
correction does not complete or replace the outfit goal.

October 12 authorization: improve CSS core where needed to support the new
outfits. Peony remains the current priority. The first addition is authored
material settings in palette templates, including textures, while preserving
manual customization, saved profiles and Original restoration. Keep optional
glowing, flowing trim in the Peony integration and test the resulting behavior
in both preview and gameplay before accepting it.

**Latest acceptance, October 12:** the user answers "Motion looks right now"
after testing Peony's Firm/Natural chest and glute motion while walking and
playing Iris Out. The installed `peony-v2-motion-package1` is the accepted Peony
motion baseline. A subsequent read-only live probe verifies all seven restored
angle and travel limits; `live-accepted.json` records the answer and rig state.
Keep this candidate's authored/cooked rig for subsequent builds. Do not rebuild
from the older wide-limit rig without applying the retained limit-restoration
recipe. Existing Eve variants' motion regression and the full lifecycle and
performance matrix remain open. The following paragraphs retain the sequence
of earlier findings; their pending Peony motion states are superseded here.

The combined V2 package now passes native catalog and asset-reference checks.
CSS also accepts explicitly declared facial meshes anywhere within the
author/project namespace, so outfit-local meshes do not require a `Faces`
folder. Source-revision and runtime mesh/rig checks remain. The private candidate
was `work/three-outfit-rig-physics/peony-v2-package2`. Its successor,
`peony-v2-package3`, adds the six verified V2 ghost material parents and is now
installed with the matching core. Installation hashes and rollback are recorded
in `deployment.json`. Validate the new outfit's native fit, controls, flowing
trim, facial playback, doubles and gameplay. The user now accepts the obvious
fit in CSS and while walking in the world: "Fit looks right in both."
The user also accepts flowing trim in CSS/world ("Works in both") and Iris Out
face/eyes, interruption and current-form doubles/outfit/glow ("All work").
Their subsequent lighting report was investigated without replacing the accepted
materials. On reopening the same build, the user reports "so far now its fine".
Keep the current lighting appearance for now and resume the remaining Peony
checks. This is provisional user acceptance, not proof of a lighting repair or
every environment. Retain the earlier observations for later polish if they
recur. See [lighting investigation](../research/2026-10-12-peony-v2-lighting.md).
The remaining native behavior and regression checks are open; the full goal
remains active. See [gameplay evidence](../research/2026-10-12-peony-v2-gameplay.md).

The user also accepts four palette templates, removable clothing and restoration
of colors, visible pieces and glow after saving/changing/reloading a profile:
"Everything restores correctly." Their subsequent motion report is now the
priority: body motion still jiggles wildly after selecting Firm or Natural and
appears possibly duplicated. Investigate preset propagation, actual active
animation/physics owners and solver response. Do not treat material/profile
acceptance as acceptance of body motion or assume duplication without evidence.
The user declined the requested Firm game capture and directed comparison with
the previous Eve and Unholy Genessa work first. Do not repeat that request.
The offline comparison now covers installed metadata, retained cooked rig
defaults and 27 native editor response cases. Current Eve/Genessa defaults
match; the older validated V42 rig has tighter movement limits. These limits
reduce Natural's chest response in the fixture but do not explain the reported
Firm failure. Continue tracing preset/animation integration before changing
the installed rig. See the gameplay record for measurements and limitations.
The next offline check passes on the saved Peony normal/facial meshes through
their actual secondary AnimBP: 3,366 frames at 30/60/120 fps preserve Firm,
Natural and disabled regional inputs, with one graph update per frame and the
expected 120 Hz solver steps. This narrows the remaining investigation to CSS
application/lifecycle, playback adapters and actual motion inputs. It does not
reproduce or accept the reported gameplay defect.
An isolated native replacement check now demonstrates an old physics snapshot
overwriting a newer Firm preset during restoration. The core candidate refreshes
that snapshot after playback deferral checks, while keeping original settings
for failed initialization. It builds and passes existing portable regression
tests, but is not installed. Reproduce the actual playback/UI sequence before
calling this the cause of the user's full motion defect. See `peony-v2-body-restore1`.

The user subsequently authorized game checks and automated diagnostics, and
requested loader hot reload. The restoration candidate is now installed:
`21f761aa35fd18619172db3e4b3b88318a42681bdcdac848412f45c3299acc37`.
`peony-v2-body-live2` records Firm, Natural and disabled chest/glute settings
reaching both public inputs and the live body rig on the regular secondary
graph and the Iris Out adapter (18 samples total). Original customization and
idle were restored. No gameplay/camera input was sent. This verifies settings
propagation, not visual acceptance of the reported jiggle. A stationary hot
reload of the same candidate passed loader acknowledgment, actual old-module
unmapping/new-module mapping and exact customization restoration in the same
game process. The loader's stale contract was repaired against the verified
installed beta.11 release and matching source hashes. No loader DLL was changed.
Keep the earlier animation-worker crash as an unresolved historical limitation;
this single controlled reload does not prove every combat/playback reload safe.
The visual Firm test is awaiting the user's walking assessment.
`peony-v2-body-live2/visual-firm.json` stores the original customization,
verified temporary values and exact restoration readback. The original settings
were restored before the temporary persistence deadline while the answer was
pending. Firm is no longer applied by this diagnostic.
The offline `peony-v2-frame-variation1` check passes 24 cases across current/V42
rigs, three motion states and four frame-time patterns. Uneven frames stay
stable; both rigs reset only on initialization or the deliberately long frames.
Those resets can snap the simulated offset back to the incoming pose. This
explains a possible hitch symptom, not the reported gameplay cause. No tuning
change follows from the synthetic result; visual motion acceptance remains open.
The retained default idle, Iris Out and Black Hole source animations also have
constant local chest, glute, thigh-helper and belly poses across all 1,813 keys.
Their pelvis poses vary, confirming that the evaluator did sample motion.
`peony-v2-animation-tracks1` rules out changing local helper tracks in those
three sources, not every gameplay animation or parent-bone movement.
The subsequent actual-pose test changes the diagnosis: Iris Out drives Firm
to about 20 degrees chest and 18 degrees glutes in the current rig, versus
about 4/8 degrees in V42. Changing only the current rig's angle/travel limits
reproduces the older response. `peony-v2-clip-response1` verifies 18 cases with
all 379 rig bones mapped, one initialization reset and zero disabled output.
The isolated Eve limit-restoration candidate in `peony-v2-body-limits1` now
passes save, fresh reload/recompile and six native animation/preset cases.
Every output frame matches the limit-only intervention within 0.000027 degrees;
the rig graph and all defaults except `Limits`/`MaxTravel` are unchanged.
The Windows cook now exits 0 with zero warnings/errors. The rebuilt candidate
is `peony-v2-motion-package1`: independent cooked readback finds exactly 14
numeric changes (the two seven-element arrays), with identical bytecode,
hierarchy and remaining defaults. The other 1,109 Eve payloads and Eve catalog
are unchanged. Shared asset containers are unchanged; only 10 Eve adapter
fingerprints in shared metadata are refreshed. All 1,535 catalog checks,
package imports and payload round trips pass. Rollback and a four-file install
plan are ready at `/run/media/eins0fx/theark/CSS/peony-before-body-limits1` and
the candidate's `installation-plan.json`. After the user confirmed closure,
the process check found no game process and the four-file trial was installed.
`deployment.json` and `installed-verified.json` record independent matching
installed hashes, verified rollback and unchanged shared asset containers.
The production motion-restoration core remains `21f761aa35fd1861...`.
The user reopened and accepted Firm/Natural while walking and playing Iris Out:
"Motion looks right now." Live readback confirms the restored limits. The earlier
simple-translation result did not cover this input.

The subsequent saved-setting check passes 774 restoration cases and 256 old
garment-toggle combinations against the installed V2 metadata. Compatible
settings survive, retired palettes fall back to Original, and obsolete controls
are dropped. Evidence is in `work/three-outfit-rig-physics/peony-v2-upgrade1`.
This does not replace the remaining native appearance and gameplay checks.

### High priority after Eve: moving sunlight reflection noise

October 13 update: the user has also seen intermittent broken shadows resembling
this report locally and supplied a clear CW screenshot. The local baseline and
six temporary renderer comparisons are retained in the sunlight evidence record.
Directional virtual-shadow softening is implicated; global reflections/contact
shadow/cache toggles did not clear the pattern. All original values were restored.
The successful diagnostic produces harder shadows and is not a completed fix.

Online workaround review: the matched-camera adaptive-ray repeat and the
backface-culling trial both leave visible noise. Their numerical settings are
restored; console priority still needs a restart as documented in the research
record. The user's Prographers article and Ultra Dynamic Sky listing have been
read and compared against Epic documentation and local 5.6.1 source. The article's
local-light sampling commands do not target the directional sunlight control;
the Fab asset is a sky/weather system, with no demonstrated fix for this case.
Next candidates include material-specific subsurface shadow softening and
resolution/temporal history, with quality and performance measured before a
shipping toggle. A hard-shadow workaround would remain default off. No production
renderer fix is installed or accepted.

Further October 13 controls: subsurface minimum shadow angle 5 to 0 does not
clear the noise. CW still has visible skin mottling with the default game idle
and CSS performance playback inactive; original Genessa is visually cleaner in
that idle. The completed comparisons restore CW, Iris Out and byte-identical
saved state. This narrows the investigation beyond Iris Out alone, but remains
a CSS-enabled, non-matched-motion comparison. See the sunlight record for the
diagnostic's corrected deferred-save check and the Shipping visualization limit.

The active game lighting manager's sun angle is 4 degrees. A temporary reflected
setter trial at 0.5357 degrees substantially reduces visible speckling while
sharpening foliage shadows; restoring 4 brings dense speckling back. Camera,
player, process and customization are preserved. Record this as a possible
opt-in workaround with scene-wide shadow-softness changes, not an accepted
default-on correction. Evidence: `sunlight-noise1/sun-angle05357/result.json`.
The angle is restored and no permanent lighting change is installed.

The source-angle result repeats near the same Iris Out phase, with stable
framing and verified restoration. A 100% screen-percentage control still shows
mottling at a similar pose, despite finer grain; the original 58.328% is restored.
The timing tool reads existing playback only. It does not pause, seek or start
performances. These are screenshot comparisons, not a temporal-video or GPU
cost acceptance. Details and the incomplete resolution restoration-screenshot
receipt are retained in the sunlight record; do not repeat that failed capture
as if the renderer setting had failed to restore.

Investigate the report from Vistani (Nexus, 2026-10-12 01:54): reflections look
like static outdoors in direct sunlight while moving, but look normal indoors
or outside direct sunlight. [Reported image](https://i.imgur.com/XiAVEbK.png).
The reporter tried disabling hardware ray tracing, updating drivers and a
ray-reconstruction/denoiser mod. They recall a fresh install looking normal,
with the issue appearing after adding more CSS skins (including Eve and goth)
and Shared Assets. These are reported observations, not verified causes or
proof of a game denoiser defect. The user suspects a connection to Eve's earlier
harsh lighting/reflection appearance; investigate that connection separately.

Queue this behind the current Peony body-motion repair. Compare the base game,
individual outfits and Shared Assets under matched lighting, movement and
render settings. Establish whether the problem is material-specific, package-
dependent or part of the game's reflection/temporal rendering before changing
anything. If a reliable workaround is demonstrated, offer an optional,
reversible MISC bug-fix setting with a name and description matching its actual
scope. Check its visual and performance costs and restoration when disabled.
Do not ship an unproven global rendering change as a confirmed fix.

During the earlier wait for game closure, the
[sunlight evidence record](../research/2026-10-12-sunlight-noise.md) now retains
the reporter's original image and a read-only live baseline of 20 console
settings. Repeated values, player/process and customization are unchanged.
The image shows blocky exposed-skin shading; its rendering cause and relation
to Peony's earlier lighting concern remain unverified. No render setting was
changed and no MISC workaround has been selected.

### Workspace recovery, 2026-10-12

`CustomShellSystem/work` is now a symlink to
`/run/media/eins0fx/theark/CSS/CustomShellSystem-work`. The user's Claude Code
session completed and verified the relocation during the disk-full interruption;
do not repeat the copy. `work/beacon-fix/reference` points to the repository's
absolute `reference` path. A subsequent scan found no broken relative links
among the 152 remaining relative symlinks.

Post-move checks in `work/storage-check1` pass: a fresh C++23 Windows `css_core`
build, a Windows UE 5.6.1 cook of three existing Genessa textures (zero warnings
or errors), and an IoStore package/verification/export round trip with matching
export and bulk payload hashes. These are workspace checks, not release or
gameplay acceptance. Nothing was installed. Authoring guards now resolve both
sides of workspace path comparisons; regression checks cover logical paths,
physical paths, sibling directories and escaping symlinks without executing
game-control tools. Older probe scripts that inferred the repository location
from their output directory now derive it from their own source path instead.

Follow [the cleanup record](cleanup-2026-10-12.md) for removals and measured disk
space. Check free space before new exports and cooks. Reuse retained inputs and
cache seeds; do not keep a complete project copy for every attempt. Remove
accounted-for rejected/superseded heavy intermediates in the same turn, retaining
recipes, useful evidence, accepted outputs and explicitly referenced backups.

The interrupted Peony stocking study survived at
`/mnt/eins0fxE/CSS-Development/peony-v2-stockings1`. `recovery-verified.json`
confirms the body and 18 fitted garments are unchanged, all four stocking layers
retain the intended body weights/morphs, and the three completed neutral render
hashes match. The follow-up `stockings-ready.blend` resets unintended nonzero
stocking shape defaults without changing mesh data. `fit-verified.json` passes
11 body-control/pose cases, retaining the intended 0.2 mm stocking offset.
The seven native stocking material packages pass fresh editor readback, Windows
cooking and IoStore/shader verification. `peony-v2-character2` now integrates the
four stocking regions into both character variants, with a separate boot-covered
section. Its 121-package Windows cook and readback preserve body/garment data,
stocking alpha, rig bindings and all 22/137 morphs. These recovery checks preceded
the installed `peony-v2-package3` trial described above. Its CSS controls are
implemented and pass portable checks; native visual review and gameplay remain
pending. Completed cook intermediates are removed under the same cleanup record.
The required optional glowing, flowing trim remains in scope.

## Objective

Audit and repair demonstrated mesh deformation, garment fit, skeletal mapping, attachment, contact and secondary-motion defects in Eve's six outfits, Unholy Genessa's two variants and Commander White. Preserve their accepted appearance, customization and gameplay behavior. Use the four reviewed videos as authoring references and UE 5.6.1/game evidence to choose implementations that actually work in the shipped game. Complete the work with reproducible assets, measured performance and explicit regression results.

Do not rebuild working systems simply to add every technique named in the request. Joint placement, weights, IK, virtual bones, sockets, body springs, hair chains and cloth each need evidence of their actual role and any defect. Quality is judged in continuous gameplay as well as still poses.

## Scope and source ownership

Paths below are relative to `/home/eins0fx/development/mods/msII`.

| Project | Included appearances | Authoring root |
| --- | --- | --- |
| Eve | Black Pearl, Prototype Planet Diving Suit, Skin Suit, Vacation Bikini, Casual Knitwear, Midsummer Alice | `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx` |
| Unholy Genessa | Both released W3 mesh variants | `CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P` |
| Commander White | Released outfit plus its current shared-pack facial replacement | `CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P` |

The differently named legacy `CSS_Seductress_eins0fx`, GeneXXa and Proxima are excluded. The two additional Eve outfits specified below are queued for later work; no other new outfits or shoe redesigns are in scope. Preserve existing animation support for all three projects and working facial support on Eve and Commander White. **Implement facial-expression support for Unholy Genessa** as specified below; it must not be assumed complete from earlier compatibility notes. Review the existing locomotion library for necessary optimization, then convert its remaining unconverted animations for the default game human skeleton. This October 8 scope update supersedes the earlier native four-animation limit. It extends target compatibility for the existing library, not acquisition of additional source animations.

Improve or fix CSS wherever necessary to support this goal, including runtime behavior, asset discovery/compatibility, customization, UI, persistence, playback, solver lifecycle and shared-pack integration. This supporting work is explicitly authorized within the goal; do not leave a required CSS change unfinished merely because the visible result belongs to an outfit. Keep changes tied to demonstrated requirements, preserve the established CSS design and commit them by context in the actual CSS repository. Explain which package a fix changes before installation.

### Asset ownership and shared-pack organization

| Owner | Contents |
| --- | --- |
| Specific Eve, Unholy Genessa or Commander White mod | Character meshes, fit/weights, character-specific morphs and facial meshes, garment/hair physics, unique skeleton extensions, materials and other assets specific to that mod |
| `CSS_SharedAssets_P` | Reusable custom skeleton foundation, shared locomotion/idle/pose assets and retargeting support, Astral assets including reusable ghost materials, and other verified reusable rig/physics resources |
| CSS runtime and CINE integration | Discovery, compatibility checks, asset selection, settings, playback and lifecycle logic that consumes those assets |

Consolidate the existing locomotion and Astral shared packs into `CSS_SharedAssets_P` with one authoritative owner for each shared asset. Share a skeleton foundation only where compatibility is proven; retain outfit-specific extensions such as Genessa's wing rig where needed. Reuse alone must not erase character-specific bindings or physics.

Use the author namespace convention **`/Game/CSS/<author>/<project>/...`**. The user's October 8 organization request supersedes the earlier preference to retain the existing unnamespaced paths. Canonical targets for this work are:

- `/Game/CSS/eins0fx/EveSB/...`
- `/Game/CSS/eins0fx/UnholyGenessa/...`
- `/Game/CSS/eins0fx/CommanderWhite/...`
- `/Game/CSS/eins0fx/Shared/Skeletons/...`
- `/Game/CSS/eins0fx/Shared/Locomotion/...`
- `/Game/CSS/eins0fx/Shared/Astral/...`

Publish this convention for other authors to use their own author/project namespace. Unreal asset paths and container filenames are separate: reusable assets still ship in `CSS_SharedAssets_P`; outfit-specific content ships in its own mod.

Before migration, inventory current package ownership, Unreal object paths, hard/soft references, manifests, saved animation/profile identifiers and CSS/CINE consumers. Build an explicit old-to-new asset map and update/rebuild references coherently, including source authoring scripts and cooked imports. Preserve public catalog/selection IDs where possible. Provide a tested compatibility path for older outfit packages and saved selections; do not assume editor redirectors survive cooking or that a file rename updates internal references. Audit character-specific assets currently shipped in shared packs, including facial replacements, and move ownership only with a working dependency and upgrade path.

Test clean installs and upgrades from separate Astral/locomotion packs and older outfit mods containing duplicate animations. Resolve legacy entries without duplicate menu choices, missing saved selections or dependence on accidental package mount order. Document which old packages must be removed or replaced and which versions can coexist. Verify missing/incompatible shared-pack handling with actionable messages. Keep the CSS runtime ZIP separate from the shared-assets ZIP, with explicit dependencies and versions.

**Organization acceptance:** an ownership/dependency manifest accounts for migrated assets, existing outfits and saved selections load correctly, and both CSS and CINE use the consolidated pack without conflicting asset providers.

## Evidence to read first

- [Research index and settled findings](../research/three-outfit-rig-physics-index.md).
- [Four-video review with timestamps and audio method](../research/2026-10-08-rig-video-review.md).
- [Released mesh, rig, source and game-dump inventory](../research/2026-10-08-three-outfit-rig-baseline.md).
- [UE 5.6 documentation and exact local 5.6.1 source findings](../research/2026-10-08-rig-physics-ue56.md).

The historical audit records a 386-bone shared prefix for Eve/Commander White, 390 for Genessa with four wing additions, and nine matching virtual-bone definitions. These are observed identities, not constants to impose on future assets. The game Skeleton dump's 1,199 raw bones must not be confused with the old 258-record mesh bind reference.

Commander White's current E9 facial replacement preserves 206 morphs and the optical repair. An older C4 outfit body is not sufficient evidence of the active facial mesh. Preserve the 19 movement-animation virtual-track repairs and derive virtual indices from the actual target skeleton.

## Preservation rules

- Keep original inputs and released ZIPs immutable. Work from traceable candidate copies or tracked source changes. Record source, export, cook, package and installed hashes separately.
- Preserve accepted proportions, silhouettes, materials, palette/glow behavior, sheer-fabric opacity and visibility, modular parts, hair appearance and existing body controls.
- Do not silently rebind all meshes, replace whole skeletons, remap animation tracks, or overwrite saved settings to make one test look correct.
- Treat **Natural chest, Bouncy glutes and Bouncy other regions** as the user's reference test setup. Retain adjustable settings; do not overwrite saved defaults without an explicit requirement.
- October 9 cloth trial: the user accepts the corrected Genessa rear-panel fit in the game world and explicitly accepts the remaining player/CSS-menu clipping for now. Preserve the world correction, record the preview limitation, and continue the other checks. Do not count preview parity as passed or reopen that limitation as a release blocker without new evidence or user direction.
- October 12 acceptance clarification: minor clipping that is hard to notice during normal play may be deferred for later improvement. Record it as a known limitation, not a repaired defect. Close-up intersections or contact counts alone must not block progress. Obvious deformation, detached or oversized clothing, conspicuous clipping and broken customization still require correction. Apply this threshold to the current Peony work and subsequent outfit checks.
- Do not impose an arbitrary attachment/bone limit such as 128. Follow verified engine constraints and actual asset counts, with explicit diagnostics for unsupported data.
  October 11 clarification: the user authorizes increasing the CSS attachment
  limit when valid outfits require it. Preserve array validation and verify
  attachment recovery and facial playback above the old limit. Keep this queued
  behind the current Peony fit repair.
- Preserve gameplay collision responses and damage/parry behavior. Private block-all collision tests do not authorize shipping block-all responses.
- No desktop focus changes, camera automation or gameplay input for diagnostics without task-specific authorization. Use existing logs/dumps first. Verify game process state before installation; do not claim core hot reload is safe.

## Phase 1: pin the baseline and reproduce defects

1. Build a manifest for all nine mesh variants and active shared facial assets: exact source recipe, cooked mesh, Skeleton, Physics Asset, clothing data, post-process graph, control rigs, materials, morphs, sockets and package ownership.
2. Verify installed identities rather than assuming an old report describes the current build. Resolve any missing source-to-final recipe before editing that asset.
3. Create a defect table with character/outfit, pose or clip/time, body settings, menu/world state, enabled simulation, screenshot/continuous footage, severity and suspected layer. Mark causes as hypotheses until isolated.
4. Compare the same pose/settings with secondary motion enabled and disabled. Separate geometry/weight problems from animation, attachment, collision and simulation problems.
5. Establish a repeatable frame-time and animation/physics baseline. Record scene, camera, frame cap, actor count and test duration.

**Exit:** every proposed repair has a reproduced defect or a concrete static validation failure. Existing accepted minor clipping is recorded, not silently promoted into a redesign.

## Phase 2: mesh, joints, weights and animation mapping

- Inspect topology around shoulders, elbows, wrists, fingers, hips, knees, ankles and toes; identify stray/degenerate geometry and discontinuous deformation without destroying intended seams or cutouts.
- Compare bone parents, joint centers, local axes/roll, reference/bind matrices, units/scales and retarget settings against the actual native game references. Distinguish deforming, attachment, helper and virtual bones.
- Audit normalized skin weights, unweighted vertices, unexpected influences, export influence reduction, twist distribution and section/LOD bone maps. Validate the exported/cooked result, not just the Blender viewport.
- Inspect both hands, thumb articulation, finger lengths in pose, wrist orientation and grips. Check weapon sockets and intentional animation release phases before changing anatomy.
- Check feet inside each shoe: arch, ankle, toe clearance, heel height and left/right fit. Keep rest shape, weights, morph deltas and shoe alignment coherent. Use runtime IK only where the game graph and contact phase require it.
- Preserve virtual-bone targets and the 19 repaired movement mappings. Do not mutate compressed body motion as a side effect of lookup repair. Verify all affected skeleton variants and shared facial replacements.
- Recheck every existing body morph and permitted combinations at neutral and supported limits. Garments and fitted ornaments must follow the same intended surface without conspicuous separation or clipping introduced by the repair. Record minor unobtrusive clipping under the October 12 deferral instead of requiring zero intersections.

**Exit:** before/after deformation evidence and import/cook readback show the intended improvement, with no unexplained skeletal, morph or attachment changes.

### Queued CW walking foot-alignment report

**October 14 resolution:** the user accepts the walking-only correction after
the boots-on/off prompt ("Walking looks right now"). The reported obvious tilt
is closed at that scope. The accepted clips now have permanent outfit-owned
paths and are installed; runtime resolution of those renamed paths, terrain
and wider lifecycle/performance coverage remain separate. The investigation
and acceptance criteria below record the original report.

On October 9 the user reported Commander White's feet look tilted inward or
sideways, most clearly while walking. Treat this as a visible motion defect
to investigate, not a confirmed skeleton fault. It is queued after the current
CW package cleanup and does not reopen the accepted hair appearance.

The user confirmed it happens with boots both shown and hidden. Include bare
feet in the comparison; a boot-only repair cannot satisfy this report.

Capture both feet through a complete walking cycle with the selected clip
recorded. Compare the same motion on the baseline and current CW meshes,
normal and facial replacements, and a native human reference where useful.
Inspect ankle/foot/toe reference axes and retarget transforms, foot and shoe
weights, heel/ground offsets, animation curves, virtual bones and any active
IK or post-process contribution. Compare idle, walk, jog and sprint to
distinguish persistent alignment from a clip-specific pose. Change the layer
that causes the defect; do not straighten every frame or alter the shared
skeleton without evidence.

**Acceptance:** the demonstrated walking tilt is corrected with side-by-side
motion evidence; foot/heel contact and left/right alignment remain plausible
through the gait cycle. Preserve shoe shape, facial playback, action
interruption, other gaits, CINE and doubles. Record remaining contact limits
and regression coverage. Outfit-specific fit belongs in CW; reusable playback
or retargeting repairs belong in CSS/shared assets where proven necessary.

### Unholy Genessa facial-expression support

Implement and validate facial-expression support for both Unholy Genessa variants through CSS and CINE. Inspect each active face mesh, facial bones/morphs, eye and mouth geometry, materials and existing playback bindings first. Add the missing character-specific controls or adapter in the Unholy Genessa mod; keep reusable expression assets in `CSS_SharedAssets_P` and general playback logic in CSS/CINE according to the ownership rules above.

Animations containing facial tracks must drive the expressions in sync with body playback, including Iris Out and other existing clips with verified facial data. Map controls to Genessa's actual face rather than assuming Eve's morph names or bone transforms apply. Preserve Genessa's identity, neutral face, eye visibility/glow, face/body alignment and customization. Check eyelids, eyeballs, jaw and lips for collapsed or missing geometry.

Handle clip loops/end, movement, attack, aiming, abilities, outfit changes and menu/world transitions without leaving a stuck expression or blocking gameplay. Restore the appropriate neutral or game-controlled face on interruption. Body-only clips remain usable without fabricated facial tracks. Verify compatibility with Faithful/Stray forms and customized doubles without disturbing their ghost materials or pose.

**Acceptance:** both variants visibly play synchronized expressions in CSS and CINE, restore correctly after interruption, and pass eye/material/gameplay checks. Record the tested clips and any unsupported controls explicitly. Eve and Commander White facial playback must remain working.

## Phase 3: collision and secondary motion

October 9 follow-up: the user confirmed Commander White needs physics for her
belt and smoother skirt physics because its current motion is jerky. The user
explicitly queued this for later and asked to focus on Eve first. After the
current Eve work, inspect CW's belt anchors, weighted chains and collision,
then reproduce the skirt motion through walking, turns and start/stop
transitions before choosing a repair. Verify continuous motion, recovery and
cost rather than accepting a still pose. Preserve her accepted hair, outfit
shape, modular controls and gameplay behavior. This remains separate from
the queued CW walking foot-alignment report.

October 9 naming follow-up: when returning to Commander White, remove any
`dev` or `Development` qualifier from the user-facing mod name and release
metadata. Check the displayed catalog/manifest name, package description,
release filenames and release text. Preserve stable selection IDs, asset paths
and internal diagnostic labels unless a functional migration requires changing them. This is queued
with CW's package work; Eve remains first.

Create a region-to-solver table per outfit. For each region record driven bones/vertices, solver, simulation space, anchors, collision source, tick order, reset behavior and UI controls. Avoid two systems independently driving the same chain.

- **Body motion:** inspect current springs and regional controls, damping, limits and recovery. Keep motion responsive and stable through gait changes and sudden actions. Coordinate body deformation with fitted clothing and ornaments.
- **Hair:** preserve hairstyle/materials; inspect root anchoring, chain orientation/length, weight gradients, head/shoulder/back clearance, reset behavior and collision cost. Test head turns, walking, attacks and teleports.
- **Loose cloth:** inspect simulation mesh, pin/constraint maps, render transfer, physical material, collision thickness and active colliders. Check visible fabric, not only particle-intersection counts. Do not repeat the rejected Commander White larger-pelvis-collider trial without new evidence.
- **Fitted garments and metal:** diagnose skinning/morph mismatch before applying loose-cloth simulation. Reproduce Unholy Genessa's menu/world ornament mismatch if still present in the baseline.
- **Wings and train:** preserve Unholy Genessa's accepted idle/walk/jog/sprint wing behavior; inspect hinge axes, weights, limits, attachment and interaction with nearby cloth. Keep deliberate gait motion separate from secondary response.
- **Collision:** identify the real consumer. Physics Asset, Chaos Cloth, Control Rig contact geometry and bone-solver limits are not interchangeable. Verify collider fit at supported body shapes and motion extremes without changing gameplay hit behavior.
- **Morph support:** determine each clothing asset's actual UE path. UE 5.6 simulation-morph support does not mean every legacy garment automatically follows skeletal-mesh morphs. Verify render/simulation correspondence and collider clearance explicitly.

V1's Blender cloth proxy is an optional reference experiment only if a reproduced defect needs it. Its Surface Deform modifier is not an automatic runtime export. Do not promise volumetric soft-body contact or add a costly new solver without a bounded prototype, engine support and measured benefit.

**Exit:** improved continuous motion and contact, stable transitions/resets, preserved controls and measured cost for every changed region.

## Follow-up stage: locomotion review and native-human conversion

After the rig changes and shared ownership are stable, review the locomotion work already completed. Reoptimize only where evidence shows a motion-quality, compatibility, memory, loading or runtime-cost problem. Check start/stop blending, the reported jog pop on stopping, loop boundaries, foot sliding, root motion, virtual-track mappings, sampling/compression and gameplay interruption. Preserve accepted motion, timing, facial synchronization and animation identifiers. Record unchanged clips as reviewed rather than recooking the entire library without cause.

Then inventory every existing library animation against the default game human skeleton: already converted, unconverted, validated, failed or blocked. Convert the remaining unconverted clips/poses in that inventory, preserving the existing four native gait animations. Validate actual target bind proportions, retargeting, virtual bones, root/additive conventions, contact and action cancellation. Bone-name compatibility alone is not acceptance.

Keep body-animation compatibility distinct from facial capability. For default game human targets, test facial tracks where compatible controls exist; report missing facial support explicitly while preserving usable body playback. Retain Eve and Commander White's working facial support and complete the Unholy Genessa facial implementation above. Verify discoverability and playback through the existing CSS Locomotion > Idle Animation/search design and CINE controls.

**Exit:** every inventoried animation has an explicit conversion/validation result and exact remaining counts. Ship converted assets through `CSS_SharedAssets_P`; retain source mappings and record any technically blocked case for user disposition rather than silently dropping it.

## Phase 4: lifecycle, gameplay and regression checks

Record pass, fail, untested or deferred for each applicable outfit and case. A static compatibility check does not count as live acceptance.

| Area | Required checks |
| --- | --- |
| Locomotion | Idle, walk, jog, sprint, start/stop, turns and speed changes; preserve prior transition repairs |
| Actions | Attack/combo, aim/fire, dodge, interaction and Genessa ability interrupt custom idles and return control correctly |
| Existing animation library | Validate affected bindings across existing clips/poses; live-check representative extremes, loop/end/interruption and facial combinations across all three characters |
| Native-human conversion | Account for the full existing-library conversion inventory; validate converted body playback, target-specific facial support and gameplay interruption |
| Shared-pack migration | Clean/upgrade installs, legacy duplicate providers, saved selections, missing dependencies, CSS and CINE discovery/playback |
| Face | New Unholy Genessa expression support on both variants; existing Eve and Commander White playback/restoration, eye visibility, material slots and face/body alignment, including current Commander White E9 |
| Preview/world | Same body/settings/pose where possible; open/close CSS/player menu, change visibility/LOD, verify tick order and secondary-motion recovery |
| Lifecycle | Outfit/profile changes, save/reload, gates/launchpads and delayed resets, Harbinger transitions, death/revive and Genessa form changes |
| Doubles | Faithful and Stray, Default/Use CSS, appearance/customization, ghost materials, full cloth, legs/feet and cleanup |
| Customization | Morph limits/combinations, physics controls, modular visibility, sheer opacity, palettes and glow; saved settings survive |
| Attachments/gameplay | Both hands, weapon/sidearm sockets and aim, feet/heels, accessories, damage/parry behavior |
| Existing UI fixes | Enter profile save, valid/invalid names, SHELL status layout, MISC Heart of Vatra and Genessa Form |

The `dddaf70` gameplay-interruption candidate was installed before this study. Its installed core hash was rechecked, and the user confirmed on October 8 that Iris Out interruption, attack, aim/fire, doubles and Genessa Form all work. Preserve that accepted baseline and recheck it after relevant changes. Previously accepted eye and doubles behavior must remain intact. The separately deferred After Brute Battle crash and user-accepted floor intersections remain recorded exclusions unless the user reopens them or a new change causes a regression.

## Performance and engineering gates

The quality target is responsive, stable gameplay with high performance, no newly introduced lag, FPS drops or behavioral regressions. Treat this as an acceptance requirement backed by measurements and tests, not an unverified promise that every possible configuration is bug-free.

- Use the project's declared C++ standard/toolchain and local UE 5.6.1 authoring build. Confirm shipped plugin/API compatibility from dumps and code before relying on it.
- Apply C++ ownership/lifetime discipline, RAII for owned native resources, initialized state, validated bounds and explicit error handling. Use Unreal-compatible object references and thread rules; check reflected calls against the shipped game rather than guessing layouts or relying on unchecked casts. Review changed code for leaks, stale references, races and unsafe callbacks.
- For runtime changes, keep UObject/animation-worker ownership safe, avoid per-frame asset discovery or unnecessary allocations, bound work by actual relevant components, and preserve graph/montage ownership on interruption. Cache with lifecycle-aware invalidation, clean up hooks/delegates/resources, and keep blocking I/O and diagnostic spam out of hot paths.
- Handle relevant edge cases explicitly: missing or incompatible assets, unavailable facial controls, invalid settings, empty/duplicate catalog entries, delayed loads, destroyed/replaced components, repeated requests, interrupted transitions and failed initialization. Preserve valid saved state and use a clear, recoverable fallback instead of leaving the player unable to act.
- Compare baseline/candidate under matched conditions: frame-time median/p95/p99, transition spikes, animation/cloth CPU work and relevant collider/chain/simulation counts. Retain raw capture durations and limitations.
- Exercise 30 and 60 fps conditions, plus a higher rate where available, to reveal integration instability. A 30 fps source clip can render smoothly at 60 fps; do not resample blindly as a substitute for correct motion and blending.
- Do not increase iterations, collision complexity or solver count without a visible benefit and acceptable measured cost. Report unresolved costs honestly, including the previously documented fresh full-cloth-double creation hitch; this goal does not silently absorb the separate beta.12 optimization project.
- Run meaningful targeted tests for changed logic and existing required checks. Exercise the regression matrix across affected outfits, shared assets and CSS/CINE consumers, including repeated switches and longer play where lifecycle/resource accumulation is relevant. Separate automated, static and in-game results; untested cases remain marked untested.
- Investigate any reproducible new hitch, FPS loss, crash or functional regression before release. Fix it or revert the responsible change; disclose any unresolved tradeoff for explicit user acceptance. Preserve previously accepted behavior and recorded deferrals. Completion claims must state the tested coverage and remaining limits.

## Queued follow-up: two additional Eve outfits

**Status: source preparation and fitting in progress, not release-ready.** The
user's latest direction prioritizes Eve. Commander White's foot, belt and skirt
follow-ups remain queued. Preserve the accepted Genessa world-cloth correction
and its documented preview limitation. The two additions remain separate from
the nine-variant repair baseline; the original repair and release scope stays
active.

Use the references in `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/reference/new-eve-outfit`:

- `ALL-IN-ONE - Peony-Hydranega V2 CNS-1724-2-1-1763118109.zip`
- `Recolors Texture-1724-1-2-1755607929.zip` (Peony/Hydrangea colors)
- `DOAXVV Slipstream CNS-3108-1-0-1778702686.zip`

Add both outfits to the current Eve mod alongside its existing six outfits. Inspect the archives before deciding the final component breakdown or display names. Preserve the reference designs and keep original inputs unchanged.

### Immediate Peony correction, October 11

**Corrected source, confirmed by the user on October 11:** use
`reference/new-eve-outfit/ALL-IN-ONE - Peony-Hydranega V2 CNS-1724-2-1-1763118109.zip`
and `reference/new-eve-outfit/Recolors Texture-1724-1-2-1755607929.zip`
under the Eve project. Retire `Peony Better Cleavage-1851-v1-0-1755197848.rar`
from this implementation. Preserve it only as historical input, not as the
garment to keep fitting. Rebuild from the corrected V2 source while retaining
the accepted Eve body, CSS customization requirements and requested removals.
The October 11 16:49 screenshots reject the whole installed garment, including
jacket, corset, back lacing and trim, as well as the lower sections. Compare
against the supplied V2 references: clean structured fabric, intact trim and
lacing, and a close body-following silhouette. Previous geometry, strain and
cook checks do not establish visual acceptance. Review the actual source
variants before choosing one; do not carry damaged fitted geometry forward.

**Palette correction:** include every distinct color scheme supplied by the V2
CNS and Recolors Texture archives as a CSS customization template. The default
must match the user's black-and-white floral Hydrangea reference: light patterned
fabric with black trim and dark lower fabric. Preserve the source pattern and
material contrast. The earlier five-template count no longer limits Peony;
body-shape variants are not additional color palettes. Retain manual tint and
optional flowing glow controls. This does not change Slipstream's palette scope.
The user's latest reference confirms this default again. Match its light floral
panels, black edging and dark leg fabric; do not substitute the previous gold-trim
trial. Use the included schemes as templates rather than inventing replacements.

The user rejected the installed Peony trial's fit and world-space cloth behavior.
Correct these before continuing Slipstream binding. Reuse the earlier Eve
garment-fitting, skin-weight transfer and body-morph workflows, including the
Planet Diving Suit work, rather than treating clearance-only expansion as a
finished body fit. Original inputs and the accepted Eve body remain unchanged.

- Fit the oversized jacket and corset closely to the body. Follow all six CSS
  body-shape controls, intermediate values and combined settings, plus secondary
  body motion. Do not enlarge the body to fill oversized clothes.
  The user reiterated that visible gaps around the breasts and buttocks are
  unacceptable. Check those silhouettes explicitly with the matching body-slider
  values; a clearance check alone does not establish a close fit.
  The installed `peony-refit1` remains rejected: the latest rear/side screenshots
  show oversized black lower straps standing away from the hips and buttocks.
  Reproduce that gap with the actual selected body values and identify the
  responsible garment sections before fitting them to the body.
- The user authorizes a redesigned corset neckline: a fitted sweetheart curve
  with a lower center opening for visible cleavage. The follow-up explicitly
  requests more cleavage: deepen and widen the center opening while retaining
  fitted cups, and check the breast-size range. Preserve the outfit's
  fabric pattern and trim; avoid the boxy, detached cup silhouette.
- Remove both hip pockets and exposed robotic ornaments from the outfit mesh,
  including the arm ornaments, and remove their obsolete customization controls.
  Check remaining panel-mounted hardware too; keep fabric and its decorative
  trim rather than treating all small components as robotic. Preserve the hand
  gloves and neck collar. The user explicitly confirmed keeping the heels and
  ponytail attachment as they are. Preserve the original source assets.
- Fix body clipping through the lower front garment, including the thigh and
  crotch seams. Check body-shape settings and posed movement, not only rest pose.
- Fix the hanging panels that distort and appear ragged in the game world.
  Inspect their render/simulation correspondence, anchors, weights, collision
  and body-morph correction using the established Genessa/Eve work. Keep cloth
  simulation and the intended panel shape; do not hide or remove the panels as
  a substitute for this repair.
  The latest `peony-refit1` world screenshot also shows a lower panel folded and
  distorted during movement. This remains an open runtime defect, separate from
  the oversized straps. Prior import, cook and cloth-buffer checks did not prove
  that the panel behaves correctly in gameplay.
  User clarification: the lower garment hangs away from the body even while
  standing still, as well as during movement. Do not require another animation
  comparison to establish this fit/attachment defect. Fix the stationary fit
  first, then verify motion independently.
  The user rejected installed `peony-refit2` with the October 11 16:37 world
  screenshots. Lower panels still bunch and distort. This is the current
  priority; do not treat its earlier strain/GPU/cook passes as fit acceptance.

Evidence includes the screenshots supplied in this conversation and
`/home/eins0fx/Pictures/screenshot-2026-10-11_03-18-49.png` (world fabric) and
`/home/eins0fx/Pictures/screenshot-2026-10-11_03-17-44.png` (pocket removal).

**Acceptance:** inspect the final textured outfit from front, side and back in
CSS/player preview and gameplay. Check neutral, individual slider endpoints,
intermediate and combined settings, idle/walk/jog/sprint, turns and action
interruption. Verify normal/facial meshes, Iris Out, eyes, clothing visibility,
all supplied palette templates, glow, saved settings and supported doubles. Record remaining
defects honestly. Private fitting renders do not establish a runtime fix.
These corrections supersede preserving the rejected Peony design unchanged;
the rest of the three-outfit goal remains active.

Each new outfit must have modular, individually removable garment components through CSS customization, allowing the outfit to be undressed. Peony must include **every distinct color scheme supplied by the corrected V2 and Recolors Texture archives**, with black-and-white floral as the default. Slipstream retains **five distinct template color palettes**. Use the established customization and saved-profile behavior. Validate component combinations, palette application, body-shape fit, skin weights, collision, garment physics and animation/facial compatibility in both preview and gameplay. Apply the same performance and regression gates as the existing outfits.

October 9 palette direction: include the user's supplied black-and-white
Hydrangea/Peony reference, confirmed on October 11 as Peony's default. Use pearl-white fabric
with the existing pattern visible in soft gray, glossy black edging, straps
and hardware, sheer black stockings, and warm amber emissive accents. Compare
the result in neutral preview and warm/cool world lighting. This is a palette
reference; preserve the accepted Eve body and the requested garment design.

October 9 Peony trim request: add optional flowing glow along the black edging
and line details, using Unholy Genessa's garment emission as the reference.
Preserve glossy black trim when emission is off. Author an explicit trim mask
and stable garment coordinates so the pulse follows the lines without lighting
the white patterned fabric, skin or stockings. Reuse the existing CSS glow
color, strength and coverage controls; expose flow speed through the established
customization system where supported. Keep Peony masks and material instances
in the Eve mod, with any reusable material function in CSS_SharedAssets_P.
Validate glow off/on, flow continuity across garment seams, removable parts,
all supplied Peony palettes, saved profiles and supported astral doubles in preview and
gameplay. The V2 implementation is installed; the user accepts flowing trim in
CSS and the world and its appearance on current-form doubles. The complete
palette/profile/visibility matrix and other Genessa form remain open.
October 12 reminder: retain this glowing, flowing trim requirement in the V2
material rebuild. The minor-clipping deferral does not defer or remove it.
The V2 implementation adds preserved rest-space flow UVs to all 18 parts and
optional upper trim, lower-panel border and hip-strap emission across the four
palette recipes. Fresh editor readback, Windows cooking, package/shader readback
and cooked surface checks pass their limited checks. Cooked UV1 retains the
common flow coordinates. A private full-character assembly now preserves the
accepted Eve body/hair, 22 body/hand morphs and the V2 garments; its facial
variant adds the accepted 115 expressions. Fresh editor reload verifies the
shared rig, nine virtual bones, material bindings and boot-coverage sections.
Windows cooking and full-character cooked geometry/UV, rig-link, skin-weight
and body/facial-morph readback also pass. Tangent fallback remains on 430
vertices, with visual impact unverified. This is not a fit or cloth acceptance:
native visual review, cloth, stockings, CSS controls, profiles and gameplay
verification are still required. See the
[flow implementation record](../research/2026-10-12-peony-v2-materials.md#first-v2-flow-implementation).

Keep outfit-specific meshes, materials, physics and palette definitions in the Eve mod under `/Game/CSS/eins0fx/EveSB/...`; put only genuinely reusable assets in `CSS_SharedAssets_P`. Make supporting CSS changes where needed while preserving existing controls and saved selections.

**Acceptance:** both outfits are selectable, modular visibility and all required palette templates work and persist, and fit/physics/gameplay checks are recorded. Verify that a new Peony selection uses black-and-white floral and saved palette choices restore correctly. Track them separately from the nine existing repair variants. Release inclusion/version remains to be settled when this queued work is reached; do not silently change the coordinated release targets or report these outfits as completed.

## Low-priority follow-up: minor outfit clipping

The user explicitly defers small, unobtrusive clipping on October 12. Keep a
record of the affected outfit, part and known pose/body settings for later
polish. Assess visibility at normal viewing scale before spending more fitting
iterations on magnified intersections. This is not blanket acceptance of all
remaining Peony defects: obvious gaps, broken silhouettes and loose garments
remain in the current repair scope. No extra user test is needed solely to
confirm an inconspicuous close-up intersection.

## Deferred follow-up: Genessa Form after switching save games

**October 15:** the upgraded save reproduces a refused Faithful request. Its
active Alteration upgrade applies `State.ReviveBlock`; the native revival
ability explicitly rejects this tag. Native player-zero and CSS resolve the
same pawn. This explains the captured failure, not every cross-save case.

October 14 offline investigation confirms that CSS does not persist a forced
form preference, and pending requests cancel on player replacement. The native
revival helper sends an event without reporting success. This does not establish
the cause by itself. October 15 live tags, owned upgrade effects and extracted
ability bytecode identify the restriction above. See
[the diagnosis and capture procedure](genessa-form-save-switch.md).

**Queued October 15, optional override requested:** investigate a temporary
Faithful override while retaining the user's Alteration upgrade. Normal game
rules remain the default. Present a confirmation dialog describing the native
restriction and the override's verified effects, with Confirm and Cancel.
Cancel must leave the game unchanged. Do not persist the override in CSS
profiles or change progression. Before promising no save impact, inspect native
form persistence and autosave, including save/load, death, travel, disabling CSS
and ending the override. Restore only state CSS owns. Do not simply remove a
gameplay tag or upgrade without a verified lifecycle and restoration path.
Finish the current cleanup/message check before implementing this queued feature.

Reproduce by selecting Stray on one save, returning to the menu and loading another save, then requesting Faithful. Compare switching saves within a session with a full restart, and compare previous Auto/Default and Faithful choices. Check whether the destination save's gameplay shell is actually Genessa and whether the character is in a state where the native transition is permitted. Observe the actual form, requested form, UI selection, transition completion and ability state separately.

Inspect form-control state across save/world/player replacement, including stale actor references, pending requests, cached shell/form identity and whether a saved preference is mistaken for the newly loaded character's actual state. These are investigation targets, not established causes. Preserve each save's progression and native form rules; do not force an unsupported shell into a Genessa transition or edit save data to work around the symptom.

**Acceptance:** after loading another save, supported Faithful/Stray requests work when the native transition is valid, the UI reflects the current character, and Auto/Default returns control to the game. Verify both directions, a fresh launch, existing single-save behavior, doubles, customization and animation interruption. Record any legitimate game restrictions and the actual cause before describing this as fixed.

## Low-priority follow-up: Trebuchaxe sidearm visibility

**Status:** original report now supported by the user's October 15 in-game screenshot. Controlled reproduction, diagnosis and repair remain pending. Investigate and fix after the primary rig, facial and animation work; this reminder does not change the current priority.

DirektorSvemira reported on 2026-10-08 at 00:05 (report timezone unspecified) that CSS sidearm visibility set to **Only when in use** produces a hovering axe above the Trebuchaxe. The offset persists when the weapon turns sideways for a charged shot. The reporter says gameplay is unaffected and no other installed mod alters the weapon or its hiding behavior. CSS causation and any Tarstone interaction remain unverified.

The user's [October 15 screenshot](/home/eins0fx/Pictures/screenshot-2026-10-15_04-53-53.png)
shows Peony aiming the Trebuchaxe with a visibly separated axe suspended above
the weapon. The screenshot does not establish the current visibility setting,
Tarstone loadout, charged-shot behavior or the detached object's identity.
Keep those details from the original report separate until checked. The passing
Naylshotte Burn draw/fire/stow test does not resolve this defect.

Reported sidearm Tarstones, top slot to bottom:

1. Tarred Fragment
2. Monarch's Vestige
3. Squall Stone

When this item is reached, reproduce with that loadout and visibility setting; compare other visibility modes, no Tarstones and individual stones. Inspect the weapon's attached components, sockets, relative transforms and visibility propagation through equip, hide/show, aim and charged-shot transitions. Determine which object is hovering before changing attachment or visibility logic.

**Acceptance:** no unintended hovering axe through those transitions; the intended weapon and effects remain visible when appropriate, charged shots and aiming work normally, and other sidearms/visibility modes retain their behavior. Record reproduction evidence, the actual cause and regression results. Keep this item queued if not yet investigated; do not describe it as fixed merely because the main goal is ready.

## Low-priority follow-up: Naylshotte Tarstone effects while hidden

**Status:** partial verification on October 15. Mesh-only hiding reproduces lingering Burn; the installed owner-visibility repair removes it without deactivating infusion. Actual draw/fire/stow passes on the user's new save. Effect replacement, Barrage, damage and the original natural owner-drift trigger remain open. See [evidence](../research/2026-10-15-sidearm-aim.md). A shared cause with Trebuchaxe has not been established.

With Naylshotte visibility set to **Only when in use**, the user reports a lingering Tarstone effect while the sidearm should be hidden. The supplied world screenshot shows an orange, weapon-shaped effect behind the character's shoulder. The loadout screenshots identify **Barrage Stone, Level 3** in the top slot and **Emberseed Stone, Level 3** in the second slot. The third slot's name is unconfirmed; do not infer it from its icon. Emberseed's displayed permanent-infusion upgrade is relevant reproduction context, not proof of the cause.

Evidence:

- `/home/eins0fx/Pictures/screenshot-2026-10-11_04-04-11.png` (Emberseed Stone/loadout).
- `/home/eins0fx/Pictures/screenshot-2026-10-11_04-04-03.png` (Barrage Stone/loadout).
- World screenshot attached to the same user report (local path not supplied).

Identify the lingering object's owner and lifecycle before changing visibility logic. Check attached components, independently spawned effects and material overlays, including effects created after the sidearm has been hidden. Compare visibility modes, no Tarstones and individual stones through equip, aim/fire, stow, weapon swaps, form changes and save/reload. Preserve the infusion and its gameplay effects; do not suppress unrelated projectiles or world effects to hide the symptom.

**Acceptance:** the hidden sidearm's attached visuals no longer linger, appropriate weapon effects return when it is in use, and infusion, damage, aiming and firing retain their behavior. Check other sidearms and the Trebuchaxe report separately. Record the actual cause and regression results; keep this queued until investigated and verified.

## Low-priority follow-up: Zhirelle and Merrick appearances

**Status:** queued at the user's request on October 11. Eve remains the current priority. The requested targets are **Zhirelle and Merrick** under the existing **SHELL > Use NPC/Enemy** selector. Also support other NPCs that use the same skeleton as either target and pass the compatibility checks below. This is not a general expansion to NPCs with other skeletons.

Inspect the game assets and dumps to identify Zhirelle's and Merrick's meshes, skeletons, animation setups, materials and attached components. Verify which other NPCs share either skeleton from asset references rather than names or visual similarity. Sharing a skeleton makes an NPC eligible for this work, but does not alone prove that its mesh, proportions, attachments and animation behavior work correctly. Determine why an appearance is missing or incompatible before choosing a repair. Retargeting is a possible solution, not an established requirement. Check whether the existing native-human path can support the targets first; reuse the repair for matching NPCs where valid rather than changing shared behavior blindly.

Keep the gameplay shell, abilities and progression unchanged. Follow the existing selector and saved-profile design, expose only validated options, and give a clear fallback for unsupported appearances. Validate movement, combat, aiming, attachments, menu/world transitions, save/reload and appearance restoration after gates or form changes. Assess doubles compatibility separately; do not imply every NPC supports it.

**Acceptance:** Zhirelle, Merrick and validated NPCs sharing either skeleton can be selected and restored reliably without breaking gameplay or existing appearance choices. Record the shared-skeleton matches, tested characters and limitations. NPCs using different skeletons remain outside this follow-up. This is deferred work, not a release blocker or a claim of current support.

## Low-priority follow-up: native shell shades for appearance overrides

**Status:** queued investigation, not implemented. HydraVenom8's October 10 request was clarified by DoraGong4N as choosing a native shell's alternate shade while using that shell only as the CSS appearance. Examples supplied were Eredrim's Yellow King/Obsidian looks. Kenshinto described playing Thiel while displaying Eredrim with another color. These names and colors are report examples; verify the available Mortal Shell II assets and variants before promising specific choices.

Inspect how the game selects native shades, including material/mesh substitutions, shell association and unlock state. Add supported shade selection through the existing SHELL appearance flow while retaining the actual gameplay shell and its abilities. Respect the game's ownership and unlock behavior; do not silently unlock shades or alter progression. Preserve existing profiles and original-appearance defaults when no shade is selected or an asset is unavailable.

Validate selection, material coverage, saved profiles, save/reload, preview/world consistency, gates, death/revive and shell/form changes. Check interaction with supported CSS doubles and non-CSS replacements without claiming universal compatibility. Keep shade selection distinct from custom outfit palettes and avoid duplicate appearance entries where the existing UI can express the choice.

**Acceptance:** a supported native shell appearance can use a verified alternate shade while another gameplay shell remains active. Its selection persists and restores correctly; missing or unsupported shades fall back cleanly. Record actual supported variants and regression results. Schedule after the primary work alongside the other low-priority follow-ups.

## Delivery, progress and completion

Maintain a compact status table for nine variants, repaired defects, validation cases and remaining failures. Report actual counts, not invented percentages. Update the research index when a question is settled or a trial is rejected.

Keep source/runtime/shared-pack changes in separate contextual commits. Provide exact candidate paths, hashes, dependencies and rollback receipts. Install only completed candidates at the authorized testing point with the game state verified. Final release packaging/versioning is decided with the user; retain the established ZIP and BBCode colocation convention.

The user has now specified the outfit release targets: **Eve v1.5.0**, **Unholy
Genessa v0.3.0**, and **Commander White v0.1.0**. Deliver the coordinated
`CSS_SharedAssets_P` and CSS runtime update alongside those outfits, in separate
ZIPs with explicit compatible versions. The CSS runtime release target is now **v1.0.0**, as requested by the user
for this standardization milestone. This is a release target, not a waiver of
the remaining compatibility, performance and regression gates. The shared-pack
version is separate and has not yet been confirmed. An installed trial is not a completed
release: report source, installed-test and upload-ready states separately.

### Cleanup during work

Clean up verified unused files, rejected candidates and redundant intermediates throughout the work, after each accepted or rejected trial and before large exports/cooks. Check available disk space before expensive generation so abandoned outputs do not accumulate until release.

Before deleting, check references and dependencies, source/build provenance, active jobs and rollback needs. Keep original source assets, user reference videos, accepted authoring files, current installed/release artifacts and the necessary known-good rollback. A file being old or named temporary is not enough evidence that it is unused. Leave uncertain or another active session's files intact until ownership and use are resolved.

For rejected trials, retain the small recipe, parameters, failure reason and useful evidence before removing reproducible heavy outputs. Remove duplicate exports/cooks/packages, stale task-local caches and abandoned trial files only once their lack of remaining use is established. Use targeted deletions; avoid blanket cleanup of shared directories or Git worktrees containing unrelated changes.

Record deleted paths or groups, why they were safe to remove and measured space recovered in the work log. Cleanup must preserve reproducibility and the ability to recover the current working build.

Completion requires all scoped variants accounted for, the two queued Eve outfits and their required palettes completed or explicitly deferred by the user, Unholy Genessa facial support accepted, agreed defects repaired or explicitly deferred, shared-pack organization accepted, locomotion review and remaining native-human conversions accounted for, regression results recorded, source-to-package provenance complete and a concise list of remaining limitations. Do not mark the goal complete from a Blender preview alone.

**First implementation steps when activated:** verify the installed gameplay-interruption baseline, pin the nine-variant/current-facial manifest, then capture matched poses to prioritize the first demonstrated deformation defect.
