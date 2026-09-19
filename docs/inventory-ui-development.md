# Native Inventory integration

CSS 0.2.0 integrates Inventory → CSS and replaces the standalone N wardrobe. Gameplay shells and abilities remain unchanged. This document records the development checks and remaining test limits.

## Layout and assets

The native menu accepts a fourth selector and page. CSS inserts its page between Inventory and Tarstones, then rebuilds the native navigation object's children. `GetMapWidgetIndex()` returns 3 after insertion, rather than retaining its original value of 2.

The native switcher's local canvas is much larger than the viewport UI canvas. In the first measurement it was 3714 × 2558.28, while the outer menu was 1568.47 × 1080.40. Applying viewport DPI coordinates inside that switcher made the entire CSS page appear tiny in the upper-left. The page now uses the parent's local geometry and checks for size changes every 500 ms. It waits for valid geometry before building.

The existing game assets supply the styling:

- Trajan Pro Regular for headings, Crimson Text Regular for labels and body text.
- `T_UI_Nav_TitleBG` for the section strip.
- `T_UI_TopBarHighlightLine` for selection accents.
- `T_UI_DescriptionHeader_Divider` for separators.
- `T_UI_Icon_Shell_BG_Black` for outfit thumbnail backgrounds.
- `WBP_Prompt` for controller and keyboard glyphs, using the game's input action where available.
- The inventory's custom scrollbar thumb brush, copied into the CSS scroll box without importing its analog-input listener.

The original top-menu asset has 80 units of horizontal padding on each side of its three titles. CSS saves the original slots and uses three quarters of that padding for four titles, restoring the captured values when detached. The early prototype removed this padding, causing titles to run together. A one-off developer command repaired that already-running session.

Section spacing uses the measured widths of Shell, Color and Templates, with four equal gaps between those labels and the LT/RT prompts. The left list owns the browse hint. Variant arrows and adjustment hints sit in the right panel. Camera hints sit below the character. Wear, Favorite and template actions are clickable buttons with native glyphs. The equipped row carries an Equipped badge; there is no repeated Wearing message.

The first keyboard screenshot revealed that setting `WBP_Prompt.InputAction` alone produced mouse icons for several keyboard actions. CSS now resolves the actual mapped key into the shipped `E_KeyboardMouseButton` values. Prompts follow the native input method. The color channel heading and controller adjustment follow whichever slider was dragged, without rebuilding the widget during mouse capture.

Local screenshots were captured through the game's loaded Steam screenshot API, then visually inspected:

- `20260914142404_1.jpg`: controller variant panel and local action hints.
- `20260914142818_1.jpg`: corrected keyboard glyphs and favorite hover highlight.
- `20260914143141_1.jpg`: Color page with controller prompts.
- `20260914143143_1.jpg`: New Template page with controller prompts.
- `20260914143739_1.jpg`: Shell page keyboard layout. This exposed a stale template status on the Shell page, corrected afterward.

Captures are under Steam's local `760/remote/2584270/screenshots` directory. The capture command does not upload them.

## Character controls

The native `BP_DisplayMenu_Character` actor retains its lights, post-processing and child cinematic camera. CSS changes camera properties only while its page is active and restores them on exit.

The game exposes stick input through `BPC_UserInterfaceHandler`. Main tabs remain game-owned. CSS queries mapped keys for section navigation and actions, with analog sticks excluded from list navigation.

Inspection of cooked `UpdateYaw` bytecode confirmed scalar `FInterpTo(CurrentYaw, TargetYaw, DeltaTime, 2)`. Two problems affected the first camera implementation:

1. Wrapping the target from +180 to -180 sent the scalar interpolator around the long way.
2. Native interpolation added roughly half a second of lag to CSS's moving target.

Yaw now remains continuous across full turns. CSS integrates a circular dead zone, a gentle response curve and a short velocity transition, then settles the native interpolator to the resulting angle. Zoom and framing use the same input response. The current control map is:

| Input | Character view |
| --- | --- |
| Right stick left/right | Rotate |
| Right stick up/down | Zoom in/out |
| Left stick left/right | Frame left/right |
| Left stick up/down | Frame up/down |
| Right mouse drag | Rotate |
| Mouse wheel | Zoom |
| Left mouse drag | Move framing |
| Right-stick click / Home / Reset view button | Restore the captured native view |

Framing moves the menu camera, not the gameplay character. The old vertical orbit-inversion setting remains readable for save compatibility but does not invert zoom. Saved horizontal rotation inversion still applies. Camera drag must start in the character area; list and slider interaction does not pan the camera. Typing a template name suspends camera controls. Losing application focus cancels a drag.

