# Inventory wardrobe integration: primary-source feasibility

Research date: 2026-09-14. Scope: a CSS tab alongside Inventory, Tarstones and Map, with Shell/Color/Templates on the left, the game's character preview in the middle, details on the right, and native inventory controls. Keep the existing N wardrobe available. This note records feasibility and proof steps, not a completed integration.

## Assessment

Runtime UMG composition is a credible approach because CSS already constructs UMG objects and invokes reflected functions. A native inventory tab is a larger change than attaching that wardrobe widget: the tab selector, page switcher, selection state, focus, input routing and preview lifetime must agree. The safest first proof is one appended, reversible placeholder page reached through the game's existing tab controls, followed by reusing its existing display actor. Styling and full wardrobe content should follow that proof.

Epic documents `UWidgetSwitcher` as a panel displaying at most one child, with no tab buttons of its own. Its active page can be selected by object or index. Thus a visible new button and a switcher child alone do not establish integration with a game's own tab model. [Epic, WidgetSwitcher, UE 5.6](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/WidgetSwitcher?application_version=5.6)

## What the sources establish

| Area | Established behavior | Consequence for CSS |
| --- | --- | --- |
| Panel attachment | `AddChild`, child counts/index queries and removal are reflected UMG functions; the documentation lists `InsertChildAt`/`ShiftChild` without a BlueprintCallable annotation. | Prefer reflected attachment functions already present in this shipping build. Do not assume every documented C++ method is callable through `ProcessEvent`. |
| Reordering | Epic explicitly says the two-argument `InsertChildAt` does not update live Slate and needs a UI rebuild. | Start by appending a page without renumbering existing pages. If visual ordering later requires reinsertion, prove the panel rebuild and restore all affected slot properties and focus. Never edit `Slots` directly and assume the rendered hierarchy follows. |
| Widget construction | `UWidgetTree::ConstructWidget` is an inline C++ template. Widget trees manage widget ownership/hierarchy; ordinary traversal does not descend into nested user widgets' foreign trees. | A DLL using UE4SS cannot assume the template is a reflected runtime function. Use the existing reflected construction approach for concrete engine widgets, and explicitly inspect nested trees. |

Sources: [Epic, UPanelWidget, UE 5.5](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/Components/UPanelWidget?application_version=5.5), [Epic, InsertChildAt, UE 5.5](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/Components/UPanelWidget/InsertChildAt/1?application_version=5.5), [Epic, ConstructWidget, UE 5.5](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/Blueprint/UWidgetTree/ConstructWidget?application_version=5.5), [Epic, UWidgetTree, UE 5.5](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/Blueprint/UWidgetTree?application_version=5.5).

The 5.6 C++ API endpoints for those construction/reordering methods failed retrieval. Their 5.5 pages establish the caution, not exact 5.6 binary compatibility. Epic's explicitly versioned 5.6 Python reference corroborates the underlying UMG widget behavior below; it does not imply that Python is available in the shipped game.

### Layout and focus

`SetNavigationRuleExplicit` and related setters require the widget to be in a widget tree. `SetFocus` addresses the owning user; keyboard focus and user focus also have query functions. `ForceLayoutPrepass` calculates desired sizes, while `GetCachedGeometry` may be absent or a frame old. Clipping controls overflow and can incur a batching cost. [Epic, Widget, UE 5.6](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/Widget?application_version=5.6)

Design implication: attach left/content/right containers within the game's actual content bounds and slot layout, then wire navigation and initial focus. A layout prepass is not evidence that a live Slate reorder succeeded. Avoid a separate viewport overlay positioned each frame from cached inventory geometry. Verify focus after activation, return from details, changing Shell/Color/Templates and closing the menu. Preserve the game's own selected/focused styling through its existing controls where possible.

### Input ownership and CommonUI

