# Genessa doubles status

Session 4, October 5, 2026. Branch `feature/genessa-doubles`.

## Current state

The runtime now selects shared ghost materials from the source's actual parent
chain, graph StateId, static switches, blend mode, mask threshold and culling.
It binds native noise textures and copied source textures after uniform copying.
The policy accepts all 102 independently decoded Eve/Commander White material
configurations; unsupported graph revisions/permutations retain the native double.
The native Uber layer is checked by its path and StateId. Its unreflected Tree
layout is bounded against UE 5.6.1's header and the game dump (0x28 bytes), and
only empty array counts/root are read. No tree allocation is dereferenced.

`AstralDoubles` now connects the bounded player-owned spawner reader to private
materials, a CopyPose visual body and leader-pose modular followers. It copies
current morphs, hidden sections, overlays and supported post-process inputs.
The native combat mesh, animation instance, weapons and collision stay in place.
Discovery runs at 20 Hz; active fades follow the original MID each frame.
Appearance revisions, reuse, changed native instances, disable and cleanup are
handled. Source visibility/physics are recaptured on activation, including after
leaving Inventory. Failed activations use the original double without retrying
every frame. Development builds start disarmed; `astral_trial` explicitly enables
the coordinator for live checks. `astral.adapters.probe` only selects/loads assets.
Frame profiling now includes a separate Astral phase.

The coordinator now records a pooling edge seen by its per-frame fade pass in
the lifecycle registry. Without that handoff, a double reused before the next
50 ms discovery snapshot could retain its old activation and fallback latch.
The regression covers immediate reuse, repeated deactivation and a recycled
object index with a different serial. The lifecycle test and both Windows builds
pass after the correction. This still needs actual pooled-summon verification.

Both Windows runtime builds pass, and all 18 host tests pass. Catalog generation
reproduces the checked-in definitions exactly. These are compile/policy checks,
not proof of reflected calls, rendering, combat contacts, cleanup or live cost.
Body companions currently reproduce albedo and coverage, not the original Uber
PBR/blood treatment. Body offsets and foreign accessory layouts still need live
comparison. Broad native VT/NPC support remains a fallback, not full parity.

The current asset candidate is `/mnt/eins0fxE/CSS-work/genessa-doubles/material-pack4`.
It packages the same 28 verified exports as pack3, using the distinct container
name `CSS_AstralSharedAssets_P` (ID 422281afa3183dc6). Independent shader and pose
readbacks pass again. The installed legacy `CSS_SharedAssets_P` from September 24
contains 28 Eve skeleton/animation assets, not these ghost materials; keep it intact.
The source manifests remain `visuals1/shared-materials.json` and
`shared-material-variants.json`. No recook was needed for the container rename.

The author confirmed closure again and the process check returned no game PIDs.
The passive trial is now installed. The selected core is
`css_core-astral-trial-7ae89dcae8e2d680.dll`, SHA-256
`7ae89dcae8e2d68087c6cb86c8ba02b1bca6984b49df854b95331e5d0f836a23`.
The receipt and previous selector/core backup are in
`work/genessa-doubles/runtime-trial1`. Independent readback verified the selector,
core, all three new package files and all 12 protected files. The existing loader,
UE4SS, saved state and legacy shared package hashes are unchanged. No game launch
or input was sent. The trial remains disarmed on boot and is not live-verified.

ABI clarification: `core.json` uses core-table ABI 1 (`css_abi`), while the
loader provides host ABI 2 (`css_host_abi`) through `css_get_api2`. CSSX's
extension interface separately uses `CSSX_ABI 2`. The installed CSS log already
records host ABI 2 background writes. This deployment did not replace the loader
or downgrade any of these interfaces.

Next: ask for actual gameplay shell Genessa equipped in the world, with Unholy
Genessa selected. Verify the loaded core, then run source/adapter and hidden
visual checks before arming the trial. Verify Faithful and both Stray doubles,
skin/body controls, hair/fabric/wing rendering, attack grips, pooling and cleanup.
Profile active summons, then exercise existing aiming/locomotion/gate/profile
checks. Both releases remain unmodified. EveTest remains queued.

Commander White's native eye refraction now passes 524 DXIL arithmetic cases
and 14 Vulkan captures. Zero surface opacity still refracts, but zero native
ghost fade removes it completely. The diagnostic comparison was inspected.
That family is included in the shared package; full faces and live doubles
remain unverified.

The paragraphs below record earlier milestones and their original scope.

