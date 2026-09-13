# Native CSS research

Investigated 2026-09-13. This note separates documented engine behavior from CSS design choices. It does not establish that a built mod works in Mortal Shell II.

## Native backend and pinned SDK

UE4SS explicitly supports C++ mods. A mod derives from `RC::CppUserModBase`, exports `start_mod` and `uninstall_mod`, and begins using Unreal APIs after `on_unreal_init`. The standard installation is `Mods/CustomShellSystem/dlls/main.dll`, enabled in `mods.txt` or through `enabled.txt`. [Creation guide](https://docs.ue4ss.com/guides/creating-a-c%2B%2B-mod.html), [installation guide](https://docs.ue4ss.com/guides/installing-a-c%2B%2B-mod.html).

The locally identified runtime is `d7e7826d415b0332b43439a64e6c87f64019be03`, `Game__Shipping__Win64`. Its build files specify C++23 and dynamic CRT `/MD`; the public base-class header cautions that incompatible CRT or Debug/Release combinations break native mods. Its CI requests MSVC developer tools version 18. Keep the game runtime and development headers pinned together. [Pinned build configuration](https://github.com/UE4SS-RE/RE-UE4SS/blob/d7e7826d/UE4SS/CMakeLists.txt), [base class](https://github.com/UE4SS-RE/RE-UE4SS/blob/d7e7826d/UE4SS/include/Mod/CppUserModBase.hpp), [CI configuration](https://github.com/UE4SS-RE/RE-UE4SS/blob/d7e7826d/.github/workflows/cmake_build_ue4ss.yml).

The matching [development release](https://github.com/UE4SS-RE/RE-UE4SS/releases/download/experimental/zDEV-UE4SS_v3.0.1-1028-gd7e7826d.zip) contains a DLL and PDB, but no import library or headers, confirmed by inspecting its ZIP directory. The moving `experimental-latest` release points to a different commit and is unsuitable as a silent replacement.

Exact source and a generated import library are available in [the local SDK](../reference/ue4ss-sdk-d7e7826d/SDK-NOTES.md). The Unreal submodule is `Re-UE4SS/UEPseudo` at `b2e876da82b17254c04304746341c8fde0ddb37c`. Anonymous access returns 404; this machine's existing authenticated GitHub account successfully downloaded the exact commit. No alternate headers or reconstructed ABI were necessary. The SDK remains development material, separate from CSS distribution packages. [Pinned submodule declaration](https://github.com/UE4SS-RE/RE-UE4SS/blob/d7e7826d/.gitmodules).

## Hooks, threading and lifetime

The pinned native hook API exposes `RegisterEngineTickPostCallback` with a callback taking `TCallbackIterationData<void>&`, `UEngine*`, `float`, and `bool`, plus `FCallbackOptions`. It returns `GlobalCallbackId`, removable through `UnregisterCallback`. Options carry once/read-only flags and owner/hook names. BeginPlay, EndPlay, LoadMap, ProcessEvent, and object construction callbacks are also present. Legacy overloads still exist but do not return removable IDs. Prefer the current API. [Exact local header](../reference/ue4ss-sdk-d7e7826d/deps/first/Unreal/include/Unreal/Hooks/Hooks.hpp), [upstream pinned header, authenticated access](https://github.com/Re-UE4SS/UEPseudo/blob/b2e876da82b17254c04304746341c8fde0ddb37c/include/Unreal/Hooks/Hooks.hpp).

`CppUserModBase::on_update` is invoked by UE4SS's event loop, while native game-thread execution is dispatched separately through engine hooks. CSS should queue selection changes from input/UI and consume them on a verified game-thread callback. `UE4SSRuntime::IsEngineTickAvailable` provides a capability check. A callback registration alone does not demonstrate that the hook is available or firing in this game. [Pinned event loop](https://github.com/UE4SS-RE/RE-UE4SS/blob/d7e7826d/UE4SS/src/UE4SSProgram.cpp), [runtime capabilities](https://github.com/UE4SS-RE/RE-UE4SS/blob/d7e7826d/UE4SS/include/UE4SSRuntime.hpp).

Raw UObject pointers held by an external DLL are not GC roots. Epic recommends weak references for non-owning caches and validating them before use. A pointer-shaped value remaining non-null does not prove an object is alive. [Epic object pointer guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/object-pointers-in-unreal-engine).

The pinned SDK exposes object indices, serial numbers, `FUObjectArray::IndexToObject`, `FUObjectItem::IsValid(false)`, and `GetUObject`. CSS can retain index/serial identity and validate the object-array entry before dereferencing the cached object. Cache world/pawn identity as well, invalidate after travel or possession changes, and reacquire components. This is a design recommendation based on the provided APIs, not an assertion that arbitrary raw pointer validation is race-free. [Object array API](../reference/ue4ss-sdk-d7e7826d/deps/first/Unreal/include/Unreal/UObjectArray.hpp), [serial-number accessor](../reference/ue4ss-sdk-d7e7826d/deps/first/Unreal/generated_include/MemberVariableLayout_HeaderWrapper_FUObjectItem.hpp).

For targeted UFunction hooks, the native API accepts a resolved `UFunction*`, pre/post callbacks and opaque caller data, returning a pair of IDs for `UnregisterHook`. Resolve functions only after their classes are loaded. Broad ProcessEvent callbacks impose work on all intercepted calls, so use them sparingly with immediate pointer comparisons and no per-call name formatting. [Native reflection API](../reference/ue4ss-sdk-d7e7826d/deps/first/Unreal/include/Unreal/UObjectGlobals.hpp), [documented loaded-function requirement](https://docs.ue4ss.com/lua-api/global-functions/registerhook.html).

## Mesh replacement boundaries

Epic documents `USkeletalMeshComponent::SetSkeletalMesh(NewMesh, bReinitPose)` as changing the rendered mesh and reinitializing animation state. The pose flag controls whether to retain or reinitialize the current pose. A visual swap can therefore affect animation state, not just render data. [Epic API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USkeletalMeshComponent/SetSkeletalMesh).

CSS design recommendation: treat each appearance as an explicit set of mesh/component/material changes. Resolve every required asset and validate skeleton/component compatibility before applying the set. Preserve the original component state for restoration. Do not repeatedly call the mesh setter when the requested mesh is already equipped. Keep physics replacement opt-in and asset-specific. Actual skeleton compatibility, cloth behavior, slot names, reflected parameter layouts and shell possession events must be verified from Mortal Shell II dumps and runtime tests. Latest Epic documentation describes API behavior, not this game's ABI.

Use unique cooked package paths for each cosmetic pack. If both Beaute variants overwrite the same default assets, a runtime selector cannot recover both identities from that collision. The conversion must give both variants separate loadable paths, including their relevant dependencies. This is the packaging requirement for CSS's intended coexistence behavior.

## CNS feature reference and CSS mapping

CNS 2.2 by DekitaRPG provides independent cosmetic entries, in-game selection, body/head/hair/accessory/weapon categories, multiple characters, variants, icons, material parameters and an animation viewer. Its public author documentation adds remembered selection and scalar/vector/texture controls. [CNS author's mod page](https://www.nexusmods.com/stellarblade/mods/1496), [author documentation](https://github.com/Dekita/SB-CustomNanosuitSystem-Docs).

The supplied local distribution also exposes shape keys, material visibility toggles, configurable scalar/vector/texture controls, favorites, camera/screenshot bindings, and outfit/animation caches. Its configuration explicitly includes alternatives intended to mitigate scanning-related crashes and disables its physics fix by default. These are reference observations from the supplied files, not evidence of CSS features already implemented. [Supplied configuration](../reference/CustomNanosuitSystem-1496-2-2-1771093897/SB/Binaries/Win64/ue4ss/Mods/DekCNS/Scripts/config.lua), [supplied control definitions](../reference/CustomNanosuitSystem-1496-2-2-1771093897/SB/Binaries/Win64/ue4ss/Mods/DekCNS/Scripts/main.lua).

| CNS concept | CSS adaptation |
| --- | --- |
| JSON outfit catalog | Versioned cosmetic pack manifests with stable IDs and unique asset paths |
| Remembered selection | CSS-owned settings storing selected IDs, separate from game save files |
| Character and mesh category | Mortal Shell II shell identity and its verified component slots |
| Variants and favorites | Coexisting BeauteGenessa and BeauteKnightLady entries and future packs |
| Runtime material controls | Explicit named parameters and material-slot validation |
| Animation viewer and camera tools | Later capabilities, requiring independent game-specific animation/camera validation |

CNS's published permissions prohibit conversion of its files to other games and require authorization for modification or asset reuse. CSS should independently implement the desired behavior, credit the inspiration, and exclude CNS scripts, blueprints and art from packages. This observation does not block building CSS from original code and separately authorized Beaute assets. [Author's stated permissions](https://www.nexusmods.com/stellarblade/mods/1496).

## Evidence needed before release

Recommended runtime checks: both appearances selectable in the same session; restoration to default; death/respawn; save reload; travel; shell possession changes; equipment changes; invalid/missing manifest assets; repeated switching; unloading or shutdown; logging without repeated errors. Measure idle callback cost and selection latency separately. A successful C++ compilation or DLL load establishes neither visual correctness nor absence of crashes.
