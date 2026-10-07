# Cooked shader readback

.NET 10 tool for offline UE 5.6 shader inspection. Inputs are read-only. The
output directory must not exist. Build with `dotnet build -c Release`.

Use the JSON produced by `AssetReadback --shader-maps`:

```text
ShaderReadback CONTAINERS SHADER_MAP_JSON NEW_DIRECTORY [MODE]
ShaderReadback CONTAINERS SHADER_MAP_JSON NEW_DIRECTORY MODE --inline-oodle MAPPINGS
```

Modes are `--pixel` (default, base-pass pixel), `--vertex` (base-pass vertex),
`--all-vertex` (all vertex passes), and `--distortion` (distortion pixel).

The first form extracts native shared-library shader groups. The second also
reads inline `FShaderMapResourceCode` from material exports using the supplied
usmap. It explicitly selects Oodle for compressed inline code; use it only for
a cook with that compression format. Uncompressed resources are copied directly.
UE 5.6.1 `ShaderCore.h`, `FShaderCode::SetFromResource`, establishes the stored
size versus uncompressed-size distinction. Inline resources have a separate
shader-code length excluding optional data.

Both paths check resource indices, frequencies and buffer boundaries before
extracting exactly one bounded DXBC container. The manifest records source map,
shader identity and output hashes. Shared groups found in several mounted
containers must have identical bytes. The 128 MiB decompression bound is an
offline allocation guard, unrelated to runtime bones or attachments.

Genessa validation evidence lives in its `work/face-material-audit1` and
`work/face-astral-shader1` directories. Adding inline support preserved all 56
previously extracted native vertex shaders byte for byte. Astral inline output
contains seven SM6 vertex shaders and one base-pass pixel shader, plus the
SM5 resources. DXC disassembly was checked only for SM6.

Extraction establishes bytecode identity, not rendered appearance or frame time.
