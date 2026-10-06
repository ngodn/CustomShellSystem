# Stellar Blade animation library for CSS and CINE

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

Updated 2026-10-06. Protocol Echelon Proxima remains paused.

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

## Objective

Port every unique character animation supplied under `extensions/cine/docs/reference/animation/`, including its `atool/` subfolder, into a reusable Mortal Shell II animation library using the established Stellar Blade to CSS retargeting workflow and Unreal Engine 5.6.1. Make the resulting animations available in both CSS's animation menu and CINE's scene controls, as confirmed by the user. Preserve each clip's recognizable performance, timing, body and finger motion, appropriate root movement and contact. Validate the cooked assets and live playback, preserve existing outfits and gameplay, and prepare an accepted distributable with installation notes, credits, BBCode notes and checksums together.

The user explicitly chose a separately released shared pack: **`CSS_LocomotionSharedAssets_P`**, with authored assets under **`/Game/CSS/Locomotion/`**. Include the previously released idle library as well as the new ports. Distribute the asset ZIP separately from CSS runtime and CINE updates, following the Astral Shared Assets packaging pattern. Existing outfit releases must continue working without repackaging.

The user wants smooth playback and clarified that the mention of 60 fps was a question, not a mandate to resample every asset. Preserve each source's native sample rate and timing by default: 30 Hz sources stay 30 Hz and native 60 Hz sources stay 60 Hz. Use higher-rate baking only where a measured retargeting or procedural-motion issue benefits, with pose/timing comparisons. Keep previous idles at their proven rate unless a specific improvement is demonstrated. Unreal interpolates between animation samples; a 30 Hz asset does not cap rendering at 30 fps, and interpolating more stored samples does not recover missing captured motion. Check transitions, contacts, loop seams and runtime frame times separately. Target smooth 60 fps gameplay where the machine and scene support it, without promising a universal frame rate.

Do not reduce this to the previously ported seven ESAP poses, a few easy idles or one showcase dance. Inventory all supplied archives and account for every unique animation. A duplicate or non-animation dependency must be identified with evidence; a difficult or broken clip stays explicitly unfinished instead of silently disappearing from scope.

The user also explicitly includes the **31 built-in Stellar Blade photo poses**
referenced by ATOOL's `/Game/Mods/HD-ATOOL_P/data/a1` table. Decode them from the
installed source game and include their unique body performances in both CSS
and CINE through the shared pack. The user corrected the earlier answer and
wants **facial-expression support where feasible**, including investigating
the **21 expressions** in `data/a2`. Inspect their actual bone/curve/pose data
and the current outfit facial rigs before selecting an implementation. Add CSS
controls for verified compatible faces, with neutral/reset and restoration on
outfit changes, menus, cancellation and unload. Keep body animation and eye,
material and physics settings intact. Make compatible expressions reusable by
CINE through the shared catalog/service contract where feasible. Unsupported
faces must remain unchanged, with clear availability rather than guessed bone
or morph mappings. Record any expression/rig that cannot be supported and the
evidence explaining why. Facial support is a feasibility requirement, not a
promise that every outfit has a compatible face rig.

## Shared pack and older outfit compatibility

### Required compatibility scope, clarified October 6

- First-class custom-rig support covers Eve (all six outfits), Unholy Genessa (both face variants), and Commander White only. Other appearances are covered through compatible default-game-human-skeleton support, not new custom-rig adapters.
- Also support other authors' CSS mods, non-CSS mods and native appearances that use the game's default human skeleton **where technically compatible**. A CSS outfit manifest or eins0fx author name must not be required solely to play a compatible body animation. Check actual skeleton hierarchy, reference pose, virtual bones, animation layers and required runtime bindings. Do not declare compatibility from the skeleton label alone.
- Preserve each supported appearance's customization, proportions, hands, footwear, hair, cloth, wings and secondary motion. Add adapters where needed for custom skeletons and post-process graphs. Body playback and cancellation/restoration must work through both CSS and CINE without requiring old outfit packages to be rebuilt.
- Facial expressions and facial tracks require a separate capability check for each actual face. Support them where feasible with verified character-specific mappings or derived assets. Do not disable otherwise compatible body playback because the face lacks expression controls, and do not apply guessed facial mappings to an unsupported face.
- Maintain verified, pending, unsupported and fallback entries for the three requested outfits and representative compatible default-human appearances. Preserve saved selections.
- Black Pearl is only one test case. Passing it, or even the existing nine-mesh test set, does not complete this broader compatibility requirement. Keep this scope active across sessions.

