# Genessa doubles status

Session 2, October 4, 2026. Branch `feature/genessa-doubles`.

## Current state

Beta.10 is preserved. No production appearance changes for doubles are installed
or implemented yet. Static tracing identifies both summon families. A completed
180-second read-only capture contains 367 samples: five Faithful actor
identities and one Stray primary, across the user's death/form transition.

Use the player's owned spawner and verified actor ownership for discovery.
Keep native ability, targeting, collision and lifetime logic intact. Each
double needs independent appearance state and visual settings driven by its
own animation. Player MIDs must not be mutated by the double's fade.

Material composition is still unresolved. The native ghost shader lacks
configurable mask textures. Replacing every material with it loses hair,
fabric and wing coverage. Retaining the original materials with a global ghost
overlay also needs proof for coverage and full fade-out. No architecture has
yet passed those requirements.

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
| Material masks, opacity and native fade composition | Open | Native shader has no texture parameters |
| Faithful reuse | One live reuse verified | Same actor 2147330429: cached/disabled -> uncached/enabled -> cached/disabled |
| Stray lifecycle and cleanup | Partial live evidence | Primary is cached=true and enabled=true while opacity changes; secondary not sampled |
| Performance and regressions | Pending implementation | No new core installed |

## Next steps

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