Native body ghost parents now compile and pass 26 HDR fixture captures across
Faithful and Stray. The checks cover native alpha-channel behavior, the exact
clip boundary, opaque-versus-masked handling, tint, animation and zero/partial
fade. These parents use albedo detail under ghost shading; native reflection,
blood/frost and death displacement are not reproduced. Full-character fidelity,
effective permutation selection and Windows cooking remain pending. Both
authoring and rendering exited zero, and the comparison image was inspected.
See [material support](genessa-doubles-materials.md). No installation changed.

Genessa's eye/smoke reconstruction passes 3,096 shader arithmetic and UV cases.
All four private parents compile in UE 5.6.1. Forty HDR captures on SK_EveW3's
authored eye/smoke sections pass both forms' tint, color/intensity, independent
animated clocks, partial fade and exact zero-fade checks. Protected hashes are
unchanged and the isolated editor exited zero. Face occlusion, DX12 and live
integration remain unverified; body integration and Commander White's native
eye family still need work. See the October 5 sections in material support.

The lit-surface experiment completed but is unsuitable for shipping. Run 6
captured 156 EXRs across seven material families and both forms. All 14 groups
pass composition, coverage, controls and fade checks. Native tint fails for
all seven Stray groups and Commander White's Faithful hair: source lighting
overpowers the ghost emission. Original source hashes are unchanged. The
editor exited zero and its tool session is terminal.

The subsequent `native_filter` experiment also completed with exit zero.
`ghost-surface-renders-filtered1` contains 156 EXRs and passes all 14 groups,
including tint checks on the baseline, red/blue palettes and authored glow.
This candidate filters base-color and emission detail through the native ghost
color while retaining original coverage. It uses the native unlit treatment,
so it does not retain the source's lit PBR appearance. The diagnostic comparison
image confirms blue/red rather than the lit experiment's white/brown surfaces.
Full characters, remaining body/eye adapters, material-detail fidelity under this ghost
treatment, DX12 and live integration remain open. All isolated editors are
terminal; there is no active render to wait on. See
[material support](genessa-doubles-materials.md).

Release packaging: two separate ZIPs, CSS runtime and shared assets. The user
confirmed this on October 4. See the goal for install paths and compatibility
notes. Do not combine them into a single runtime ZIP or bundle shared assets
again with each outfit.

Beta.10 is preserved. No production appearance changes for doubles are installed
or implemented yet. Static tracing identifies both summon families. A completed
180-second read-only capture contains 367 samples: five Faithful actor
identities and one Stray primary, across the user's death/form transition.

Use the player's owned spawner and verified actor ownership for discovery.
Keep native ability, targeting, collision and lifetime logic intact. Each
double needs independent appearance state and visual settings driven by its
own animation. Player MIDs must not be mutated by the double's fade.

A bounded lifecycle tracker and an on-demand C++ observer are now implemented.
The observer is compiled only with `CSS_INVENTORY_DEV`, through engine bridge
operations `astral.observe` and `astral.clear`. It has no tick or spawn hook,
does not change appearances, and is not installed. Portable lifecycle checks,
including AddressSanitizer/UndefinedBehaviorSanitizer, pass. All 17 available
host CTest cases pass. Development and shipping Windows DLLs build successfully;
the latter excludes the observer. Live validation of this C++ observer remains
pending; the earlier Lua capture is the behavioral reference.

`AstralMaterials` now provides private uniform copies and native-opacity
synchronization, without any component assignment or summon hook. Matching
source/companion pairs share one instance per owner. Scalar indices belong to
their own MIDs and are captured after uniform copying; unchanged opacity skips
all setters. Weak owner/native handles gate access, retained material inputs
protect preparation, and explicit release drops the private set.

The development-only `astral.materials.probe` checks the actual owner using an
unattached opacity driver, leaving native actors and player materials alone.
Its source documents copy/readback, fade endpoints and clamping, repeated slots,
rejected preparation and keep-alive cleanup checks. Neither this probe nor the
owner has been live-verified. Adapter selection, binding, appearance transfer,
pooling integration and production shared-asset packaging remain unfinished.

`AstralMaterialBindings` now binds a prepared private set to up to 16 components
owned by one actor (128 base/overlay references total). It validates all inputs
and retains rollback references before writes, rejects missing authored overlay
companions, clears the transient global effect only after per-slot bindings,
and checks readback. Restore touches only slots still owned by that set; it
preserves native replacements and clears its own leftovers if the mesh changed.
No mesh/pose, collision, ability or player settings are changed by this class.
It is not called automatically yet.