- Publish new shared assets under `/Game/CSS/Locomotion/`, with stable IDs and a versioned catalog. Use subfolders for clip families where useful; asset paths and menu names must not be the identity used for saved selection.
- Include Eve Default Idle and the seven released CSS poses: Seated Recline, Kneeling Bow, Low Lean, Deep Kneeling Bow, Reclining, Forward Fold and Side Kneel. Preserve their existing saved IDs through explicit aliases where necessary. Previously removed experimental ESAP choices are not silently reintroduced just because old trials contain them. All actual unique clips in the newly supplied archive set remain in scope.
- CSS must discover the shared catalog without representing it as a fake wearable outfit. Both CSS and CINE must consume the same resolved identities and compatibility information.
- Where an old outfit bundles a verified equivalent clip, show one menu choice. Prefer the compatible shared asset when installed and loadable; otherwise use the compatible bundled asset. Match by explicit stable identity and audited legacy aliases, not display-name similarity, archive load order or arbitrary substring matches.
- Shared-pack absence, removal, corruption, an incompatible skeleton or a failed asset load must not invalidate an old outfit's entire catalog. Preserve the saved choice, use its valid bundled fallback when one exists, and otherwise show an unavailable state with safe Default behavior. Do not retry a known failure or load the same asset every frame.
- Outfit-specific animation variants, including extra-bone or wing-driving versions, must retain their required behavior. They are not interchangeable merely because the ordinary body motion or menu name matches. Resolve an appropriate adapter/version or keep that outfit's specialized asset. Test Unholy Genessa's wing movement explicitly.
- Explicit user reminder on October 6: inspect the actual released and installed skeleton, mesh reference pose, virtual-bone mapping and post-process animation contracts for **all six Eve variants, Unholy Genessa and Commander White**. Record a compatibility table with asset paths, hashes and evidence. Test the shared library on each, preserving wing, hair, cloth, body physics, footwear and hand behavior. Do not infer compatibility from common body-bone names, an older skeleton-ownership note or one successful outfit. Use a verified adapter or specialized fallback when their rigs differ; keep legacy assets unchanged.
- Adding or removing the shared pack must preserve profiles, existing IDs, default idle, walk/jog/sprint selections and outfit-level/variant-level overrides. Do not edit or delete old outfit containers to remove duplicate bytes; CSS resolves the playable choice.
- Cache catalog merging and compatibility decisions outside the frame loop, retain asset references safely while playing, and invalidate them correctly on outfit changes, world changes and catalog reloads.
- Test old outfit alone, shared pack plus old outfit, shared pack plus several outfits, shared pack removed after saving, malformed catalog, missing asset, duplicate identity with conflicting definitions, and incompatible/specialized skeleton cases. Both CSS and CINE must behave consistently.

Initial code evidence: `Catalog::animation_options` in `native/src/data.cpp` currently selects a variant list or its outfit fallback; it does not merge a global library. `AnimationSet::parse` currently caps a slot at 64 entries and distinguishes Idle sequences from movement blend spaces. Implement shared discovery, deduplication and suitable browsing/playback deliberately; do not merely rename cooked paths or raise the bound without reviewing the UI and save limits.

## Supplied archives

Initial directory inspection found 13 archives, about 103 MiB on disk:

- `Bass Down Low Dance-2268-1-0-1760256228.zip`
- `Black Hole Dance-2851-1-0-1772785631.zip`
- `Catwalk Aniamtions-2717-1-2-1769488550.zip`
- `Come Closer Dance-2301-1-0-1760713954.zip`
- `D_DumbDumb_P-2545-v1-0-0-0-1764931729.7z`
- `D_Iris_Out_P-2587-v1-1-0-0-1768677296.zip`
- `Jang_Won_Young_Spin_P_V1-1861-v1-0-0-0-1755318813.zip`
- `atool/ATOOL making mod related files-1662-5-8-1768739366.zip`
- `atool/Better Jiggle Mod 2 ATOOL Patch-1662-6-0-1769752368.zip`
- `atool/HD-ATOOL-1662-6-1-1769772413.zip`
- `atool/HD-ATOOL-animtest-1662-1-0-1753606989.zip`
- `atool/HD-ATOOL-k2-1662-6-1-1769772439.zip`
- `atool/HD-ATOOL-k3-1662-6-1-1769772464.zip`

Archive names are labels, not evidence of contents, duration, author, skeleton compatibility or permissions. Record hashes, member lists, source readmes and actual asset metadata before assigning public clip names. Keep packages that override identical source paths mounted separately so one dance cannot silently replace another during export.

## Recover the proven workflow

Read these existing records before changing tools:

- `docs/development/eve-animation-sources.md`
- `docs/development/eve-idle-expansion.md`
- `docs/development/eve-v1.3.0-animation-repair.md`
- `docs/development/eve-idle-acceptance.md`
- `tools/MeshExport/ANIMATION.md`
- `tools/authoring-probes/animations/`

The earlier 700 through 713 floor-pose candidates came from ESAP. The ATOOL packages were also investigated, but the current folder's contents must be decoded independently. The earlier source mapping had unresolved added skeleton indices and positive non-unit scales. Do not reuse a guessed map or discard unknown tracks merely because some old clips looked acceptable. Preserve a full track audit and resolve dependencies from the actual source package or documented source basis.

Use the corrected virtual-bone mapping from the current released animations. Preserve Eve Default Idle, the seven retained CSS poses, and existing walk/jog/sprint selections. Verify all six Eve variants' actual post-process activation contracts, including footwear-specific graphs. A matching skeleton alone did not make all earlier idles work.

