# CSS development SDK

## Current runtime pin, 2026-09-19

The game uses the user's supplied `UE4SS For MS2 - NO AOB 45 3 2026-09-17T13-49Z R0FFdhg1M`
build, `3.0.1-1111-g97b7e501`. Its DLL and the installed DLL both have SHA-256
`fb1839ee91f71f83d508d44a2763a15ac1bb0c5fb4e504ac0fcfca64376a054a`.
`native/ue4ss-runtime.json` is the build/installer/release pin. The earlier
`d7e7826d` runtime hash below is historical.

The selected migration retains the `d7e7826d` headers and UEPseudo reference,
while linking the current runtime through a separate import library. This is
not a full source-SDK migration. `CSS_SDK` names those retained headers;
`CSS_UE4SS_IMPORT_LIBRARY` names the new library. The runtime revision used by
the mappings generator comes from the same JSON pin.

`tools/prepare_ue4ss_import.py` checks the supplied DLL hash, enumerates its 4239
exports, verifies every UE4SS import in each requested consumer and generates
`reference/ue4ss-runtime-97b7e501/UE4SS.lib` with LLVM dlltool. The manifest records
the exact consumer and library hashes. Before rebuilding, all 32 loader and
137 core imports resolve by exact decorated name. This proves symbol
availability, not general class-layout compatibility. Fresh rebuilt runtime
verification is still required.

The import library SHA-256 is
`487fd9693eb1ea4c9dc57e344e232684b9badb5328f496a38295b16db7e71c9c`.
No game UE4SS replacement is needed. An earlier session installed this supplied
build and reported CSS 0.4.2 loading; that historical observation is separate
from current Next-Gen acceptance.

## Retained header SDK and earlier runtime evidence

Acquired 2026-09-13 for the already installed UE4SS runtime. Development dependencies only.

- UE4SS source: `d7e7826d415b0332b43439a64e6c87f64019be03`.
- UEPseudo submodule: `b2e876da82b17254c04304746341c8fde0ddb37c`, downloaded with existing authenticated GitHub access.
- Runtime: `Game__Shipping__Win64`, C++23, MSVC-compatible ABI, dynamic release CRT `/MD`.
- Header dependencies under `sdk/third-party`: fmt 11.2.0, ImGui 1.92.1, ImGuiColorTextEdit 1.2.0, matching pinned source build declarations.
- Additional headers: Zydis 4.1.1, its Zycore submodule `0b2432ced0884fd152b471d97ecf0258ff4d859f`, PolyHook `298d56210b9d9e66cde8f96481d6053925c6ae15`, its asmjit submodule `a3199e8857792cd10b7589ff5d58343d2c9008ea` and asmtk submodule `3bce8a48aa895e6d639501d1f1105ab5fe007753`, and glaze 6.4.0. These follow the pinned UE4SS FetchContent declarations and upstream gitlinks. Zydis is the external UE4SS version selected by its build, rather than PolyHook's nested copy.
- `sdk/lib/UE4SS.lib` was generated from the official DLL's export names using LLVM dlltool for x86-64. `sdk/lib/UE4SS.def` records those exports. CSS compilation, linking and live loading against this import library passed during the first playable milestone.
- `sdk/UE4SS.dll` is the development release's DLL for reference, not a runtime replacement installed by this work.

Source: [matching official development archive](https://github.com/UE4SS-RE/RE-UE4SS/releases/download/experimental/zDEV-UE4SS_v3.0.1-1028-gd7e7826d.zip). Original archive was inspected and has no headers or import library.

SHA-256:

```text
development ZIP 726c59f28b654c4f01dcd57afea4fa4f6e406f721862b3afccb6abe657d75270
UE4SS.dll        4cdd44e79df2a01fb00cf885791f933c1dd3a83324767c7a084c4da5a82f33bc
UE4SS.lib        1a4bb987e63765098970ba36b46ad177e8e1272612b57df950ff77c88546253a
```

The SDK lives in the ignored `reference/ue4ss-sdk-d7e7826d` directory. Include paths below are relative to that SDK directory:

```text
UE4SS/include
UE4SS/generated_include
deps/first/*/include                 (expand to each directory)
deps/first/Unreal/generated_include
deps/first/Unreal/include/Unreal
deps/first/Unreal/include/Unreal/Core
sdk/third-party/fmt/include
sdk/third-party/imgui
sdk/third-party/imguitextedit
sdk/third-party/zydis/include
sdk/third-party/zycore/include
sdk/third-party/polyhook
sdk/third-party/asmjit/src
sdk/third-party/asmtk/src
sdk/third-party/glaze/include
```

Lua headers already ship in `deps/first/LuaRaw/include`; the wildcard first-party include expansion above includes them. A C++ mod can require these headers transitively without implementing its runtime in Lua.

Use `RC_UE4SS_API`, `RC_UE_API` and other import defaults supplied by the headers. Do not define producer-side `RC_UE4SS_EXPORTS` or `RC_UNREAL_BUILD_STATIC` in the consuming mod. Additional macros and dependencies may need to follow the pinned upstream build if used by CSS. `CppUserModBase.hpp` includes GUI types even for a non-GUI mod, hence the ImGui and TextEditor header dependencies.

The pinned source retains generated Unreal member-layout headers. The `patternsleuth` submodule is not fetched because it is a backend implementation dependency, not required for a consumer linking the existing DLL.

Hook API is in `deps/first/Unreal/include/Unreal/Hooks/Hooks.hpp`. Registering current callback overloads returns a removable global ID. The tick callback receives callback iteration data, engine, delta seconds and idle flag. Set owner and hook names in `FCallbackOptions`, retain returned IDs, and unregister before unloading callback code.
