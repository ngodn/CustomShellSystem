# Shared asset authoring

## Release ZIP

`packaging/astral-shared-assets.json` pins the accepted containers and their
independent validation receipts. From a clean CSS version tag, run:

```bash
python3 tools/shared-assets/release.py build AUDITED_PACK --output dist/VERSION
python3 tools/shared-assets/release.py verify dist/VERSION/CSS-AstralSharedAssets-vVERSION.zip
```

The shared ZIP contains only its three containers, README and checksum manifest.
It does not bundle an outfit, the legacy Eve shared package or personal data.
Copy the matching BBCode from `packaging/astral-shared-changelog.bbcode` beside
the ZIP. Build the separate runtime with `tools/css_release.py` at the same tag.

This UE 5.6.1 editor-only module creates `/Game/CSS/SharedAssets/Astral/ABP_CopyPose`.
The generated animation template has no target skeleton and uses the engine's
Copy Pose From Mesh node. It reads its attached parent, including curves and
custom attributes, before the visual mesh's existing post-process graph runs.
Its generated runtime parent is `UAnimInstance`; the editor module is not a
runtime plugin or part of the DLL distribution.

The native double keeps its original mesh and combat animation. A separate
visual component can use this template to follow that double. Do not attach
it to the player, assign the player's idle, or replace the double's skeleton.

## Isolated check

Use the project's existing UE 5.6.1 engine, C++20 editor toolchain and bundled
Python 3.11. Host staging uses Python 3.14 on this machine. The runtime remains
C++23. Stage on the secondary drive; source asset links are read-only during
the commandlet. This module has its own Binaries directory and does not use
or rebuild Unholy Genessa's authoring module.

```bash
python3 tools/shared-assets/stage-authoring.py SOURCE_CONTENT NEW_STAGE
ENGINE/Build/BatchFiles/Linux/Build.sh CSSSharedEditor Linux Development \
  NEW_STAGE/CSSShared.uproject -NoHotReload -MaxParallelActions=4
bash tools/shared-assets/run-pose-check.sh ENGINE NEW_STAGE pose pose1
bash tools/shared-assets/run-pose-check.sh ENGINE NEW_STAGE physics physics1
```

The first check creates the template and refuses to overwrite an existing one.
It compares 60 moving frames on the same and different skeletons, with visual
post-process disabled. The second reloads the saved template and enables the
outfit's authored post-process. It checks that the extra wing bones move and
the source animation instance remains unchanged. Its `passed` field establishes
secondary animation execution, not combat-pose equivalence. Per-bone differences
are recorded for that separate assessment.

These are manually ordered component evaluations in an editor world, without
rendering or collision. They do not prove live tick ordering, attacks, weapon
grips, cloth collision, speed-driven wing transitions, cooked Windows behavior,
resource cost, or runtime cleanup. No game package or installed runtime is
changed by these commands.

## Shared ghost materials

The verified diagnostic graphs are inputs, not shipped outfit overrides.
`prepare-shared-materials.py` checks their protected-source hashes and links
them into a separate shared editor stage. `create-shared-materials.py` copies
twelve parents to `/Game/CSS/SharedAssets/Astral/Materials`, replaces texture
parameter defaults with shared neutral placeholders and checks package
dependencies. Literal/unrecognized textures are rejected rather than removed.
The manifest records which parameters need original game noise textures and
which need the current source material's texture values. Opacity defaults to
zero until the runtime binds them.

All parents support skeletal, morph and cloth rendering. The body adapters
also serve clothing slots, so their cloth permutations are required even if
the initial fixture used a rigid sphere. `create-shared-variants.py` adds an
instance with the opposite two-sided setting for each parent. Runtime must
select the matching cooked variant; changing a flag on a MID is insufficient.

