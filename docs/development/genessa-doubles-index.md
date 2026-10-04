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
- Live gameplay tags are `CharacterId.Player.Shell.Genessa` and
  `CharacterId.Player.Darkform.CorruptedGenessa`. The Stray tag is not under
  `Shell`. A Genessa appearance on another gameplay shell is insufficient.
- `K2_CopyMaterialInstanceParameters(source, true)` is the reflected route to
  uniform hierarchy copying. It clears destination values first. Copy before
  setting clone fade controls, and do not use the slow default-false path.

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
| [Native shader reconstruction](genessa-doubles-shaders.md) | Shared shader extraction and SM6 disassembly; 8,462 arithmetic cases, UE VULKAN_SM5 compilation and ten sphere images passed. Native image equivalence and outfit integration remain pending. |
| `native/src/astral_lifecycle.hpp`, `tests/astral_lifecycle_tests.cpp` | Bounded identity/serial tracker; pooled reuse, cached-active Stray, ownership changes, invalid snapshots and cleanup tested on host, including sanitizers. |
| `native/src/astral_observer.inl` | Development-only `astral.observe` bridge operation reads the owned spawner; no tick, appearance writes or actor retention. Compiles for Windows; not installed or live-tested. |
| `native/src/astral_materials.inl`, `astral_materials_probe.inl` | Private uniform copies, per-double material sharing and indexed fade synchronization; development-only unattached-copy probe. No summon bindings, installation or live validation yet. |
| `native/src/astral_source.inl`, `astral_source_probe.inl` | Live body/modular-component capture with current material sources, morphs, hidden sections and supported post-process settings. Both Windows configurations compile; runtime readback and summon application remain pending. |
| `tools/diagnostics/genessa-doubles/copy-materials.lua`, `work/genessa-doubles/live-material-copy-03.log` | Five live families copied into unattached private MIDs; scalar/vector values and ten inherited skin textures matched, and copy edits left player sources unchanged. |
| [Material support](genessa-doubles-materials.md) | Eve v1.4.0 and Commander White v0.0.6-dev family audit, separate hair companions, garment fade evidence and remaining gaps. |
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
| `tools/diagnostics/genessa-doubles/stage-materials.py`, `create-fade-materials.py` | Isolated copies of five garment and two hair parent graphs with original coverage multiplied by a fade scalar. No production changes. |
| `tools/diagnostics/genessa-doubles/run-material-test.sh`, `render-fade-materials.py` | Read-only source sandbox; garment disappearance passed the original/full/half/zero/removed comparison. |
| `tools/diagnostics/genessa-doubles/render-hair-fades.py`, `check-fade-renders.py` | Eight hair material card fixtures and actual pixel checks for fade endpoints and intermediate states. |
| `material-outfits1/hair-renders-all_backdrop/pixel-check.json` on the secondary drive | All eight groups pass with backdrop; full matches original, zero matches removal. Dim captures do not establish bright ghost cutout quality. |
| `tools/diagnostics/genessa-doubles/material-inventory.py`, `work/genessa-doubles/eve-cw-inventory.json` | Seven released meshes, 112 resolved material interfaces, effective instance properties retained for adapter planning. |
| [Cloth overlay investigation](cloth-overlays.md) | Exact UE 5.6.1 per-slot overlay behavior and existing CSS effect arbitration. |

## External reference

[Epic's UE 5.6 transparency documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-transparency-in-unreal-engine-materials?application_version=5.6)
describes per-pixel opacity, texture-driven transparency and overdraw costs.
It does not establish that an overlay inherits the base material's mask.
Use local engine source and rendered tests for that boundary.

Parameter-array decoding uses UE 5.6.1 `MaterialTypes.h` lines 185-195:
scalar=0, vector=1, double-vector=2, texture=3, static-switch=8. Do not confuse
double-vector or texture-collection entries with textures or switches.
