# Genessa doubles status

Session 3, October 4, 2026. Branch `feature/genessa-doubles`.

## Current state

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

Material composition is still unresolved. The native ghost shader lacks
configurable mask textures. Replacing every material with it loses hair,
fabric and wing coverage. Retaining the original materials with a global ghost
overlay also needs proof for coverage and full fade-out. No architecture has
yet passed those requirements.

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
| Appearance and customization transfer | Not implemented | No runtime patch |
| Private material uniform copy | Live test passed for five families | live-material-copy-03.log; inherited textures included, player material values and bindings unchanged |
| Material masks, opacity and native fade composition | Open | Native shader has no texture parameters |
| Private garment fade graphs | Vulkan fade test passed, visual equivalence not established | Both zero renders equal removed; full and half differ; source hashes unchanged |
| Eve and Commander White hair | Companions authored; card render pending | Seven-mesh audit, two distinct graphs, eight hair instances |
| Faithful reuse | One live reuse verified | Same actor 2147330429: cached/disabled -> uncached/enabled -> cached/disabled |
| Bounded lifecycle tracking | Portable tests and development Windows build passed; C++ live check pending | `astral_lifecycle.hpp`, `astral_observer.inl`, `astral_lifecycle_tests.cpp` |
| Stray lifecycle and cleanup | Partial live evidence | Primary is cached=true and enabled=true while opacity changes; secondary not sampled |
| Performance and regressions | Pending implementation | No new core installed |

## Next steps

The user explicitly added Commander White material support alongside Eve on
October 4. Both are required validation targets, including their hair coverage
and customization. Inspect released material families and stage private copies;
do not edit either outfit's authoring sources or current packages for this test.

1. Use the captured material families to establish composition. Both diagnostic
   runs completed and restored the previous callback. Full Stray secondary and
   cleanup coverage remain for a later test.
2. Establish a material composition method with visual evidence, preserving
   masks and full fade. Inspect existing source materials and native effects;
   do not assume a MID can change a compiled shader graph.
3. Implement per-double ownership, appearance and visual-physics state in the
   C++23 runtime, then test both forms and every required appearance source.

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