```bash
python3 tools/shared-assets/prepare-shared-materials.py CANDIDATE_STAGE SHARED_STAGE
bash tools/shared-assets/run-pose-check.sh ENGINE SHARED_STAGE materials materials1
bash tools/shared-assets/run-pose-check.sh ENGINE SHARED_STAGE variants variants1
```

Copy only `Content/CSS/SharedAssets` into a separate Blueprint-only Windows
cook project. Its runtime parent classes belong to Engine; do not include the
editor module. Cook through `cook-shared-inner.sh` with the project's UE 5.6.1
Windows cooker and patched Wine environment. The rootless container needs
`--user 0` to write the host-owned project/output bind mounts. The engine stays
read-only. The script exits rather than reusing an existing prefix or cook log.

```bash
python3 tools/shared-assets/pack-shared.py COOKED_PROJECT GAME NEW_PACK_STAGE --retoc RETOC
```

That tool stages only 28 shared assets, verifies the IoStore container and
checks byte-identical export payloads after conversion back to legacy assets.
Run AssetReadback with its generated `packages.txt` and `--shader-maps`, then:

```bash
python3 tools/shared-assets/check-cooked-materials.py \
  PACK/assets.json STAGE/shared-materials.json STAGE/shared-material-variants.json PACK/material-check.json
```

The checker verifies both Windows targets, skin/cloth shader variants,
refraction shaders, default opacity, texture binding names, both culling modes,
and absence of outfit/preview/editor-module imports. The pose checker takes
the single pose entry from the same readback separately. These checks do not
replace in-game appearance, animation or lifecycle testing.

## Runtime trial

### Constant-color material extension

`add-clothdriver-material.py` adds the independently audited native clothdriver
parent used by More Beaute. It checks the decoded source revision, preserves
all existing shared assets and creates one parent plus its opposite-culling
instance. Put the independent AssetReadback result in `clothdriver-source.json`
inside a copied shared stage, then run the `clothdriver` mode of
`run-pose-check.sh`. The new parent uses the source `Param` vector directly;
no synthetic color texture or render target is needed.

After cooking that stage, pass both `--manifest STAGE/shared-materials.json`
and `--variants STAGE/shared-material-variants.json` to `pack-shared.py`.
The exact expected asset set then comes from those manifests (30 assets for
this extension). Without the flags, the original 28-asset check remains.
Regenerate the runtime catalog with the same manifest. Cook/readback checks
establish package and shader coverage, not live outfit fidelity.

The current container name is `CSS_AstralSharedAssets_P`. `CSS_SharedAssets_P`
is already used by an older Eve skeleton/animation package on the test machine.
Do not rename the output files alone: `pack-shared.py` uses the new basename
when creating the container, giving it a distinct internal ID.

Generate `native/src/astral_material_catalog.inl` with
`generate-adapter-catalog.py MANIFEST VARIANTS NEW_OUTPUT READBACK...` using the
independently decoded original source roots. It pins graph StateIds and static
defaults, not outfit IDs. Runtime reads effective instance overrides before
choosing a companion. Unknown graphs/permutations fall back to the native double.

`install-trial.py PACK NEW_RECEIPT_DIRECTORY` requires a closed game and the
checked developer build. It backs up the selected core and selector, installs
only the separate new shared package and unique core DLL, and preserves settings,
loader, UE4SS and the old Eve package. It does not quit or launch the game.
Development builds remain disarmed until the ordinary CSS request protocol sends
`{"action":"astral_trial","enabled":true}`. Send false to remove the owned visuals.
The `astral.adapters.probe` engine request selects/loads companions without
binding materials or creating components. Follow it with the existing hidden
visual/material probes before enabling the full coordinator.

The visual body uses Copy Pose; modular items use the source body's leader-pose
arrangement. This follows [Epic's modular character guidance](https://dev.epicgames.com/documentation/unreal-engine/working-with-modular-characters-in-unreal-engine),
with exact call layouts checked against the local UE 5.6.1 source and game dump.
Live pose order, appearance fidelity and measured cost remain acceptance checks.