`inventory_motion_tests.cpp` checks continuous rotation across 180 and 360 degrees, matching displacement at 30/60/144 FPS, diagonal normalization, release distance, inversion, reset, stalls and invalid input. These tests verify the math. The user confirmed the revised rotation felt smooth before the final axis remapping; physical-controller confirmation of the new axis assignment is still separate from those tests.

## Transitions and branding

The cooked Inventory Anim_FadeIn uses 0.5-second opacity and panel-translation tracks, not an overall character zoom. CSS uses a 0.5-second eased panel fade/slide on page entry and section changes, with a shorter 0.16-second exit before closing. The native top-level switcher still owns tab visibility. Ordinary slider updates and row selection do not restart entry animation. Transition updates stop when settled.

`assets/inventory-logo-v1.png` was generated with the built-in imagegen tool from `assets/wardrobe-v1.png`. Prompt: preserve the hollow-shell/moth/thorn emblem and the text CUSTOM, SHELL SYSTEM, by _eins0fx; remove the old panel and background; create a compact transparent lockup for the native Inventory menu. The generated RGBA artwork is retained unchanged. The older panel remains publishing/source artwork only.

The first settled logo screenshot (`20260914150349_1.jpg`) showed the author line touching the section strip. Moving the logo up crowded the main navigation (`20260914150857_1.jpg`). The corrected layout preserves the logo at y=65 and moves the entire section strip down 30 normalized units, including its prompts, hit targets and highlight. The outfit list stays in place.

The final live screenshot (`20260914151121_1.jpg`, 1324 × 928) shows about 20 pixels between the author line and the tabs, and 11 pixels between the tab border and the list. The previous tab-to-list gap was about 36 pixels. Both development and shipping DLL builds pass with this layout.

The selected section uses the native `T_UI_TopBarHighlightLine` beneath its title. The earlier 2-unit height compressed its soft line until it was nearly invisible. The replacement preserves the native 10-unit height, 60% opacity and 1.5-times-title width. Alpha is applied to image tint so panel transitions preserve it. The settled Shell screenshot `20260914151539_1.jpg` verifies the visible underline.

## Lifecycle and support fixes

The game owns menu pause, input and display actors. CSS no longer creates a standalone paused preview or calls its old pause wrapper. The old N shortcut, Back+Y shortcut, preview light adapter and generated wardrobe background are removed from runtime and new release payloads. The compact transparent `assets/inventory-logo-v1.png` replaces that background above the section tabs. The older lighting investigation is retained as historical notes and an ignored local archive.

The Inventory adapter detects a replaced player controller or menu and reattaches. A recoverable integration exception retries after two seconds instead of permanently disabling the page. No callback points into an unloaded core DLL.

The support log from Irmassidarkstar uses the same UE4SS commit as the tested local installation. It contains a standalone wardrobe pause error. This supports correcting CSS's behavior, not telling the user to replace UE4SS. Fresh state no longer shows the misleading CSS-is-off instruction. Package lookup starts from the game executable, independent of a redirected UE4SS Mods directory. Diagnostics record the resolved package folder and outfit count; an empty native list explains that a CSS outfit package and restart are needed.

The reporter's exact empty-catalog trigger remains unconfirmed. The supplied log does not establish their installed outfit filenames. Removing the old pause path and testing local startup do not prove every remote installation is fixed.

On the first game-thread tick, package lookup now prefers Unreal's `ProjectContentDir`, resolved with `ConvertRelativePathToFull`, when its Paks directory exists. Executable-ancestor lookup remains the fallback. Both routes ignore the UE4SS Mods directory. Live reflection resolved the expected folder and retained all eight outfits on the local Steam installation. Unit tests cover alternate platform directory names, engine-path priority and inaccessible/missing candidates. These are path tests, not verification of GOG or Xbox game binaries.

## Verification and remaining checks