## Work sequence

Related fix, added by the user on October 5: after the current source/retarget
work, investigate questman's beta.10 report that stopping with all Eve movement
slots briefly flashes the jog/run-start back-foot lift. Inspect gait selection,
deceleration flags, animation phase and native stop transitions before choosing
a correction. Verify walk/jog/sprint stops, reversals, controller partial input,
mixed Default/custom slots, aiming/attacks and native movement. Preserve smooth
starts and avoid trading the pop for sliding or delayed idle. Record the cause,
fix and regression evidence. This is part of the animation work; the separate
rim-light, body-oil and Scyther tint requests remain in `docs/work-queue.md` for
later work.

1. Inventory and provenance: hash all archives; list safe member paths and sizes; extract into separate ignored working directories; inspect readmes; identify animation, skeleton, mesh, montage, curve, audio and support assets. Do not execute bundled programs. Record exact duplicates separately from same-path replacements.
2. Decode: identify the source engine format, mappings and required base-game dependencies using the existing source tools. Export complete animation metadata and tracks, duration, frame rate, root trajectory, scale, curves and notifies. Resolve skeleton names, parents, reference transforms and any added source indices. Retain unknown information and fail explicitly instead of fabricating a plausible pose.
3. Classify from motion: review all unique clips at their original speed. Distinguish looping idles, one-shot gestures, dances, catwalk cycles, starts/stops and companion tracks. Preserve the full original performance. Do not trim every source to a short idle or force a loop through mismatched endpoints.
4. Retarget in isolation: target the current CSS rig and compatible native rigs through the tested source-basis workflow. Preserve hand/finger articulation, hips, feet, root movement and ordinary body tracks. Correct virtual-bone mappings and contact with measured, documented changes. Keep source animation immutable and give derived assets unique paths and stable IDs.
5. Offline review: compare source and retargeted full-motion previews, ground contact, body/garment deformation, root trajectory, loop position and velocity where looping is intended, and transitions. Check several compatible outfits and proportions, including all six Eve variants. Preserve physics and footwear corrections.
6. CSS integration: add a discoverable animation library with readable names and appropriate categories. Looping choices may use the idle system where compatible; longer performances need suitable play/stop behavior. Preserve existing locomotion settings. Movement, attack, aim, dodge, menu exit, shell change, death/revive, travel and unload must interrupt or restore playback correctly. Do not globally replace native animations or leave weapons hidden.
7. CINE integration: expose the same catalog and stable IDs in scene controls. Support clip selection, play/stop, suitable looping, take start/end and restoration. If timing, seek or speed controls are offered, implement and validate them against actual animation playback. Use the existing neutral CSSX service bus; do not put CSS-specific behavior into CSSX core. Keep camera, HUD, materials and saved customization intact after cancellation or failed playback.
8. Cook and package: use the established Windows cook path with UE 5.6.1. Inspect dependencies and decode representative cooked frames against authoring data. Verify the full catalog resolves from the packaged containers and required dependencies alone. Build `CSS_LocomotionSharedAssets_P` with assets under `/Game/CSS/Locomotion/`; avoid duplicate copies per outfit. Ship its separate asset ZIP alongside independently versioned CSS/CINE updates and notes. Final version and release destination must follow the resulting integration, not an assumed CSS beta increment.
9. Live validation: install a backed-up candidate only when the game is closed, unless the exact asset/runtime update path has independently verified safe reload. Test every clip, both consumers, restoration and representative compatible outfits. Capture real frame-time and memory costs for selection, cold/warm playback, interruption and repeated takes. Fix regressions without claiming universal zero cost or zero bugs.
10. Delivery: retain a per-clip acceptance table with exact source, status and evidence. Package accepted files with short installation/dependency instructions, source credits and BBCode description/changelog plus checksums in the same version folder. Confirm source redistribution terms before preparing a public upload; if unclear, preserve the local port and document the unresolved release condition. No public upload is implied by this task.

## Engineering and source protection

- Original archives, existing mod source assets, released packages and the paused Proxima project stay unchanged.
- Work in the actual CSS checkout, following its existing no-worktree instruction. It was clean on `feature/cine-scene` at task start. Preserve the other session's CINE 1.1.0 work; do not switch branches, revert it, merge or publish without the appropriate instruction.
- Keep source extraction and animation authoring reusable independently of menu integration. Use small focused changes, meaningful regression tests and commits per context once verified. Do not commit source archives or bulk extracted files into the code repository.
- The existing exporter targets .NET 10; editor tooling is UE 5.6.1 Python/C++. Confirm installed/pinned versions before implementation. Do not upgrade the engine or exporter as an unrelated task.
- Initial system-drive space was about 26 GiB. List sizes first, avoid duplicating the full games or Unreal installations, and keep a storage estimate before large cooks. Do not delete unrelated caches, sources or rollback files under this goal.
- Do not move the user's character, switch desktop workspaces, trigger recordings or change saved appearance without a task-specific need and authorization. Offline work comes first.

## Completion evidence