Slate directional navigation works without CommonUI. CommonUI's input configs can own movement/look suppression and cursor capture; independent changes to those settings may overwrite one another. Handled replies normally stop further input routing, while unhandled replies continue it. Activatable CommonUI widgets have their own desired focus target. These are conditional facts, not evidence that this game's main tabs use CommonUI. [Epic, Input Fundamentals, UE 5.6](https://dev.epicgames.com/documentation/en-us/unreal-engine/input-fundamentals-for-commonui-in-unreal-engine?application_version=5.6)

Design implication: use the game's existing menu input owner and navigation events. Do not install a second global input mode or analog-stick poller for the integrated page. Trace which existing axis handler drives orbit/zoom, which drives list navigation, and what consumes each event. CommonInput presence or controller glyph changes alone do not establish CommonUI tab routing. A fallback global Slate navigation change would affect unrelated UI and needs stronger justification than this feature provides.

### Pinned UE4SS capabilities

The project's [SDK record](ue4ss-sdk.md) pins UE4SS `d7e7826d415b0332b43439a64e6c87f64019be03` and UEPseudo `b2e876da82b17254c04304746341c8fde0ddb37c`. The corresponding local headers expose:

- `UObject::ProcessEvent(UFunction*, void*)` in `UObject.hpp:215`.
- `UObjectGlobals::StaticConstructObject` in `UObjectGlobals.hpp:142`, plus `RegisterHook`/`UnregisterHook` at lines 267-270.
- ProcessEvent and engine-tick callbacks returning removable IDs in `Hooks/Hooks.hpp:203-204,241-242`, and `UnregisterCallback` at line 288.

These files are under `reference/ue4ss-sdk-d7e7826d/deps/first/Unreal/include/Unreal/`. Upstream pinned source: [UObject](https://github.com/Re-UE4SS/UEPseudo/blob/b2e876da82b17254c04304746341c8fde0ddb37c/include/Unreal/UObject.hpp), [UObjectGlobals](https://github.com/Re-UE4SS/UEPseudo/blob/b2e876da82b17254c04304746341c8fde0ddb37c/include/Unreal/UObjectGlobals.hpp), [hooks](https://github.com/Re-UE4SS/UEPseudo/blob/b2e876da82b17254c04304746341c8fde0ddb37c/include/Unreal/Hooks/Hooks.hpp). The matching local copies were inspected; upstream access can require authentication.

This supports a narrow adapter around existing reflected engine/game objects. It does not establish that arbitrary native Unreal subclasses, virtual overrides or delegate targets can be added simply by compiling a normal C++ class in a UE4SS DLL. Confirm each function signature and property layout against this build. Keep hooks scoped to the live inventory instance, prevent recursive callbacks, mutate UI on the existing game-thread path and unregister callbacks before unloading code.

UE4SS's `register_tab` facility creates tabs in its ImGui console GUI. It is not a game-inventory tab API. [UE4SS official C++ GUI guide](https://docs.ue4ss.com/dev/guides/creating-gui-tabs-with-c%2B%2B-mod.html)

## Preview reuse and limits of current evidence

The game's existing CXX header dump establishes a custom Blueprint navigation surface. `WBP_MGT_Main.hpp` declares `BP_HBC_Menu_Game`, `BP_WS_Menu_Game`, Inventory/Tarstones/Map button references and `MainTabIndex`/`SubTabIndex`; `BP_WidgetSwitcher.hpp` declares `UpdateActiveWidgetFromIndex(Index, SubTabIndex)`. `BPC_UserInterfaceHandler.hpp` declares both thumbstick axis vectors, an input mapping context, CommonInput access and menu enable/disable methods controlling focus, cursor and pause. `BP_DisplayMenuBase.hpp` declares `RotateDisplayActor`, `UpdateDisplayYaw`, `UpdateDisplayVector`, `GetCameraActor` and an `ApplyLighting` boolean. These files were inspected in `/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2/Binaries/Win64/ue4ss/CXXHeaderDump/`. The companion [feasibility note](inventory-integration-feasibility.md) retains the curated local evidence. These declarations support targeting the custom navigation and display owners; they do not reveal live child order, zoom bindings or whether the fourth index is accepted.

The local implementation in [`Appearance::sync_menu`](../native/src/engine.cpp) already finds the menu display character, requires the current player's `CharacterId`, checks compatible skeletons, copies appearance/materials and restores the previous menu appearance. This is code evidence of an existing reuse seam. It does not prove that a new tab keeps the actor alive, retains its camera or lighting, or receives orbit/zoom inputs.

Design implication: preserve the game's preview actor and presentation owner. Treat inventory entry/exit, page changes and display actor replacement as separate lifecycle events. Reacquire instances when identity changes. Existing same-shell guards also mean previewing an arbitrary unequipped shell would require an explicit design change, not just another call to `sync_menu`. Do not mutate gameplay equipment merely to populate a preview.

## Next proofs, in order

1. Record live page IDs/indices, selector children, switcher children, custom navigation arrays, focus targets and input callbacks across all three original tabs. A reflected declaration proves a callable surface, not its event order or bounds handling.
2. Append one inert CSS selector/page through actual parent APIs. Reach it using the same keyboard/controller next/previous controls as inventory, then return to each original tab. Check wraparound, selected styling and menu close before adding wardrobe content.
3. Keep that page active and identify the existing display actor/camera. Prove existing orbit and zoom controls still work while list navigation and accept/back work. Record the axis call path and consumption, not just the final camera movement.
4. Insert one wardrobe list row and details update into bounded native panels. Verify long text, scrolling, the current resolution plus another aspect ratio/UI scale, and controller focus after selection changes.
5. Close/reopen, switch original tabs repeatedly, change player shell, reload/travel and disable/re-enable CSS. Confirm no duplicate children/hooks, stale preview references, cursor/input ownership leaks or changed N wardrobe behavior.

The main risks are hardcoded tab bounds or index mappings, game-specific navigation caches, initialized widget lifecycle/delegate requirements, game-owned preview teardown, focus restoration, and two consumers fighting over analog input. The first two proofs should decide whether full implementation is justified before investing in the final layout.
