# Genessa doubles capture

Requires the already installed, running MS2AttackProbe with its on-demand
`repair_` request handler. Python 3.14 on Linux and UE4SS Lua 5.4. This is a
development diagnostic, not a CSS runtime dependency.

```sh
python3 tools/diagnostics/genessa-doubles/capture.py \
  /path/to/ue4ss/Mods/MS2AttackProbe \
  work/genessa-doubles/session.log --seconds 180
```

The user summons normally. The capture reads only their owned spawner and
its actor list, bounded to 32 actors and 128 material slots per actor. It sends
no gameplay input, changes no materials, and loads no game assets.

Use `--mode materials` for current player base/overlay parent chains and scalar,
vector and texture overrides. This includes transient customization MIDs.
It is not a complete material graph dump or proof of visual equivalence.

`--mode copy-materials --seconds 1` creates unattached transient MIDs for up to
eight distinct current material families. It copies uniform parameters, checks
explicit scalar/vector/texture values and inherited textures, then changes a
scalar on each copy and verifies the source and component binding are unchanged.
Copies are never assigned to a mesh or rooted and can be collected normally.
This mode writes only the new transient instances, unlike the read-only modes.
It runs once and does not measure frame cost or prove shader compatibility.

The wrapper temporarily replaces `Scripts/RepairPrologue.lua` with the
read-only callback and restores the previous bytes on completion, error,
Ctrl+C or SIGTERM. It refuses to overwrite a concurrent callback edit.
Do not run another probe client concurrently. The lock coordinates only other
instances of this wrapper. SIGKILL or host failure cannot run cleanup.

Output files must be new. Each sample ends with ASTRAL_END; errors stop capture
instead of treating a partial sample as evidence. Request logs remain in the
probe directory; the combined file copies their contents for analysis.

Research and limitations: [index](../../../docs/development/genessa-doubles-index.md).

## Offline garment fade experiment

Use the exact UE 5.6.1 Linux editor and its bundled Python. This experiment
does not install anything or communicate with the running game.

```sh
python3 tools/diagnostics/genessa-doubles/stage-materials.py \
  /path/to/UnholyGenessa/work/ue/Content \
  /path/on/secondary/drive/material-prototype --render-fixtures
bash tools/diagnostics/genessa-doubles/run-material-test.sh \
  /path/to/UnrealEngine-5.6.1-installed/Engine \
  /path/on/secondary/drive/material-prototype create create1
bash tools/diagnostics/genessa-doubles/run-material-test.sh \
  /path/to/UnrealEngine-5.6.1-installed/Engine \
  /path/on/secondary/drive/material-prototype render render1
```

The stage must be new. It copies the garment material sources, links unchanged
mesh/dependency files and the existing CSSAuthoring editor module, and records
source hashes. Bubblewrap mounts everything read-only except the stage. All
generated assets, caches and logs stay there. Do not run these scripts in the
outfit's source project.

`create-fade-materials.py` duplicates five garment parents. Their original
opacity or thresholded mask is multiplied by `CSS_AstralOpacity`, defaulting
to zero. Opaque parents gain an opacity input. Original surface parameters
remain in the copied graphs. This requires a translucent shader permutation;
it is not a runtime MID blend-mode override.

`render-fade-materials.py` prepares private instances with matching authored
defaults, then renders the original, full, half, zero and physically removed
garments from two views. Compilation results, covered slots and image hashes
go in `fade-renders/captures.json`. The source body stays fixed as a reference;
eyes are excluded. Render-instance copies use the UnholyGenessa namespace
because the existing editor compilation helper limits its input to that path.
Shipping adapters, if adopted, belong to CSS rather than that fixture namespace.

These files establish only an offline coverage/fade test. Native ghost shading,
live customization, body and eye adaptation, DX12 cooking, animation and
performance still need implementation and verification. A successful compile
alone does not prove visual correctness.

For Eve v1.4.0 and Commander White v0.0.6-dev, add both source receipt directories
to staging:

```sh
python3 tools/diagnostics/genessa-doubles/stage-materials.py \
  /path/to/UnholyGenessa/work/ue/Content \
  /path/on/secondary/drive/outfit-materials --render-fixtures \
  --eve-hair /path/to/Eve/work/hair-v14-material2 \
  --commander-hair /path/to/CommanderWhite/work/cw270/runtime5
```

Run `create` as above, then use runner mode `hair` for the flat UV-card comparison
of all eight current hair instances. Results go in `hair-renders/`. Each graph
keeps its original AO wiring, specular default, texture parameters and alpha.
Source hashes must match the authoring receipts before copying. Graph creation
and flat-card rendering do not establish full hairstyle or in-game support.
Set `CSS_ASTRAL_HAIR_INDEX` to 0..7 to isolate one card per editor process;
its output and fixture names receive that index. Use a new stage for a repeated
index. A Vulkan readback stall occurred in the first multi-card run; preserve
its partial outputs, and require a complete capture report before validation.

Run `check-fade-renders.py RENDER_DIRECTORY` on either completed image set.
It uses Python 3.14 and Pillow 12.3, checks capture hashes, requires zero opacity
to equal actual removal, and rejects blank or unchanged full/half renders.
`pixel-check.json` records differences from the original without declaring
visual equivalence. The output must be new.

`material-inventory.py MESH_READBACK OUTPUT MATERIAL_READBACK...` resolves cooked
skeletal material references through all decoded parent files. It rejects missing
parents, cycles and conflicting package data. Its output retains instance
properties, including blend overrides; classifying by the root alone is unsafe.