- Every supplied archive has an immutable inventory and every actual unique character clip has an accounted-for disposition.
- All requested unique usable clips are ported in full; unresolved mappings or failed clips are explicitly incomplete, not counted as successful.
- Both CSS and CINE can select and play the catalog with correct interruption and restoration.
- Compatibility acceptance covers all six Eve outfits, both Unholy Genessa faces, Commander White, and representative default-game-human-skeleton appearances. Excluded projects are not release requirements.
- Other authors' CSS mods, non-CSS mods and native appearances using the default game human skeleton are covered where technically compatible, without an author-name or CSS-manifest restriction. Record representative playback/restoration evidence and any specific unsupported cases. Missing facial controls must not block compatible body playback.
- The separate `CSS_LocomotionSharedAssets_P` distribution supplies old and new clips under `/Game/CSS/Locomotion/`, with proven duplicate resolution, legacy saved-selection compatibility and missing-pack fallback. Old outfit packages remain byte-for-byte unchanged.
- Per-clip source/cooked comparisons and full-motion visual checks pass; documented live tests cover playback, contacts and regressions.
- Every delivered shared animation preserves source timing and motion, with any resampling justified by measured benefit. Native 60 Hz sources are not accidentally imported at 30 Hz. Check interpolation, transitions, contacts and loop seams separately from runtime frame rate. Existing bundled outfit assets remain unchanged.
- Existing idles, movement, aiming, abilities, physics, profiles and CINE takes remain functional.
- Eve's reported stopping pop has a verified diagnosis and correction, with the stated movement/interaction regression cases checked.
- Packaging is reproducible, dependencies and credits are clear, performance evidence is recorded and the user accepts the result.

## Current checkpoint

Goal created and initial source/previous-work inspection completed. `tools/authoring-probes/animations/inventory_library.py` produced `work/cine-animation-port/inventory.json`: all 13 archive hashes and member lists, 106,949,081 compressed bytes and 264,507,413 unpacked bytes. Original archives are unchanged.

The authoring-support archive includes a source Eve skeleton, Blender examples, FBX examples and Unreal assets. This may help resolve the earlier source-index ambiguity; its actual compatibility remains unverified. The Jiggle patch is a Lua support mod, not evidence of another character animation. Main/k2/k3 ATOOL archives contain the same container filenames with different bytes, so they must be inspected in isolation. The catwalk archive includes a source animation catalog.

All 95 files are now extracted in isolation with hashes. The dance/catwalk/animation-test archives contain 19 AnimSequence assets and two DataTables. Seventeen sequences decode strictly, including two static Eve placeholders and the real Adam/Lily catwalk tracks. DumbDumb and Iris Out reference seven bone indices absent from both the retail and supplied authoring skeleton; their full diagnostic tracks are retained, and strict export still refuses them. ATOOL main/k2/k3 metadata and Blender/FBX examples need further inspection.

60 Hz source preparation and rate-aware UE import/evaluation are implemented. Ten timing/interpolation checks pass. A real 381-frame source becomes 761 frames at 60 Hz with every original body/finger key retained and unchanged duration; the legacy 30 Hz preparation and per-track decode remain byte-identical for that fixture. See [the investigation](../../../../docs/development/shared-animation-library.md) and `work/cine-animation-port/sample60-verification.json`.

The user subsequently clarified that 60 fps was a question about the proper approach. Preparation now preserves the verified native sample rate by default, with optional tested 60 Hz resampling. The first native-30-Hz Bass Down Low candidate retargets successfully and passes a fresh-process comparison of all 381 frames and 388 evaluated target bones. Its fitted-mesh preview identifies shoe/floor contact correction still needed. It remains a private candidate, not accepted or installed.

Full ATOOL main/k2/k3 readback is complete: two identical static support sequences, with differences in Blueprint support packages. Blender/FBX example actions are inventoried but not yet accepted. The user now includes all 31 referenced photo poses and feasible support for the 21 facial expressions. All 52 base-game packages load and all 31 photo poses pass strict bone-track decode. The retained Eve authoring body matches all 99 facial curve names (two differ only in capitalization), but its current export filters out the face set, so released-mesh checks remain necessary.

No new animation has been cooked or installed. This task has not changed CSS runtime or CINE code; the other session's CINE edits remain separate. Shared-pack compatibility and facial controls are still requirements, not implemented features.

Released Eve v1.4.0 inspection confirms all six variants have 22 body/hand
morphs and no matching facial morphs. Facial support therefore requires derived
Eve mesh assets as well as CSS/CINE controls. All 21 source expressions are
constant curve poses; preserve their signed weights. The source Neutral pose
has seven nonzero curves, so offer it separately from restoring the outfit's
previous face state. Preserve body controls and hand correctives when switching
or clearing expressions. Evidence is recorded in
`docs/development/shared-animation-library.md` and the private facial audit.

