# Goal: Eve, Unholy Genessa and Commander White rig and physics repair

Prepared 2026-10-08. **Status: active, baseline and migration audit in progress.** See [implementation status](three-outfit-rig-physics-status.md) for current evidence, completed work and remaining checks.

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
- Do not impose an arbitrary attachment/bone limit such as 128. Follow verified engine constraints and actual asset counts, with explicit diagnostics for unsupported data.
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
- Recheck every existing body morph and permitted combinations at neutral and supported limits. Garments and fitted ornaments must follow the same intended surface without separation or clipping introduced by the repair.

**Exit:** before/after deformation evidence and import/cook readback show the intended improvement, with no unexplained skeletal, morph or attachment changes.

### Unholy Genessa facial-expression support

Implement and validate facial-expression support for both Unholy Genessa variants through CSS and CINE. Inspect each active face mesh, facial bones/morphs, eye and mouth geometry, materials and existing playback bindings first. Add the missing character-specific controls or adapter in the Unholy Genessa mod; keep reusable expression assets in `CSS_SharedAssets_P` and general playback logic in CSS/CINE according to the ownership rules above.

Animations containing facial tracks must drive the expressions in sync with body playback, including Iris Out and other existing clips with verified facial data. Map controls to Genessa's actual face rather than assuming Eve's morph names or bone transforms apply. Preserve Genessa's identity, neutral face, eye visibility/glow, face/body alignment and customization. Check eyelids, eyeballs, jaw and lips for collapsed or missing geometry.

Handle clip loops/end, movement, attack, aiming, abilities, outfit changes and menu/world transitions without leaving a stuck expression or blocking gameplay. Restore the appropriate neutral or game-controlled face on interruption. Body-only clips remain usable without fabricated facial tracks. Verify compatibility with Faithful/Stray forms and customized doubles without disturbing their ghost materials or pose.

**Acceptance:** both variants visibly play synchronized expressions in CSS and CINE, restore correctly after interruption, and pass eye/material/gameplay checks. Record the tested clips and any unsupported controls explicitly. Eve and Commander White facial playback must remain working.

## Phase 3: collision and secondary motion

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

**Status: requested, not implemented.** Continue the current Genessa cloth/body-shape repair and existing rig work first. This addition does not change their priority or the nine-variant repair baseline.

Use the references in `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/reference/new-eve-outfit`:

- `Peony Better Cleavage-1851-v1-0-1755197848.rar`
- `DOAXVV Slipstream CNS-3108-1-0-1778702686.zip`

Add both outfits to the current Eve mod alongside its existing six outfits. Inspect the archives before deciding the final component breakdown or display names. Preserve the reference designs and keep original inputs unchanged.

Each new outfit must have modular, individually removable garment components through CSS customization, allowing the outfit to be undressed, plus **five distinct template color palettes per outfit (ten total)**. Use the established customization and saved-profile behavior. Validate component combinations, palette application, body-shape fit, skin weights, collision, garment physics and animation/facial compatibility in both preview and gameplay. Apply the same performance and regression gates as the existing outfits.

Keep outfit-specific meshes, materials, physics and palette definitions in the Eve mod under `/Game/CSS/eins0fx/EveSB/...`; put only genuinely reusable assets in `CSS_SharedAssets_P`. Make supporting CSS changes where needed while preserving existing controls and saved selections.

**Acceptance:** both outfits are selectable, modular visibility and all ten palette templates work and persist, and fit/physics/gameplay checks are recorded. Track them separately from the nine existing repair variants. Release inclusion/version remains to be settled when this queued work is reached; do not silently change the coordinated release targets or report these outfits as completed.

## Low-priority follow-up: Trebuchaxe sidearm visibility

**Status:** user-supplied report, not reproduced or diagnosed. Investigate and fix after the primary rig, facial and animation work; this does not change the current priority.

DirektorSvemira reported on 2026-10-08 at 00:05 (report timezone unspecified) that CSS sidearm visibility set to **Only when in use** produces a hovering axe above the Trebuchaxe. The offset persists when the weapon turns sideways for a charged shot. The reporter says gameplay is unaffected and no other installed mod alters the weapon or its hiding behavior. CSS causation and any Tarstone interaction remain unverified.

Reported sidearm Tarstones, top slot to bottom:

1. Tarred Fragment
2. Monarch's Vestige
3. Squall Stone

When this item is reached, reproduce with that loadout and visibility setting; compare other visibility modes, no Tarstones and individual stones. Inspect the weapon's attached components, sockets, relative transforms and visibility propagation through equip, hide/show, aim and charged-shot transitions. Determine which object is hovering before changing attachment or visibility logic.

**Acceptance:** no unintended hovering axe through those transitions; the intended weapon and effects remain visible when appropriate, charged shots and aiming work normally, and other sidearms/visibility modes retain their behavior. Record reproduction evidence, the actual cause and regression results. Keep this item queued if not yet investigated; do not describe it as fixed merely because the main goal is ready.

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

Completion requires all scoped variants accounted for, the two queued Eve outfits and their ten palettes completed or explicitly deferred by the user, Unholy Genessa facial support accepted, agreed defects repaired or explicitly deferred, shared-pack organization accepted, locomotion review and remaining native-human conversions accounted for, regression results recorded, source-to-package provenance complete and a concise list of remaining limitations. Do not mark the goal complete from a Blender preview alone.

**First implementation steps when activated:** verify the installed gameplay-interruption baseline, pin the nine-variant/current-facial manifest, then capture matched poses to prioritize the first demonstrated deformation defect.