The new development operation `astral.bindings.probe` creates one hidden,
unregistered diagnostic component and checks duplicate rejection, binding,
restoration, native material takeover, overlay takeover and retained-object
cleanup. The probe compiles but has not run in game. Both development and
shipping DLLs build after this addition; all 17 existing host tests pass.
Those host tests do not exercise Unreal binding calls. No build was installed.

The offline material inheritance resolver now distinguishes 102 interfaces in
the seven-mesh Eve/Commander White inventory. Native Uber contributes 67 masked
and 24 opaque interfaces, so a root-only adapter lookup is insufficient. Seven
inheritance tests pass. Five additional native surface/eye SM6 pixel shaders
were extracted and disassembled, including a masked shader's alpha/channel/
strength discard calculation. The resource-table decoder now resolves 13
material texture bindings across those five shaders and confirms that the
masked variant samples base-color alpha. Seven binary-parser tests pass.
Literal textures use the cached expression list, whose ordering differs from
the export's top-level referenced textures. Native body and other eye-family
composition remain pending; Genessa's two eye shaders now have rendered
companions in isolated fixtures. The detailed evidence and
commands are in [material support](genessa-doubles-materials.md). This audit
does not establish runtime or visual compatibility.

Validation for this material-owner change: `css_core` built successfully in
`build/windows` (development) and `build/release-windows` (shipping), with no
reported compiler warnings. Reconfigured and built `build/release-host`; all
17 registered CTest cases passed. Those portable tests do not execute reflected
material calls. The signatures were checked against the game's `Engine.hpp`
dump and UE 5.6.1 `MaterialInstanceDynamic.cpp`. The scalar-index constraint is
also documented by [Epic](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UMaterialInstanceDynamic/SetScalarParameterByIndex).
No DLL, package or settings were installed by this change.

`Appearance::astral_source()` now captures the live visual inputs for later
double preparation. It requires the actual Faithful/Stray gameplay tag and a
matching observed player component. It refuses a transitional vanilla mesh
while CSS still intends to restore a custom appearance. The capture includes
the body and CSS-owned modular components, retained material/overlay handles,
per-LOD hidden material slots, explicitly set nonzero morph weights, component
visibility/transforms, and supported post-process spring/dynamics/rig/body
geometry settings. No player locomotion, combat pose, weapon attachments,
collision configuration or ability state is copied.

For the managed body, material sources come from CSS's retained expected
overrides and mesh defaults; authored fabric MIDs take priority over transient
gameplay overlays. Their current uniform values include unsaved edits. These
are live handles, not immutable parameter snapshots, so capture and private
material preparation must happen together on the game thread. Other sources
and modular components use their currently bound materials. Arbitrary foreign
accessory components are not enumerated or claimed supported by this capture.

The development operation `astral.source` serializes this capture and verifies
that its scoped retained-object count returns to baseline. Development and
shipping Windows builds both completed successfully after adding the capture
and probe. It is not installed, has not been live-verified, and is not yet wired
to summon preparation. Reading source settings alone does not prove that a
double reproduces them. Existing portable tests do not cover these reflection
calls.

Material composition is still unresolved. The native ghost shader lacks
configurable mask textures. Replacing every material with it loses hair,
fabric and wing coverage. Retaining the original materials with a global ghost
overlay also needs proof for coverage and full fade-out. No architecture has
yet passed those requirements.

The native shared shader libraries have now been decoded. Four SM6 pixel
shaders disassemble successfully. A diagnostic HLSL reconstruction of the
blue/red color response and shape-dependent dissolve passes 8,462 comparisons
against the extracted instructions and compiles as `ps_6_6`. This moves native
composition beyond parameter inspection. Its UE graph now compiles on
VULKAN_SM5 and passes a ten-image sphere test for both forms, time variation
and disappearance. The GameTime binding is verified from generated UE layout.
Native-versus-copy visual equivalence and outfit composition remain unverified.
See [shader evidence](genessa-doubles-shaders.md).

Seven coverage-composed ghost parents now exist in the isolated stage. Both
112-image hair runs completed, but the combined ghost coverage checks failed:
several Commander White layers were blank, and the dim LDR fixture did not
provide enough testable holes. Manual exposure did not change those results.
Readback confirms original alpha wiring is preserved. A subsequent EXR test
resolves the blank-layer observation: the native Vulkan color-reading path
quantizes through 8-bit FColor, while RGBA16F EXR export preserves the values.
All eight hair materials now pass linear coverage-product, color, animation and
fade checks in both forms (16 groups). These are CSS-owned experiments, not
changes to any outfit or installed build. Complete hairstyles, palette
composition, body/eyes, DX12 and runtime integration remain pending.