The Blender source examples now have three additional private candidates,
preserving evaluated NLA timing and constraints: 200 at 25 Hz, 801 at 60 Hz,
and the retimed camera-scene body performance at 60 Hz. All 1,963 target frames
pass saved/repeated raw-pose comparison. A source-import finalization repair
also preserves the native compression sample rate. Fresh-process sampling
checks pass on those candidates and the earlier L2/L3 batches, covering 98
saved source/target assets. These checks do not establish visual acceptance.
The 801 source has a long final hold, and the camera-scene skinned review needs
to account for its large root translation and numeric reconstruction error.
No new assets are cooked or installed; shared runtime integration, facial
support and the stopping-transition correction remain unfinished.

2026-10-06: installed-package verification now covers all nine mesh bindings
across Eve 1.4.0, Unholy Genessa 0.2.0 and Commander White 0.0.6-dev. Their
386-bone base references and nine virtual-bone definitions match exactly;
Genessa adds four wing bones. Every current post-process graph exposes the
CSS idle contract. See `docs/development/animation-library-rig-compatibility.md`.
This is static compatibility evidence, not acceptance of all new clips.

The stopping candidate now retains Walk/Sprint during verified deceleration
when the game's gait flags clear before speed reaches Idle. Portable tests
(80 runtime checks and the animation/skeleton suites) and the Windows core
build pass. No DLL was installed. The reported jog pop, cycle phase and full
transition matrix still require live verification. Details and remaining cases
are in `docs/development/eve-stopping-transition.md`.

Shared-library discovery is implemented as a separate CSS.Package kind, with
strict metadata validation and no fake outfit. Conflicting installed library
identities and aliases reject all involved libraries independently of outfits.
The metadata resolver orders exact-rig, audited compatible-rig and exact bundled
fallback candidates while preserving specialized outfit choices. The Windows
build and discovery regression checks pass. CSS menu/playback wiring, loaded
asset validation/failure caching, CINE service integration, facial support and
the actual shared cook remain unfinished. See
`docs/development/shared-animation-catalog.md` for the implemented boundary.


2026-10-06 runtime/menu checkpoint: CSS now merges shared idle and movement
choices into valid outfit variants, preserving audited bundled IDs when saving.
Menus are cached; loaded assets are validated and retained; failed loads are
cached until selection or owning objects change. Missing player meshes defer
shared playback. All 19 portable CTest suites, 65 packaged-library checks and
the Windows core build pass. No DLL was installed. Live restoration/performance,
one-shot controls, CINE integration, facial support and shared cooking remain
unfinished. DumbDumb and Iris Out still need seven source bone mappings.

Facial asset progress, 2026-10-06: all six current Eve meshes now have separate
editor candidates with 99 expression shapes added to their existing 22 morphs.
Fresh-process readback passes on all six, preserving base geometry, old morph
attributes, skeleton, materials, physics and post-process bindings. All 21 poses
and Reset have Blender review renders. Nothing is installed or cooked. Facial
runtime/UI/CINE support and full deformation/performance review remain open.
See `docs/development/eve-expression-candidates.md` for exact evidence.

DumbDumb/Iris Out investigation: both clips declare skeleton GUID
`64ACCBA8-42A4F5D4-E692FAA9-08A5F607`, different from the supplied authoring
skeleton. The supplied 200/801/camera Blender rigs do not identify the seven
extra indices. Preserve their full track audits; do not invent names or silently
omit those tracks. Matching author skeleton/source data remains unresolved.

Recording-priority update: the user requested early deployment of the stopping
fix. Installed CSS core `9b983b7441557c17` after confirming the game was closed,
with previous DLL/selector backed up and saved settings unchanged. Live visual
acceptance is pending. No new shared animation assets are installed. All 57
resolved sequences have now been duplicated into `/Game/CSS/Locomotion/Eve/`
with exact authored-key comparisons; fresh reload and cook checks are next.
The two unresolved dances and the remaining full goal stay active.

Fresh-process reload checks passed for all 57 shared sequences, preserving every
authored transform and native timing. The recording trial core is confirmed
loaded in CSS.log. Cooking was not started while the user launched for recording.

Resumed 2026-10-06: all 57 sequences now cook successfully into a private shared
trio (7,012,825 bytes). Independent readback and 273 sampled raw/cooked pose
comparisons pass, including nine virtual tracks and explicit ACL wrap endpoints.
The diagnostic reader was corrected to honor sequence-owned reference poses.
The pack still requires Shared/SKEL_Base; this check used installed Eve 1.4.0.
Nothing was installed. See `docs/development/shared-animation-cook.md` for
limits, dependencies and evidence. One-shot/pose playback, facial runtime,
the two unresolved dances and live review remain active work.

Five explicit-time performance adapters now compile and pass fresh-process
structure/default checks: Eve, Eve bikini, Eve knitwear, Genessa and Commander
White. Original controls, nodes, pin defaults and skeletons are preserved.
They are separate editor assets, not installed or runtime-integrated. Next
verify active/held/disabled evaluated poses, then add owned CSS/CINE playback
and restoration. See `docs/development/animation-performance-layers.md`.

