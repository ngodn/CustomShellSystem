# Ability-tag and Asset Registry probe 02

Status: prepared in the workspace, not installed or observed live. Probe 01 verified reading 137 current grants, four selectors and their class/montage links. It also proved that the SDK's legacy `GetAssetsByClass` wrapper does not match the live parameter layout. Do not retry that wrapper.

Probe 02 keeps the first pass and adds reflected `AbilityTags`, `ActivationOwnedTags` and `IsHoldAttack` where present. Referenced class CDOs are read as well as granted defaults/instances. This supplies semantic evidence for slot mapping without interpreting class-name suffixes as proof.

After the grant capture, it resolves the registry through reflected `GetAssetRegistry`, confirms `IsLoadingAssets` is false, then makes three limited queries:

1. The exact package of a montage already found through a live selector. Its full path and `AnimMontage` class must appear in the response.
2. A deliberately nonexistent package under a unique probe path. It must return no assets.
3. The known montage's containing folder, nonrecursive. It must include that known montage. Additional returned assets are metadata candidates, not approved moves.

The known path comes from the current capture. No weapon, montage or asset-class list is baked into the probe. Property names and expected property kinds are interface contracts that are checked against the running game.

Each upcoming registry call is logged on a previous tick. The probe polls background-writer completion and proceeds only after that log is flushed. No disk flush waits on the game thread. Registry output is read with its reflected struct, property offsets, array inner size and name fields. It never casts output to the SDK's historical `FAssetData` C++ layout. Returned native values are destroyed through reflected property operations.

The first probe's timeout and measured 2 ms step cutoff remain. Registry queries themselves are synchronous metadata calls. Their cost cannot be preempted, and the 128-row output check runs after a query returns. Narrow package/folder controls limit the initial scope; they do not establish a hard latency guarantee for every patch. A changed contract, loading registry, failed control, changed object generation, excessive result count or timing overrun stops the capture.

No assets are loaded, no gameplay hook is added and no combat values are changed. No full registry or UObject scan runs. `ScanPathsSynchronous`, rescans and waits are deliberately excluded. Epic records a UE 5.6 cooked-IoStore issue where forced rescans can discard known assets: [UE-278356](https://issues.unrealengine.com/issue/UE-278356). The standard query behavior is documented in [Epic's Asset Registry overview](https://dev.epicgames.com/documentation/unreal-engine/asset-registry-in-unreal-engine).

## Build and validation

```sh
python3 CCS/tools/ccs.py build-registry
```

This uses `CCS/build/windows-registry`, with the experimental menu disabled. Default and first-probe builds explicitly disable the extra registry controls.

Use the same `probe_report.py` to inspect captured output. For probe 02, it checks actual positive/negative/folder results in addition to terminal counts and timing; the producer's `passed` flag alone cannot establish success.

Installation requires a closed game and a fresh stopped-playing confirmation:

```sh
python3 CCS/tools/ccs.py stage --registry --confirm-game-stopped
```

The installed first probe remains unchanged while the user is resting. Runtime registry controls are still unverified. No evidence from compilation or host tests can replace the live control results.
