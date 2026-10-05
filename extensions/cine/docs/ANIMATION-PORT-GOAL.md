# Stellar Blade animation library for CSS and CINE

Updated 2026-10-05. This is a new task. Protocol Echelon Proxima remains paused.

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