All nine meshes passed seven offline component/post-process evaluation cases
each (63 cases). This verifies held body poses and inactive restoration, not
world cloth simulation or live acceptance. Sources retain their input hashes.
The explicit-time clock passes 36 checks; all 20 host CTest suites pass.
Runtime binding/ownership, full moving clips and CSS/CINE controls are next.
No additional assets or DLLs have been installed.

Latest checkpoint, 2026-10-06: the explicit-weight adapter revision passes
81 cases across all nine meshes. The full Black Hole clip compares 1,201 poses
per mesh at 60 Hz (10,809 total); earlier two-consumer pose wiring was rejected
after its zero-weight test failed. All original source files retain their hashes.
The five adapters cooked successfully. Independent cooked readback confirms
their existing CSS defaults, rig settings, transferred bones and pose links
match the installed source graphs. `pack-with-adapters-v2` contains 57 sequences
and five adapters, with 62 byte-exact export round trips and version-bound
adapter metadata. Its dependency check currently mounts all three outfits.

The native `css.customize` service now stages canonical-ID playback, loop/once/
hold, explicit time and blend weight, pause/seek/rate/stop and transient tokens.
It binds only a verified source outfit revision, copies CSS/physics settings,
preserves native animation-layer assignments and registers rollback before the
post-process setter. It restores the original graph on release. No main graph
replacement, shared asset mutation or bone-buffer/accumulator writes are used.
This is a compiled candidate, not live-accepted code. Recreated idle phase,
world cloth, interruption, cold/warm costs and dependency closure need checks.

Windows core builds successfully. All 21 host CTest suites pass, including 45
clock checks and 32 session/resolver checks. Synthetic package discovery passes
66 checks; the actual combined pack plus Eve, Genessa and Commander White passes
75 checks, including all five source-revision resolutions. Fixed a pre-existing
FileWriter test that incorrectly assumed one worker scheduling order; production
writer code was unchanged, and the test passed 20 repeated runs.

Next finish CSS browsing/start-after-menu-close and CINE take controls using the
shared service, then a backed-up live candidate. Facial runtime/cook/controls,
the two unresolved dances, complete visual review and release preparation are
still outstanding. The other session's CINE edits are preserved. Nothing new
has been installed during this resumed work. See `performance-service.md` and
`animation-performance-layers.md` for the exact implementation boundary.

## CSS/CINE performance trial, 2026-10-06

CSS now has Locomotion > Dances & Photo Poses. Its searchable picker groups
libraries, distinguishes unsupported outfits and queues playback only after the
native menu closes. The queue expires after five seconds and rejects any player,
appearance, outfit, variant or catalog change. This is committed as `83c1302`.
The native service lists library identities and separates browser compatibility
from the gameplay readiness check at Start.

CINE's catalog/owned-session client and its regression tests are committed as
`c62e769`. The actual controller/menu integration is implemented and tested in
the working tree. Those four files also contain the other session's uncommitted
work, so they have not been indiscriminately staged. Their pre-integration
copies are under `work/cine-animation-port/cine-before-performance/`; the exact
new delta is `work/cine-animation-port/cine-performance-integration.patch`.
Preserve both contributors' work when committing the integration.

CINE exposes Refresh animations, Library, Animation, speed and start time.
Playback begins at Roll and ends with the take. Natural animation completion
does not cut a longer camera take. F9, menu opening, travel, gameplay interruption,
setup failure and shutdown clean up ownership. A stale take cannot stop another
owner. Off preserves ordinary takes without per-tick service calls. Stationary
presets are required; route walking, combat and look-changing tracks are rejected
explicitly while a full-body performance is selected.

Validation: 21 CSS host suites pass, including 43 session/resolver checks.
All three CINE suites pass, including 29 performance-client checks and 206
director checks. Both Windows DLLs build. The actual shared pack plus the three
installed outfit packages passes 75 catalog/adapter checks; all source package
hashes and the 62-package roundtrip remain valid.

Installed a private trial with the game closed, using
`tools/authoring-probes/animations/install_performance_trial.py`.
Receipt and backups: `work/cine-animation-port/live-performance-1/`.
CSS core: `css_core-performance-8fe5a0909c5a0833.dll`.
The 57-sequence/five-adapter shared pack and CINE DLL/menu are installed.
Permanent loaders, UE4SS, saved state and all authored outfit packages remain
unchanged. No game launch, hot reload, movement, camera or recording input was
sent. The game was still closed at the last process check.

Asked the user to launch with Eve, Unholy Genessa or Commander White and choose
Black Hole in CSS's new browser. Await that live result. Do not label the trial
accepted or release-ready. Still required: gameplay/physics/restoration checks,
all-clip full-motion review and live performance measurement, facial cook/runtime
and controls, DumbDumb/IrisOut's unknown source tracks, and final distributions.
All 22 frontal facial previews have now been inspected; this does not establish
side-view contacts or cooked facial playback.

## Facial shared assets checkpoint, 2026-10-06

