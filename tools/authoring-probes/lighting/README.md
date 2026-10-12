# Lighting comparison helper

UE 5.6.1 editor module, C++20. This is private authoring tooling, not a CSS
runtime dependency or a game package.

`CSSLightingLibrary.ConnectClothWeight` connects an expression to
`MP_CustomData0`, which the engine's Python enum hides. It rejects null inputs,
expressions owned by another material and materials without the Cloth shading
model. It reads back the connection before returning success. Python still
owns the rest of the comparison graph and checks the unchanged material inputs.

The Peony fixture is `tools/eve-fit/render_peony_lighting.py`. Set
`CSS_LIGHTING_STAGE` to its prepared native stage and `CSS_LIGHTING_VARIANT` to
`cloth`. Baseline and `flat_normal` do not need this helper. Each comparison
refuses to overwrite its output directory.

Copy `Source` into a separate editor project containing a `CSSLighting` editor
module. Build with the matching installed UE 5.6.1:

```sh
Engine/Build/BatchFiles/Linux/Build.sh CSSLightingEditor Linux Development /absolute/path/CSSLighting.uproject -NoHotReload -MaxParallelActions=3
```

Keep the native fixture's existing prebuilt modules. Add the resulting
`libUnrealEditor-CSSLighting.so` to a private `Binaries/Linux` directory and
merge its `UnrealEditor.modules` entry only after matching both BuildId values.
Add the editor module to the fixture's project descriptor. Do not modify the
accepted source project's binary directory through its symlink.

The October 12 build command, exit code, log and binary live in
`work/three-outfit-rig-physics/peony-v2-lighting1/native1/lighting-module`.
The per-run recipe and command are retained beside the render log. A successful
build or capture is not visual acceptance; see the Peony lighting research note.
