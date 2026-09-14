# CSS inside Inventory: live feasibility findings

Date: 2026-09-14. Status: integration points verified; a new working tab has not been implemented or tested.

## Assessment

The requested layout is feasible enough to justify a small runtime prototype. The open Inventory screen exposes its selector, page switcher, navigation object and character display. CSS can target these existing objects through its native reflection adapter. This does not yet prove that the game's callbacks accept a fourth page or that the display survives switching to it.

Keep the N wardrobe available throughout development. The integrated page should use the game's menu ownership and character presentation, with the same CSS appearance catalog and custom state behind both interfaces.

## Proposed layout

Top navigation: **Inventory / CSS / Tarstones / Map**.

| Left panel | Center | Right panel |
| --- | --- | --- |
| Shell: scrollable outfits and their variants | Existing inventory character display | Outfit name, author, variant and available customization |
| Color: editable parts and color controls | Same display and lighting, updated appearance | Selected part, values and reset controls |
| Templates: named saved configurations | Preview of the selected configuration | Included appearance/settings, save/load/rename/delete actions |

Follow the game's mapped top-tab, sub-tab, navigation, accept and back actions. Reserve right-stick orbit and left-stick zoom/framing for the active CSS page, retaining vertical-only inversion. Mouse and keyboard controls should reach the same actions. Actual analog routing, pitch and zoom remain to be proven.

Templates can build on existing CSS presets in its independent state file. The backend supports named selection/color maps; the current wardrobe exposes three look slots. Missing packages or removed variants must produce a useful message instead of applying an unrelated fallback. No gameplay shell switch or game-save edits are required for this design.

## What the live inspection established

Evidence: `work/inventory-integration/live-inventory.json` and `live-inventory-camera.json`. These are local development artifacts, outside the release ZIP.

| Area | Observed | Remaining proof |
| --- | --- | --- |
| Top menu | `WBP_Menu_Game.WBP_Menu_Main` is `WBP_MGT_Main_C` | Callback behavior when a fourth page is added |
| Selector | `BP_HBC_Menu_Game` has three `WBP_NB_Menu_C` children, indices 0/1/2 | Rebuilding cached navigation and selection styling |
| Pages | `BP_WS_Menu_Game` contains Character, Tarstones Single and WorldMap, in that order | Activating a new page without breaking the original pages |
| Index coupling | `GetMapWidgetIndex()` returned 2 with success | Every route that assumes the old ordering |
| Navigation | Selector owns `BP_PanelWidgetNavigation_C`, including `AllNavigableWidgets`, active index and active widget | Native next/previous, wraparound, accept/back and focus restoration |
| Character display | Active `BP_DisplayMenu_Character_C` owns a `BP_Character_Menu_C` preview | Retaining this display while CSS is the active page |
| Lighting | Display owns left/axe rect lights, fill/rim spotlights and `PostProcess_DisplayMenuCharacter`; `ApplyLighting` and `IsActive` were true | Visual comparison after adding the new page |
| Camera | Display has a camera child component and a live `CameraState_Menu_C` | Effective camera actor and working orbit/pitch/zoom path |
| Input | UI handler exposes both thumbstick axis vectors and its mapping context; Inventory listeners are enabled | Mapping opaque enum values to actions and preventing double consumption |
| Pause | Game menu was open with `PauseGameCounter=1` | Repeated tab changes, close and reload without ownership leaks |

The generic display `GetCameraActor` returned no actor. The camera component summary has no `child_actor` result because the conditional reflected getter path did not produce one. This is not proof that the camera child is absent. Read its reflected `ChildActor` property or inspect the active camera framework during the prototype. `RotateDisplayActor`, `UpdateDisplayYaw` and `UpdateDisplayVector` are exposed, but none was exercised during this inspection. Translation alone is not proven zoom support.

Input listener values were captured as numeric enum IDs. The generated enum labels do not identify physical buttons. Do not hardcode those IDs based on guesses.

## Integration constraints

### Tabs are custom Blueprint navigation

The presence of CommonInput is not evidence that the top menu uses CommonUI tabs. This menu uses a custom horizontal container, navigation object and widget switcher. Adding only a visible CSS button would leave routing and cached child lists inconsistent.

Final ordering changes Tarstones from index 1 to 2 and Map from 2 to 3. The successful live Map getter returning 2 makes this a concrete concern. Trace direct Map/Tarstones entry, menu reopen, next/previous and sub-tab restoration before changing the order. A temporary appended placeholder is useful for proving the fourth page without simultaneously renumbering the original pages.

Epic also documents that `InsertChildAt` alone does not update the live Slate hierarchy. Use supported parent operations and prove the rebuilt visual order and navigation order agree. See [primary-source research](inventory-integration-primary-sources.md).

### Preserve the native display owner

Reuse the inventory display actor, its lighting, post-processing and camera state. The existing `Appearance::sync_menu` already updates the menu character's mesh/materials, guarded by the current player's CharacterId and skeleton compatibility. That is the appearance seam, not proof of complete new-page lifecycle support.

Do not transplant the standalone wardrobe's paused preview copy or global input-mode changes into the game menu. Let the existing menu continue owning pause, focus and cursor state. Enable CSS analog handling only while its integrated page is active; remove it before returning control to another page.

### Lifetime and reload

Track menu/display identity rather than retaining pointers across travel or recreation. Attach once per live menu, detach only CSS-owned widgets and restore original routing when disabling or reloading. The permanent loader owns callbacks; the reloadable core must continue following the existing callback ownership model. All UI mutations belong on the existing game-thread path.

## Next proof steps

1. Append an inert, reversible CSS page and selector. Test native keyboard/controller tab navigation, wraparound, original page behavior and closing. Trace the index-dependent callbacks, then prove the final Inventory/CSS/Tarstones/Map ordering.
2. Retain the native character display on CSS. Identify the effective camera and prove rotation, pitch, zoom and framing while list navigation remains independent.
3. Add Shell/Color/Templates using existing CSS state and appearance operations. Test scrolling, focus, missing packages, menu reopen, reload, travel and another aspect ratio. Verify N still works independently.

No tabs, input bindings or camera transforms were changed during this investigation. A temporary diagnostic core was compiled in the detached `work/inventory-integration/native-probe` worktree, loaded for read-only snapshots and removed afterward. The original core `css_core-ddeb827a207f25a4-1789356524511079680.dll` was restored and acknowledged by the loader. Existing root lighting experiments were left untouched.

Curated SDK headers are retained under `work/inventory-integration/sdk/`. Online API findings and version limits are in [the primary-source note](inventory-integration-primary-sources.md). Current conclusions describe the inspected game/UE4SS build, not guaranteed compatibility with future game updates.