Continued offline while the installed performance trial awaits the user's check.
All six Eve facial candidates are copied into `/Game/CSS/Locomotion/Faces/`,
fresh-load verified and cooked for Windows. An independent CUE4Parse readback
passes all six: original render geometry, skinning, bindings and the 22 existing
body/hand morphs are unchanged; the 99 added facial shapes decode with valid GPU
deltas. The private diagnostic container is not installed. Existing outfit
containers are unchanged. See `docs/development/eve-expression-candidates.md`
for evidence and the precise limits of these checks.

Added optional facial-rig metadata to the shared animation catalog, with signed
weights, exact outfit revision/mesh matching, case-insensitive morph-name
collision rejection, and order-independent catalog conflict checks. The generated
real catalog has 21 expressions and six bindings. All 22 CSS host suites pass;
40 facial checks also pass against the generated catalog.

Next implement owned expression activation and restoration, persisted selections,
and CSS/CINE controls. Keep the installed performance trial stable until its live
result arrives. No facial expression has been installed or accepted in game.
The other unfinished animation and delivery work above remains in scope.

Also added optional `Customization.expression` persistence and catalog lookup by
the actual outfit variant and verified container revision. Old/default settings
are unchanged; profile snapshots retain the selected ID through missing-pack
fallback, and palette changes preserve it. All 22 host suites pass; 56 checks
pass against the actual facial catalog. Runtime activation and menu controls are
still pending, so no expression has been applied to the game.
The Windows core builds successfully. It is not installed; the last process check
found the game closed and the previously installed performance trial unchanged.

## Facial activation and CINE checkpoint, 2026-10-06

Implemented revision-bound facial mesh activation, independent facial morph
ownership/rollback, CSS's searchable expression picker and Reset all handling.
CINE now has an expression catalog and a take-owned expression helper, with menu
integration, pre-take acknowledgement, cleanup retries and restoration tests.
Default As set in CSS adds no expression service calls. Look-changing takes must
use that default until cross-look expression ownership is supported.

Both Windows DLLs build; all 22 CSS host suites and four CINE suites pass.
CINE has 34 expression client checks and 231 director checks. The actual facial
catalog passes 75 checks. The combined shared candidate at
`work/cine-animation-port/shared-v1/pack-with-faces-v1` passes all 68 package
roundtrips and independent facial decoding on all six Eve meshes. The trio is
315.6 MiB. It is not installed or live-accepted. The installed performance-only
trial and pending Black Hole playback question remain unchanged.

Commit only owned native/helper/tooling changes. CINE's mixed integration delta
is saved in `work/cine-animation-port/cine-expression-integration.patch`; its
pre-edit copies are in `cine-before-expressions/`. The earlier performance
integration patch remains separate. Do not discard the other session's edits.

Next: live-check the existing performance trial, then install a backed-up facial
candidate when appropriate and validate all six variants, expression reset,
profiles, body/movement/physics and CINE restoration. Facial source-to-cooked
delta error, side contacts and costs still need evidence. Continue the unresolved
source mappings and full-clip acceptance work; this checkpoint does not complete
the goal.

Further offline facial evidence, 2026-10-06: all six combined-package meshes
now pass an independent source-to-cooked comparison of every facial delta and
all 21 weighted expressions. Maximum combined deformation difference is
0.217 mm (Kiss); mapped base positions match exactly. Profile and three-quarter
renders of all 21 expressions plus Reset were inspected, with no obvious
facial tearing in those views. Source hashes remain unchanged. See
`docs/development/eve-expression-candidates.md` for bounds and artifacts.
No candidate was installed and the game is closed at the latest process check.
Live expression/performance validation and the rest of the full goal remain open.

Motion-review checkpoint, 2026-10-06: generated 1,075 skinned replay frames for
all thirteen moving L2 clips and timestamped sheets with explicit endpoint gaps.
Inspected every Black Hole sample and wider overhead-swing views for ATOOL
Performances 5/8. Confirmed Black Hole's 15.7-second lift exists in the source;
do not flatten it. Fixed the source-review tool's eight-second truncation and
unconditional looping. See `docs/development/shared-animation-motion-review.md`
for measured bounds and the exact remaining review scope. No runtime or asset
installation changed. Better Jiggle's available local ZIPs contain scripts, not
the missing skeleton assets, so the two extended-skeleton dances remain open.

The sampled silhouette pass now covers every rendered sample of all thirteen
moving L2 clips. Remaining contact, prop/support and secondary-motion questions
are recorded per clip in the motion-review document. A new read-only endpoint
audit measures all thirteen clips and preserves their hashes. Large endpoint
orientation gaps confirm several dances need one-shot playback. Catwalk matches
its endpoint pose closely but still has a 49.913 cm/s local translation velocity
gap, so it is not accepted as a seamless loop. Four diagnostic regression tests
pass, including equal endpoint poses with unequal velocity, quaternion sign
equivalence and invalid snapshot rejection. Native-speed and live checks remain
open. The game is closed, and the installed trial's DLL selectors, DLL hashes and
shared containers still match its deployment receipt. No installation changed.

