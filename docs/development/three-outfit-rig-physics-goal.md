# Goal: Eve, Unholy Genessa and Commander White rig and physics repair

Prepared 2026-10-08. **Status: active, baseline and migration audit in progress.** See [implementation status](three-outfit-rig-physics-status.md) for current evidence, completed work and remaining checks.

October 12 authorization: improve CSS core where needed to support the new
outfits. Peony remains the current priority. The first addition is authored
material settings in palette templates, including textures, while preserving
manual customization, saved profiles and Original restoration. Keep optional
glowing, flowing trim in the Peony integration and test the resulting behavior
in both preview and gameplay before accepting it.

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

The subsequent saved-setting check passes 774 restoration cases and 256 old
garment-toggle combinations against the installed V2 metadata. Compatible
settings survive, retired palettes fall back to Original, and obsolete controls
are dropped. Evidence is in `work/three-outfit-rig-physics/peony-v2-upgrade1`.
This does not replace the remaining native appearance and gameplay checks.

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

**Status:** queued from the user's October 11 report, not reproduced or diagnosed. Keep Peony as the current priority. The user reports that after using a different save game, if the last **MISC > Genessa Form** choice was **Stray**, switching to **Faithful** may no longer work. The exact sequence and cause are unconfirmed.

Reproduce by selecting Stray on one save, returning to the menu and loading another save, then requesting Faithful. Compare switching saves within a session with a full restart, and compare previous Auto/Default and Faithful choices. Check whether the destination save's gameplay shell is actually Genessa and whether the character is in a state where the native transition is permitted. Observe the actual form, requested form, UI selection, transition completion and ability state separately.

Inspect form-control state across save/world/player replacement, including stale actor references, pending requests, cached shell/form identity and whether a saved preference is mistaken for the newly loaded character's actual state. These are investigation targets, not established causes. Preserve each save's progression and native form rules; do not force an unsupported shell into a Genessa transition or edit save data to work around the symptom.

**Acceptance:** after loading another save, supported Faithful/Stray requests work when the native transition is valid, the UI reflects the current character, and Auto/Default returns control to the game. Verify both directions, a fresh launch, existing single-save behavior, doubles, customization and animation interruption. Record any legitimate game restrictions and the actual cause before describing this as fixed.

## Low-priority follow-up: Trebuchaxe sidearm visibility

**Status:** user-supplied report, not reproduced or diagnosed. Investigate and fix after the primary rig, facial and animation work; this does not change the current priority.

DirektorSvemira reported on 2026-10-08 at 00:05 (report timezone unspecified) that CSS sidearm visibility set to **Only when in use** produces a hovering axe above the Trebuchaxe. The offset persists when the weapon turns sideways for a charged shot. The reporter says gameplay is unaffected and no other installed mod alters the weapon or its hiding behavior. CSS causation and any Tarstone interaction remain unverified.

Reported sidearm Tarstones, top slot to bottom:

1. Tarred Fragment
2. Monarch's Vestige
3. Squall Stone

When this item is reached, reproduce with that loadout and visibility setting; compare other visibility modes, no Tarstones and individual stones. Inspect the weapon's attached components, sockets, relative transforms and visibility propagation through equip, hide/show, aim and charged-shot transitions. Determine which object is hovering before changing attachment or visibility logic.

**Acceptance:** no unintended hovering axe through those transitions; the intended weapon and effects remain visible when appropriate, charged shots and aiming work normally, and other sidearms/visibility modes retain their behavior. Record reproduction evidence, the actual cause and regression results. Keep this item queued if not yet investigated; do not describe it as fixed merely because the main goal is ready.

## Low-priority follow-up: Naylshotte Tarstone effects while hidden

**Status:** queued from the user's October 11 report, not reproduced or diagnosed. Investigate alongside the Trebuchaxe visibility issue after the current Peony work. A shared cause has not been established.

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