An isolated garment-fade experiment now exists under
`/mnt/eins0fxE/CSS-work/genessa-doubles/material-prototype1`. The first
NullRHI authoring run completed on UE 5.6.1 CL44394996, creating five private
parents while verifying unchanged source hashes. The ten-image Vulkan render
completed with exit zero. In both views zero-opacity garments are pixel-identical
to removed garments; half opacity differs from full opacity. Full opacity also
differs from the original, so this is not proof of visual equivalence.

Eve and Commander White's released meshes resolve to four material families.
Separate hair companions were authored successfully, preserving their distinct
AO wiring and specular defaults. The first flat-card hair render stalled during
Vulkan image readback after three complete card sets and was stopped. The bounded
single-card retry (`hair-index3.log`) also stopped with timeout exit 124, after
original/full/half images and an empty zero-opacity output. No test editor
remains running. The user was asked to close the game temporarily to compare
without GPU contention; that cause is not yet established. Do not claim a
passed hair batch from these partial images.
An alternate-order retry (`hair-zero-first.log`) started with zero opacity and
also timed out with exit 124, leaving an empty first PNG. Thus the stall does
not require earlier captures in that process. It still does not establish the
cause. No test editor remains running; do not repeat the same configuration.
A changed setup with an opaque backdrop completed the index-3 and full eight-card
tests. All eight hair materials pass the fade pixel checks, including full
opacity equal to the original in these dim captures. Full hairstyle and native
ghost integration remain pending. The full batch had a wrapper exit 127 caused
by editing the running shell script; UE completed all captures successfully.
The material findings document the distinction and evidence.
[Material findings](genessa-doubles-materials.md) record the
exact versions, source paths, known limitations and readback evidence.

## Evidence

| Capability | Status | Evidence |
| --- | --- | --- |
| Faithful spawn route | Static and live actor identity verified | AstralArmy -> AstralCopy; live-summons-02.log |
| Stray spawn route | Primary live verified; secondary static only | AstralClones -> primary/secondary AstralClone_Single |
| Initialization order | Static verified | ActivateAstralAI sends event before completion broadcast |
| Owned discovery | Live verified for Faithful | AllCharacters contains player-owned AstralCopy actors |
| Native effect materials | Static and Faithful live verified | All nine clone slots share its private native summon MID |
| Native fade | Static and sampled live values verified | Faithful spans 0..1, Stray primary .388..1; no CSS writes |
| Clone animation/physics | Faithful live baseline | ABP_Shell_Genessa; cloth/rigid body disabled; clone physics override |
| Appearance and customization transfer | Implemented, not installed or live verified | `astral_doubles.inl`, `astral_source.inl`, `astral_visual_mesh.inl` |
| Private material uniform copy | Live test passed for five families | live-material-copy-03.log; inherited textures included, player material values and bindings unchanged |
| Material masks, opacity and native fade composition | Offline companion checks pass; live composition pending | `material-pack4/material-check.json`; runtime adapter selection; native MID fade source |
| Private garment fade graphs | Vulkan fade test passed, visual equivalence not established | Both zero renders equal removed; full and half differ; source hashes unchanged |
| Eve and Commander White hair | Eight flat-card fade checks passed; full hairstyle/ghost integration pending | `hair-renders-all_backdrop/pixel-check.json`; two distinct parent graphs |
| Hair coverage composed with native ghost | 16 HDR fixture groups passed; full-character integration pending | `ghost-hair-renders-exr1/hdr-check.json`; source-alpha product, time, fade and both tints |
| Native effect reconstruction | Arithmetic, UE compilation and sphere fade/time checks passed | `native-response-check2`, `native-ghost-compiled.json`, `native-ghost-renders/pixel-check.json`; no native image comparison yet |
| Faithful reuse | One live reuse verified | Same actor 2147330429: cached/disabled -> uncached/enabled -> cached/disabled |
| Bounded lifecycle tracking | Portable tests and development Windows build passed; C++ live check pending | `astral_lifecycle.hpp`, `astral_observer.inl`, `astral_lifecycle_tests.cpp` |
| Stray lifecycle and cleanup | Partial live evidence | Primary is cached=true and enabled=true while opacity changes; secondary not sampled |
| Performance and regressions | Instrumentation ready; live checks pending | Separate Astral frame-profile phase; no new core installed |

## Next steps

The user explicitly added Commander White material support alongside Eve on
October 4. Both are required validation targets, including their hair coverage
and customization. Inspect released material families and stage private copies;
do not edit either outfit's authoring sources or current packages for this test.

1. After normal game close, run `tools/shared-assets/install-trial.py` with
   `material-pack4` and a new receipt directory. It checks the process again and
   preserves the old core, settings, loader, UE4SS and legacy Eve shared package.