- Five C++ suites pass: data, colors, startup, recovery and inventory motion.
- Seven release ZIP tests cover the allowlist, checksums, core selection, exclusion of personal data and old artwork, and compatibility with earlier archives.
- Live fresh-state core reload regenerated state and found all eight installed outfits. Selecting an appearance and switching native tabs worked. Original personal state was restored. Evidence: `work/support-first-use/native-1789366962049851369/`.
- Actual mouse input toggled Favorite and restored it, rotated with right drag, zoomed with the wheel and moved framing with left drag. Evidence: `work/inventory-integration/development/mouse-interaction.json`.
- Actual mouse Save created a temporary template. Rename, load and delete were exercised through the core request path, then the original preset collection was checked.
- The direct developer HandleGameMenu opening command produced Inventory over the gameplay camera, captured in `20260914144122_1.jpg`. It bypassed the native input sequence. Refreshing the target alone did not repair it. Real Escape/I input restored the display, verified visually in `20260914144821_1.jpg`. That direct-open command was removed.
- The same real-input regression found a missing CSS Back binding. CSS now handles mapped IA_Menu_Back and a clickable Close button through Inventory's native close path. Three real Escape/I cycles passed with the correct display on Inventory and CSS: `work/inventory-integration/development/1789368889921652534-menu-reopen-real-input.json`.
- Shell, Color and Templates were visually reviewed with native fonts, textures and prompts. Keyboard glyphs and contextual hints were checked in screenshots, not inferred from widget properties.
- Real Home input restored yaw, zoom and both framing axes after a displaced view. Evidence: `work/inventory-integration/development/reset-home.json`. The controller screenshot `20260914150857_1.jpg` also shows the right-stick-click Reset view glyph.

The missing-state test used live core reload, not a cold launch on the reporter's machine. Long travel/death sessions, more viewport sizes, other UI mods and physical-controller testing of the final axis map remain broader regression work. Developer probes and synthetic-input commands are compiled out of shipping builds.

## References

- [Epic: cached widget geometry](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/UWidget/GetCachedGeometry). Cached geometry may be unavailable before painting; CSS reads its parent and waits for valid bounds.
- [Epic: viewport DPI scale](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/UWidgetLayoutLibrary/GetViewportScale). Viewport scale alone does not account for this menu's inner scaled canvas.
- [Epic: Enhanced Input](https://dev.epicgames.com/documentation/unreal-engine/enhanced-input-in-unreal-engine). Dead zones, response modifiers and input smoothing.
- [Microsoft: thumbstick normalization and dead zones](https://learn.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput). CSS reads the game's input, rather than installing another XInput poller.
- [Epic: Blueprint Paths Library](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UBlueprintPathsLibrary) and [relative-path resolution](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UBlueprintPathsLibrary/ConvertRelativePathToFull). The game supplies its own content directory; no store-specific install root is required.

Cooked game assets and the live reflected signatures establish the actual shipped behavior. The public documentation describes the techniques, not Mortal Shell II's implementation.


## Preview physics verification, 2026-09-20

The V30 menu character retained `bDisablePostProcessBlueprint=true` and had no post-process instance after its mesh was replaced. Enabling that flag alone did not instantiate the graph. A controlled engine call retaining the existing override and requesting animation initialization created the missing preview instance. A second test then showed player chest stiffness changing from 115 to 88.82643960980423 while the menu remained at 115.

`Appearance::sync_menu_physics` now enables the authored preview graph, initializes a missing instance once, and mirrors only SpringBone/AnimDynamics roots actively controlled by CSS. The preview captures its own original node values and disable flag. Removing overrides or releasing the preview restores those values. Slider changes preserve both the preview's main and post-process instances; the player is never reinitialized for this synchronization.

Windows build and live V30 checks pass: 64 assertions cover all 30 spring nodes matching the player under tuning, exact original restoration and stable instance identity. Another 29 assertions cover saved profile load, disable/enable restoration, preview disable-flag restoration and recovery after a controlled return to the stock mesh. Existing profiles and the current selection are restored exactly. Evidence is in `work/nextgen-live-preview-{mismatch,fixed-verdict}.json` and `work/nextgen-live-lifecycle-verdict.json`.

These are SpringBone and controlled stock-mesh recovery proofs. Actual travel/death and the experimental AnimDynamics adapter remain unverified. A null post-process instance must not be treated as proof that a visually animated preview has secondary physics.


## Game-window motion clips

`tools/css_window_clip.py --window-id 0x... --seconds 10 --output work/name.mp4` records the validated Mortal Shell Xwayland window directly, without desktop focus changes. It uses FFmpeg's explicit `window_id`, records no audio and never falls back to a monitor. `--orbit` records a bounded turn and return, then restores the original view. Scripted filming interpolation runs before the focus guard; actual user keyboard/mouse input remains behind that guard.

The live 10-second idle and 18-second orbit clips contain 299 and 539 frames at 1920x1080. Frame samples show a stable silhouette and ponytail movement during the turn. Weapons obscure parts of the hair, and the feet are outside this framing, so these clips do not establish full collision or footwear acceptance. The clips, timing metadata and sampled contact sheets are `work/nextgen-live-{idle,orbit}.*`. Steam's screenshot API remains the still-capture path.
