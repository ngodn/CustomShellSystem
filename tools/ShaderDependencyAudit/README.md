# Shared material shader audit

.NET 10, using the repository's patched CUE4Parse checkout. This checks every
package in a supplied list, identifies materials by export class, and resolves
their shared shader hashes and shader-group chunks. Inline shader data is parsed.
Missing hashes, missing chunks and parse errors produce a nonzero exit status.

```sh
dotnet build tools/ShaderDependencyAudit/ShaderDependencyAudit.csproj -c Release
dotnet tools/ShaderDependencyAudit/bin/Release/net10.0/ShaderDependencyAudit.dll \
  CONTAINER_DIRECTORY GAME_MAPPINGS.usmap PACKAGES.txt NEW_REPORT.json
```

The container directory must include current base game containers, required
outfit/shared dependencies, and any patch being tested. Generate `PACKAGES.txt`
from the full `retoc list --path` inventory. Use `/Game/...` package names without
extensions. The output must not already exist.

Capture the Steam build, input container hashes and tool revision separately.
Do not run while Steam is replacing game files. A passing report establishes
dependency presence under this mount set. It does not establish actual game
mount order, bytecode execution or rendered appearance. A package with no
material exports still needs its ordinary dependency and gameplay checks.

The associated Python 3.14 staging tool has two entry points:

- `tools/repair_shared_material_shaders.py --help` reconstructs an independently
  verified original/patched donor pair byte for byte before applying its shader
  suffix to compatible targets.
- `tools/repair_shared_material_shaders.py from-game --help` uses current-game
  shader slices. Extract each set with `ShaderDependencyAudit --extract-tails
  CONTAINERS MAPPINGS PACKAGES NEW_DIRECTORY`. Supply an ordinary AssetReadback
  report for each set as an independent check of the extracted resource data.

Both paths are restricted to the observed retoc UE5.6 single-export,
two-platform material layout. They preserve the target property bytes and change
only the shader suffix and two serialized size/offset fields. Output directories
must be new. Packed readback and current-game testing remain required.

The current-game path first requires equal parents, static parameters, base
property overrides and static-permutation flags. Native aliases with identical
properties and compiled data are equivalent donors. Static-parameter differences
are refused unless explicit `--compiled-reference-readback` and
`--compiled-reference-slices` inputs prove the target's original compiled shader
matches a reference with the donor's exact properties. That proof compares both
resource JSON and every shader byte, allowing only material identity hashes to
differ. Parent, base overrides and static-permutation flags must still match.
Multiple proved donors must produce equivalent compiled shaders under that same
comparison. Different shader code, bindings or flags are never interchangeable.

This optional proof preserves the previously compiled behavior of materials
whose saved switch metadata differs from that behavior. It does not recook a new
permutation from those switches. Every exception is recorded as a
`compiled_reference` in the staging report.
