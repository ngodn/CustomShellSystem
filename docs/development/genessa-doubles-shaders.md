# Native summon shader reconstruction

October 4, 2026. Research on `feature/genessa-doubles`. Nothing in this document
claims installed support or a completed material adapter.

## What changed

The native summon shader code is available in the game's shared IoStore shader
libraries. Empty material-resource output from the earlier readback did not mean
the bytecode was absent. CUE4Parse's `ReadShaderMaps` was disabled. AssetReadback
now has an optional `--shader-maps` mode. Its ordinary output for the same two
packages remains structurally identical to the earlier `effects.json` readback.

`tools/ShaderReadback` finds each material's resource hash in a shader library,
resolves the base-pass pixel shader indices, decompresses its Oodle group and
extracts both the UE shader record and its bounded DXBC container. It refuses
missing/ambiguous groups, invalid ranges, oversized groups and existing output
directories. It is an offline read-only diagnostic, not a package writer.
Requests must have unique shader-map hashes. The extractor does not reconstruct
material graphs or support arbitrary shader types.

The native Faithful parent and Corrupted instance produced four shader maps
(SM5 and SM6 for each), containing eight distinct base-pass pixel shaders. All
16 extracted files match the SHA-256 values in their manifest. Microsoft's DXC
successfully disassembled the four SM6 containers. SM5 disassembly is untested.

## Evidence and tools

Under `work/genessa-doubles/`:

- `summon-shader-maps.json`: material shader bindings, uniform expressions and
  shader-map resource indices.
- `native-pixel-shaders/manifest.json`: package, map hash, shader hash, source
  library/chunk, group offsets, container offset and content hashes.
- `native-pixel-shaders/*.ll`: DXC disassembly of the four SM6 shaders.
- `summon-default-regression.json`: ordinary AssetReadback regression output.
- `native-response-check2/result.json`: numerical and compilation results below.

The private DXC installation is `reference/microsoft-dxc-1.9.2609/`. Its Linux
archive SHA-256 was verified against the official release asset digest:
`96faadc7f5c282d2ffda49804beb4c3ee38127bc252b723234e3c5cdf7aa39a1`.
`source.json` records provenance. No system compiler was replaced.

The response reconstruction uses these two SM6 shaders:

| Form | Map hash / resource index | DXIL shader hash |
| --- | --- | --- |
| Faithful | `5ABE009F6CA07E8779EF333B384E0BF4D5944A91` / 4 | `1267d8d0470c084767f31c25d5c80260` |
| Stray | `FBF9FDC100C1D350095BDA08A4F43A40EF6790A0` / 9 | `64e6ad0fa5bb429754b43e24daeaf4e8` |

The map associates these same shaders with GPU skin default/unlimited,
APEX cloth default/unlimited and local vertex factories. Niagara mesh factories
use separate indices 14 and 10, which are disassembled but not reconstructed.

## Recovered material arithmetic

`tools/diagnostics/genessa-doubles/native-ghost-response.hlsl` contains an
explicit reconstruction. Its inputs separate the material math from UE's view,
primitive and texture bindings. This is a diagnostic source file, not a cooked
or registered engine shader.

The noise projection uses position normalized by local object bounds. Local Z
is doubled. A time phase feeds offsets of +225 in Y and -90 in Z. Three red
channel noise samples are blended with weights `saturate(abs(normal.x/z)*3-1)`.
Their result distorts a second three-sample projection of the blue channel.
Both forms use the same noise arithmetic. There are six noise samples per pixel.
The exact View time field behind register 163.z still needs binding verification;
the diagnostic accepts time explicitly and does not guess an engine offset.

Facing and noise feed a four-segment scalar ramp. The forms have different
color-ramp thresholds and RGB constants, followed by division by the exposure
buffer value. The Faithful branch produces the native cyan/blue response; the
Corrupted branch produces the red response. No user texture mask participates
in this native shader.

Opacity combines five factors:

1. A facing term, `saturate(2 * abs(dot(normal, camera))^1.45)`.
2. `saturate(2 * localY^1.75 - projectedBlueNoise)`, with native power guards.
3. A dissolve based on distance from `(0.5, 0.325, 0.7)` in normalized
   pre-skinned bounds.
4. A near-camera fade, `saturate((pixelDepth - 24) / 256)`.
5. A scene-depth intersection fade over five world units.

The native CPU preshader stores `sqrt(saturate(GlobalOpacity))` squared.
With `inverse = 1 - storedOpacity`, the dissolve is:

```text
lower = 2 * (max(inverse, 0.5) - 0.5)
upper = min(2 * inverse, 1)
dissolve = saturate((saturatedRadius - lower) / (upper - lower))
```

This explains why a uniform opacity multiplier alone cannot reproduce the
native fade. Native zero-denominator endpoint behavior is retained in the
diagnostic. It is explicitly exercised by the arithmetic check, with DXIL
saturate's NaN-to-zero behavior represented by the host shim.

Primitive buffer entries 24/25 are pre-skinned local bounds; 26/27 are local
object bounds. Exact UE 5.6.1 `Engine/Shaders/Private/SceneData.ush` lines
461-467 confirm these identities. The material's preshader bytes decode using
`Engine/Public/Shader/Preshader.h` and `Engine/Private/Shader/Preshader.cpp`:
Parameter is opcode 3 with a uint16 index, Saturate is 26, Sqrt is 22 and Mul
is 6. SelectionColor only supplies the normal engine selection highlight.

## Verification and limits

`check-native-ghost-response.py` compiles the same HLSL scalar function as C++23
and compares its output against an independent evaluator of the extracted DXIL
instructions. It requires the expected shader hashes and fails on unsupported
instructions. There are 4,231 cases per form: 4,096 deterministic random samples
and 135 boundary combinations, including both fade endpoints. Maximum absolute
error was `4.174804679735189e-08`, below the declared tolerance. The full HLSL,
including the noise projection, also compiles as `ps_6_6` with warnings as errors.

This establishes scalar arithmetic agreement, not a rendered match. Texture
sampling, mip derivatives, normals, camera/view bindings, local and pre-skinned
coordinate wiring, exposure, fog and scene-depth conversion still require UE
integration and image comparison. The numerical test supplies those inputs.
It does not test the GPU or game runtime. The six-sample noise implementation
has compilation evidence only.

Next: wire these calculations into an isolated UE material with the original
noise texture and verify both forms against the native effect. Then combine
with each source material's real cutout/opacity and current customization.
Do not bypass Eve/Commander White hair coverage or switch production outfits
to these diagnostic shaders without that comparison.

Primary references: [Microsoft DXIL specification](https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/DXIL.rst),
[DXC container layout](https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/DxilContainer/DxilContainer.h),
[official DXC release](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2609),
[CUE4Parse material reader](https://github.com/FabianFG/CUE4Parse/blob/master/CUE4Parse/UE4/Assets/Exports/Material/UMaterialInterface.cs).
Exact engine extraction behavior is in RenderCore's `ShaderCodeArchive.h`
(`GetShaderUncompressedSize`) and `ShaderCodeArchive.cpp` (Oodle group decode).