Combined-trial checkpoint, 2026-10-06: installed the verified facial/body shared
pack and matching CSS/CINE DLLs while the game was closed. The new installer
requires the exact previous trial and passes five filesystem failure/rollback
tests. All 83 real-pack catalog checks passed before installation. Independent
post-install verification confirms selectors, DLLs, containers, saved state and
the retained old pack. Receipt: `work/cine-animation-port/live-facial-1/deployment.json`.
Core is `css_core-facial-6b9cf837e4c2239c.dll`, CINE is
`cine-facial-bb2d36a5ca792bbf.dll`. The current user check is Smile then Original
on Eve Black Pearl; the earlier performance-only prompt is superseded.
Live body/facial playback, other variants, contacts, restoration and performance
remain unverified. No goal requirement or unresolved source clip was removed.

## October 7: optional author head received

The user supplied `Jail3_Head` and `D_Cinderella_Main` under the reference
animation directory. See `docs/development/shared-animation-author-head.md`
for their hashes and current evidence. The optional head supplies all nine
missing facial shapes for DumbDumb, Iris Out and Spin. The transfer candidate
retains 105 existing Eve facial shapes and adds ten, including Cinderella's
surprise control. All six Eve meshes pass import and fresh editor checks;
shared staging, Windows cooking and complete six-mesh cooked readback pass.
The verified face catalog retains 21 expressions and now prepares all five
earlier performances. Complete facial performance review, combined packaging
and live checks remain separate gates. This candidate is not installed.

The three earlier facial performances now prepare without missing controls,
retaining 2,616, 1,892 and 426 source keys respectively. Combined-pack and live
acceptance remain pending. Cinderella adds one newly inventoried body clip
(8,381 samples, 139.66667 seconds) and one facial performance. Its body is not
retargeted or integrated. Keep it explicitly unfinished, outside prior counts.
The optional head does not resolve the seven unidentified source bone indices.
No excluded outfit work, game input or installation was performed.

## October 7: recovered faces merged into the shared candidate

`eve-face-jali3-trial-v2` keeps 62 body/movement IDs, 31 photo poses, five
outfit adapters and four native locomotion choices. It replaces exactly six
Eve facial exports and preserves the other 90 exports, including Commander
White. Eve retains 21 expressions and now has five prepared performances.
Actual combined/pack-only loader checks pass 1,131/1,119 assertions. The first
candidate exceeded the manifest size limit due to indentation; compact JSON
fixes packaging without changing CSS's limit or runtime. Seven preservation
tests and three focused host suites pass.

Full-duration facial comparison videos are encoded for DumbDumb, Iris Out and
Spin; sampled source/target views look consistent. Complete visual and live
acceptance remain. The candidate is not installed. Verified duplicate v1
payloads were replaced with references to identical v2 files, reclaiming
1,084,314,122 bytes. See `shared-animation-author-head.md` for exact evidence
and the remaining Cinderella, source-binding and outfit facial work.

Cinderella continuation: the complete 8,381-sample body audition now exists
in `retarget-cinderella-c1`. A fresh conversion matches all 3,251,828 authored
transforms exactly. Source accounting preserves all 155 tracks, including four
unidentified bindings. Sampled skinned review found 1.408/1.697 cm right-shoe
penetration at the first/last poses, so this is not accepted or cataloged.
Next is source-relative contact correction with non-leg motion preserved,
then dense review, cooking and outfit checks. Do not count it in the existing
62 catalog IDs or five Eve facial performances. All current jobs are terminal.

Cinderella contact follow-up: C6 passes visible Black Pearl shoe contact at
all 8,381 authored keys and repeats all 3,251,828 transforms exactly, with
non-leg local poses unchanged. It still has knee-direction discontinuities
and is not accepted. C3 incorrectly drove correction from hidden alternate
foot sections and is rejected. The C7 limb-relative diagnostic closely retains
source knee flexion but penetrates the floor, so investigate hip placement
together with leg proportions next. Do not stack more lifts onto C2 or relax
the existing knee repair guard. Full findings and paths are in
`docs/development/shared-animation-author-head.md`. No game files were changed;
all C2-C7 diagnostic jobs finished. Two byte-identical source-leg captures now
reference the retained C2 capture, removing 11,233,280 duplicate bytes.


Cinderella C9 continuation: a position-only pelvis clearance bake now preserves
all joint rotations/scales and every non-pelvis authored transform. Exact fresh
repeat, zero-lift preservation, full visible-footwear contact and all nine
virtual-bone checks pass across 8,381 keys. The former 135-degree knee step is
14.87 degrees. Review windows are rendered; full-motion review, independent
cook/readback, outfit adapters and gameplay remain pending. Twelve Cinderella
facial curves (3,897 keys) are prepared exactly but not cataloged. Existing
pack counts have not changed. No game files were installed. See the author-head
investigation and `retarget-cinderella-c9/` for evidence and limitations.


C9 Windows cooking, package round-trip and full independent ACL readback pass.
The dense 240 Hz scan then finds seven left-shoe samples below -0.1 cm
(worst -0.458656 cm), so C9 is not accepted. C10 is being baked with a measured
pelvis-only clearance addition of at most 0.463657 cm. Preserve the unchanged
joint rotations and error limits; verify the new cook before cataloging it.
