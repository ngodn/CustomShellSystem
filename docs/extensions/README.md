# CSSX SDK guide

CSSX adds extension menus to Mortal Shell II's Inventory. Write behavior in C++ or Lua, declare controls in JSON, and let the shared UI Kit handle rendering and navigation. CSSX uses UE4SS for engine access. An existing UE4SS mod needs a port or adapter; copying its DLL or script into extensions is not enough.

This guide describes API 1, manifest schema 1 and menu schema 1 for CSS/CSSX 0.3.0. These are separate from the CSS.Package v1 outfit format. Existing outfit ZIPs do not need conversion for CSSX.

## Choose a starting point

| Goal | Start here |
| --- | --- |
| Lua extension with saved settings | [Counter example](../../examples/extensions/lua-counter) |
| Native C++ extension | [Native counter](../../examples/extensions/native-counter), then [native build steps](native.md) |
| Explore every control | [UI Kit gallery](../../examples/extensions/ui-kit) and [component reference](ui-kit.md) |
| Manifest, packaging and UI reload | [Getting started](getting-started.md) |
| Engine access, settings, logs and exports | [Host API reference](host-api.md) |
| Cleanup, ownership and compatibility | [Architecture](architecture.md) and [game-build record](../development/game-build-compatibility.md) |

## What players install

1. CSS goes into `MortalShell2/Binaries/Win64/ue4ss/Mods/`.
2. CSSX goes inside the resulting `CustomShellSystem/` folder.
3. Your extension folder goes inside `CustomShellSystem/extensions/`.

Use a unique, permanent namespace such as `yourname.mod-name`. It owns your settings, logs and output paths. Do not include personal state, cached resources, development DLLs, CSS or UE4SS in your extension ZIP.

## Before shipping

Build and run the canonical validator, then package with `tools/cssx_package.py`. Test an empty state directory, an existing saved preference file, Unicode installation paths, keyboard/mouse, controller navigation, missing player, travel, death, and cleanup while enabled. Test both menu layouts if you offer both. An accepted function call does not prove its gameplay result; inspect the resulting game state.

Keep ticks short. Cache stable metadata, resolve the current player again after possession changes, and stop periodic work when it is not needed. Never repeat an irreversible action automatically after a timeout or failed result read. The game may already have saved it.

Native extensions are trusted code in the game process. CSSX provides validation, managed services and lifecycle handling, not crash isolation for arbitrary native memory access.
