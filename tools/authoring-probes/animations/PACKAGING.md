# Adding audited performance adapters

`package_shared_library.py --adapter-addition <descriptor.json>` adds a cooked
adapter without replacing the existing verified adapter set. Repeat the flag
for additional outfits. The descriptor requires these fields:

- `bindings`: independent cooked graph comparison with revision-bound catalog entries.
- `evaluation`: complete body and finger results for every declared mesh and moving clip.
- `package_receipt`: the cook/probe receipt covered by the binding evidence.
- `cook_exit`: successful Windows cook result.
- `cook_root`: directory containing `CSSAuthoring/Content` cooked payloads.
- `meshes`: exact evaluated mesh object paths.

The validator requires inactive, start, seek, held-end, stop, null-clip, restored,
zero-weight and full-clip cases. It protects the meshes, clips and graph inputs
used in evaluation. Bind-mounted authoring paths resolve only through the same
package-relative path and SHA-256 in the protected cook receipt. Changed input,
missing evidence, duplicate mesh/clip rows or conflicting layers fail before
staging. The packager checks declared source TOC/bulk revisions and rejects
conflicting adapter bindings. It does not install anything.

The GeneXXa example descriptor is
`work/cine-animation-port/expanded-outfit-inventory/genexxa-pack-addition.json`.
Its complete invocation is recorded in
`work/cine-animation-port/shared-v1/genexxa-package-command.json`.

Run the validator regression tests with:

```sh
python3 -m unittest discover -s tools/authoring-probes/animations -p test_adapter_additions.py -v
```

After packaging, run `verify_animation_dependencies.py` on a fresh output
workspace. Supply the exact .NET executable, mappings and independently built
AssetReadback DLL through `--dotnet`, `--mappings`, and `--exporter`. This mounts
one outfit with the shared pack at a time and checks all declared dependencies.
The current six-adapter fixture is also accepted by the host test executable's
`--audited-expanded-pack` and `--audited-expanded-isolated-pack` modes.

These checks establish package structure, catalog resolution and unchanged
payloads. They do not replace live animation, physics or interruption checks.

For legacy authoring dependencies, run `inspect_external_rig_assets.py` in
UE 5.6.1 with `CSS_ANIM_WORK` pointing to a private animation workspace whose
`cook.txt` lists the shared roots. It traverses package dependencies and writes
`external.txt` for `CSSCookAssets -ExternalRigAssets=...`. Only existing
CSSAuthoring skeletons and skeletal meshes become external boundaries, with
hashes checked before and after traversal. Other dependencies remain subject
to the cooker's normal path checks.

`package_adapter_probe.py --help` describes the single-adapter comparison
step. It requires a successful cook and fresh authored graph receipt, mounts
the original outfit beside the probe, then independently decodes and compares
the original and cooked graphs. Its `probe/verified-binding.json` is evidence
for the descriptor's `bindings` field. Complete per-clip evaluation is still
required separately; this probe cannot approve an adapter for all animations.
