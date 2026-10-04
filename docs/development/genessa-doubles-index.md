# Genessa doubles research index

Read this before resuming work. [Goal](genessa-doubles-goal.md) defines completion;
[status](genessa-doubles-status.md) records what is currently proven.

## Established findings

- Faithful uses `AstralCopy`. Stray uses `AstralClonePrimary` and
  `AstralCloneSecondary`. Use the player's owned `BPC_AstralAISpawner`, not a
  world-wide actor scan.
- `ActivateAstralAI` sends the activation gameplay event before broadcasting
  `OnSpawnCompleted`. A completion hook runs after attack activation.
- The actual Faithful material is `M_Genessa_AstralArmy_Summon`, not the
  similarly named generic `M_Ghost_Genessa`. Stray uses its Corrupted instance.
- The native summon parent's only runtime scalar is `GlobalOpacity`. Its
  vector/texture parameter lists are empty. Arbitrary mask or tint parameters
  cannot be added to this cooked shader by setting MID values.
- The base double disables cloth and rigid-body animation nodes and has a
  clone-specific physics override. Keep gameplay collision independent of
  adapted visual physics.
- Live Faithful actors use their own `ABP_Shell_Genessa_C` instance. Never
  replace this combat pose with the player's selected idle or leader pose.
- Live Stray primary can have `bIsCached=true` and `bCharacterEnabled=true`
  while fading. Do not use the cache flag alone as a visual eligibility test.

## Dead ends and probe corrections

- Assigning the native ghost MID to every custom slot cannot retain authored
  material cutouts or fabric opacity. A mesh-only swap is not a complete fix.
- `GetPhysicsAsset` is not callable through this game's Lua reflection surface.
  The first clone capture stopped with a Lua error there. Read the declared
  `PhysicsAssetOverride` field instead. The corrected capture succeeds.
- No script hot reload. The diagnostic uses the already installed probe's
  on-demand callback, restoring its previous callback afterward.

## Artifacts

Paths under `work/` are local diagnostic evidence, not distributable game assets.

| Artifact | Finding |
| --- | --- |
| `work/genessa-doubles/packages.txt`, `assets.json` | Seventeen ability, actor and spawner packages decoded with AssetReadback. |
| `work/genessa-doubles/effect-packages.txt`, `effects.json` | Native summon parents, fade cue and VFX component. |
| `work/genessa-doubles/ActivateAstralAI.json` | Bytecode ordering: activation at 510, completion broadcast at 569; no-event branch broadcasts at 642. |
| `work/genessa-doubles/OnSpawnedCharacterInitialized.json` | Activates fresh actor with NewSpawn=true, then advances query queue. |
| `work/genessa-doubles/live-baseline.log` | Player uses Unholy Genessa SK_EveW3; owned spawner initially empty. |
| `work/genessa-doubles/live-summons-01.log` | Two Faithful actors identified; read stopped at unavailable GetPhysicsAsset. |
| `work/genessa-doubles/live-summons-02.log` | Corrected read-only actor/material/opacity capture. See status for verified scope. |
| `work/genessa-doubles/live-summons-02-summary.json` | 367 complete samples, six actor identities, one Faithful reuse and Stray primary. |
| `work/genessa-doubles/live-source-materials-01.log` | Current player's base/overlay parent chains, scalar and texture overrides. |
| `tools/diagnostics/genessa-doubles/capture.py`, `capture.lua` | Bounded, on-demand capture of owned doubles without gameplay input. |
| `tools/diagnostics/genessa-doubles/source-materials.lua` | Read-only current material families and effective overrides for adapter planning. |
| `work/genessa-doubles/live-source-materials-02.log` | Same read with vector overrides included; completed and callback restored. |
| `work/genessa-doubles/surface-packages.txt`, `surfaces.json`, `material-capabilities.json` | Eight actual player material parents and native summon shader parameter capabilities. |
| `work/genessa-doubles/ghost-adapter-packages.txt`, `ghost-adapter.json` | Native generic ghost component and Uber ghost function metadata; not the Astral summon path. |
| [Cloth overlay investigation](cloth-overlays.md) | Exact UE 5.6.1 per-slot overlay behavior and existing CSS effect arbitration. |

## External reference

[Epic's UE 5.6 transparency documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-transparency-in-unreal-engine-materials?application_version=5.6)
describes per-pixel opacity, texture-driven transparency and overdraw costs.
It does not establish that an overlay inherits the base material's mask.
Use local engine source and rendered tests for that boundary.

Parameter-array decoding uses UE 5.6.1 `MaterialTypes.h` lines 185-195:
scalar=0, vector=1, double-vector=2, texture=3, static-switch=8. Do not confuse
double-vector or texture-collection entries with textures or switches.