2. After relaunch with actual Genessa equipped, verify the selected DLL and
   shared package, then run the source/adapter and hidden visual checks. Arm
   `astral_trial` only after those checks pass. No player input automation is needed.
3. Test Faithful and both Stray doubles with the current appearance and unsaved
   edits, then pooling, transitions and cleanup. Use live evidence to resolve
   remaining body-offset, material-fidelity and accessory limitations.
4. Verify Eve, Commander White and representative original/NPC/non-CSS looks,
   then measure resource cost and run aiming/locomotion/profile/gate regressions.
   Release packaging remains pending full acceptance and an agreed version.

Unholy Genessa's world-only garment clipping remains a separate queued task.

## Live material baseline

`live-source-materials-01.log` records 25 player material slots and six authored
fabric overlays. The active body uses the game's UberShaderV2 parent; garments
use Mat1/Mat10/Mat12 and Fabric02; eyes use the game's native eye materials.
Current overrides include GlowStrength=4.4, GlowFill=1 and FabricVisible=1.
This reads current MIDs, including unsaved values, without changing them.

The exact-version `SkeletalMeshSceneProxy.cpp` lines 537-549 and 843-876 create
a separate overlay batch, swap its material proxy, and reuse section geometry.
They do not pass the base material's mask into the overlay shader. Therefore
native ghost overlays alone are not a proven solution for masked surfaces.

Eight current player material parents were decoded using a scratch directory
of symlinks to the base containers and installed Unholy Genessa containers.
AssetReadback scans only its top-level directory; its first attempt against
the base Paks folder could not resolve mod packages. That offline process
aborted, with no changes to the game. The staged extraction decoded all eight.

`material-capabilities.json` confirms the original summon shader has only
GlobalOpacity plus a compiled Corrupted switch. The outfit's Mat12 metal is
masked, Fabric02 parents are translucent, and none expose a general ghost/fade
parameter. UberShaderV2 exposes IsGhost but also a static UseGhost switch;
its existence does not prove the player's compiled instance supports it.
Generic BPC_GhostFX swaps materials and has a separate timing system, so calling
that component is not a substitute for the Astral clone's native fade cue.

The next material experiment must preserve the actual source masks and
parameters, and drive full disappearance from the native clone opacity. A
CSS-owned companion shader/adapter is a candidate; no shader implementation
or visual compatibility is claimed yet.

## Isolated fade test

The diagnostic scripts stage Mat1, Mat10, Mat12 and Fabric02 material sources.
They duplicate the graphs and multiply original coverage by a clamped
`CSS_AstralOpacity`, default zero. The masked parent's original threshold is
retained through a Step expression. All copied graphs use translucent surface
lighting for continuous fade. This is a compiled-material change, not a MID
property trick. Native ghost shading, body and eyes are not adapted yet.

The render script uses the existing authoring module and SK_EveW3 through
read-only links, with writable copies only in the secondary-drive stage.
It compares original/full/half/zero/removed garments from front and back,
freezes emission phase and cloth, and records shader compilation results,
slot coverage and source hashes. Test instances use a private subdirectory of
the UnholyGenessa namespace to satisfy the existing compilation helper's
path check. They are not production assets.

The first render was started with `NoZenLocalFallback`, which misses the
installed engine's `Compressed.ddp`. It is doing a cold global-shader build.
The reusable runner now uses the exact-version `InstalledNoZenLocalFallback`
graph, which includes that read-only cache, and `UE_LocalDataCachePath` for its
writable stage cache. The original cold render has finished. Do not repeat it
without a changed hypothesis or render configuration.

The user subsequently authorized pip/npm cache cleanup, conservative uv
pruning, Zen DDC cleanup and safe backup cleanup. These are now complete.
Main-drive available space increased from 12,246,016,000 to 45,349,818,368
bytes during the cleanup (the final `df -h` reports 43G). uv retained about
16G after pruning. Zen's `ue.ddc` and `ue4.ddc` namespaces were dropped using
its management tool; three project records, the CAS and installed binaries
were retained, and the temporary server was shut down. The offline render used
its separate file cache during cleanup and has since finished.

Backup cleanup removed 7.67 GiB of generated package thumbnails/dye masks
and 18 redundant staging sets totaling 3.32 GiB. Every removed staging
trio was SHA-256 matched against a complete retained rollback trio before
deletion. Unique staging sets, all retired rollback package files, settings
and release archives remain. Backups now account for about 20G. Receipts are
under `work/cleanup/20261004/`. Keep generated shader files on the secondary
drive for this experiment.
