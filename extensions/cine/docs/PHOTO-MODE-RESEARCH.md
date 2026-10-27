# Photo Mode integration research

27 October 2026. Offline package evidence from Astra (lane A), plus Fable's live reads in the last section. No game state was changed by either.

The native Photo Mode offers reusable tab, row and navigation machinery, but adding a CINE tab is not yet proven live. Its page/row construction is designer-authored; each option row then spawns a logic object. The clean integration boundary is a page that follows the game's tab/interfaces and owns CINE-specific row logic. Reusing a game option object would also run that option's game-setting behavior. Sources: [widget analysis](../work/photo-mode/astra-a/widgets/WIDGETS.md), `WBP_NB_Photomode.CreateLogicObject`, `BP_UIL_Option_Photomode.ChangeSetting`, `WBP_MGT_Photomode.HandleTabSwitch`.

## Evidence and limits

The [Lane A report](../work/photo-mode/astra-a/REPORT.md) is the handoff. The [methods and evidence index](../work/photo-mode/astra-a/METHODS.md) records commands, source hashes and retained dumps. Initial extraction produced 155 package JSON files containing 2,611 exports and 1,350 functions. The reader used the old September usmap. Its `failed 0` count hides two object-level decode errors: `Default__BPC_PhotographyMode_C.HandHeldEffect` and `Default__CameraState_PhotoMode_C.CollisionSettings`. Native CDO values and offsets must not be copied from those results. Sources: [export log](../work/photo-mode/astra-a/export.log), [manifest](../work/photo-mode/astra-a/export-manifest.json).

Blueprint statements and member references cited below are from the new packages. Native field offsets, defaults, implementation details and runtime behavior are separate questions. The old `Sparta.hpp:6814-6847` declaration is retained and clearly marked [OLD BUILD](../work/photo-mode/astra-a/old-build-photography-header.txt). `@N` below is a Blueprint `ScriptBytecode` statement index.

## Entry and ownership

The action is owned by `/Game/Sparta/UI/Menu/Options/WBP_MOT_System`, not `WBP_GamePaused`. The System page has seven designer `WBP_NB_System_C` entries under `BP_VBC_System`: Resume, Activate Photo Mode, Restart, Load, Save, Main Menu, Exit. Load and Save are serialized disabled. The Photo Mode row uses `ST_Core_Photomode:ST_ActivatePhotomode`, and its availability follows `IsPlayerInterfaceEnabled`. Source: [widget analysis, Pause menu](../work/photo-mode/astra-a/widgets/WIDGETS.md), named row exports and `UpdateButtons@267-337`.

Confirmed activation flow, `WBP_MOT_System.HandlePhotomodeButton`:

1. State byte 3 is `ENavigableState::Confirmed`.
2. `OnMenuClose()` on the System page (`@50`).
3. Owning controller's `BPC_UserInterfaceHandler.HandleOptionsMenu(0,true)` (`@139`).
4. `GetPhotomodeComponent()` (`@181`).
5. `BPC_PhotographyMode_C.ActivatePlayerMode()` (`@205`).

The UI handler closes the existing options menu, removes its widget, restores HUD/input state and clears its menu flag. A runtime extra entry is structurally possible by adding a `WBP_NB_System_C`, binding its own confirmed behavior and refreshing the container's navigation. The container-wide confirmed event only plays selection audio. It is not a generic action dispatcher. Sources and exact close calls: [widget analysis](../work/photo-mode/astra-a/widgets/WIDGETS.md), `BPC_UserInterfaceHandler.HandleOptionsMenu`.

`BPC_PhotographyMode.ActivatePlayerMode` checks its inherited availability call, caches defaults, selects player controls, activates, sets camera mode 4, restores prior user values and creates the panel. `ActivatePhotoMode` sets active/focus flags, adds the selected control IMC at priority 3, adds `State.PhotographyMode` and pauses. Full exit is `ResetPhotoMode`, which saves player settings, restores defaults, clears state/tag, resets lighting/weather/dilation and closes the panel. `DeactivatePhotoMode` alone clears focus and input state but leaves active state to the full lifecycle. The native `OnActivate` callback selects developer controls first, so it is not equivalent to `ActivatePlayerMode`. Sources: [component analysis](../work/photo-mode/astra-a/COMPONENT.md), named functions and statement indices.

## Panel and tabs

`WBP_MGT_Photomode` has `ScaleBox_0/Overlay_Main` at its root. `Overlay_Main` contains separate grid, UI, logo, shutter and confirmation siblings. Under `Overlay_UI`, the tab strip is `BP_HBC_Menu_Options` (`BP_HorizontalBoxContainer_C`) and the page switcher is `WS_Tabs` (`BP_WidgetSwitcher_C`). The bottom key bar is `PhotoModeBottomPrompts`, with ten serialized prompts. Exact tree, slot references and actions: [widget tree](../work/photo-mode/astra-a/widgets/widget-trees.txt) and [widget analysis](../work/photo-mode/astra-a/widgets/WIDGETS.md).

| Index | Tab instance | Icon in `UI/PhotoMode/Textures` | Page suffix | Serialized row widgets |
| --- | --- | --- | --- | --- |
| 0 | `WBP_NB_Menu_Gear` | `T_UI_Photomode_Gear_Icon` | GeneralSettings | 11 |
| 1 | `WBP_NB_Menu_Camera` | `T_UI_Photomode_Camera_Icon` | CameraSettings | 5 |
| 2 | `WBP_NB_Menu_Lens` | `T_UI_Photomode_LensIcon` | LensSettings | 4 |
| 3 | `WBP_NB_Menu_Brightness` | `T_UI_Photomode_Brightness_Icon` | BrightnessSettings | 7 |
| 4 | `WBP_NB_Menu_Color` | `T_UI_Photomode_dotIcon` | ColorSettings | 9 |
| 5 | `WBP_NB_Menu_FX` | `T_UI_Photomode_FX_Icon` | FXSettings | 9 |
| 6 | `WBP_NB_Menu_Eye` | `T_UI_Photomode_Eye_Icon` | VisibilitySettings | 8 |
| 7 | `WBP_NB_Menu_Cloud` | `T_UI_Photomode_CloudIcon` | FogSettings | 12 |

Each tab is `WBP_NB_Menu_Image_C`; page names expand to `WBP_MOT_Photomode_<suffix>`. Counts are serialized widget counts, not proof every row is visible or usable. There are 61 rows with explicit option logic classes, represented by 61 `BP_UIO_Photomode_*` packages. Sources: [designer rows](../work/photo-mode/astra-a/widgets/designer-rows.tsv), [option-row bindings](../work/photo-mode/astra-a/options/row-labels.json).

A page contains its own scroll box, vertical navigation container, row widgets and input listener. Regular `WBP_NB_Photomode` and slider `WBP_NB_Photomode_Slider` call `CreateLogicObject`, spawn `LogicObjectClass`, wire UI/controller/player references and initialize it. The regular row guards against replacing a valid existing object; the slider path lacks that guard. `HandleNavigableInput` forwards to `OptionLogicObject.ChangeSetting`; previous/next calls reach `SettingsToChange(...,-1/+1,...)`. Value changes need the compatible logic path, not just new TextBlock text. Source: [widget analysis, Pages](../work/photo-mode/astra-a/widgets/WIDGETS.md).

Tab selection calls `HandleTabSwitch`, which casts the active page to `WBP_Menu_Option_Tab_C`, checks `CanChangeTab`, then changes switcher index. The switcher dynamically iterates its children and calls `BPI_Menu_Generic.OnMenuOpen/OnMenuClose`. Panel focus/refresh paths also iterate current children. A generic UserWidget would fail the page contract. `BP_PanelWidgetNavigation.GetNavigableChildren` clears and rescans child widgets, which provides an offline basis for adding rows and tabs at runtime. Sources: [widget analysis, Runtime addition](../work/photo-mode/astra-a/widgets/WIDGETS.md), supplemental navigation bytecode.

Proposed insertion sequence, pending live validation: append a matching icon/page index; use a page derived from `WBP_Menu_Option_Tab_C` implementing the menu and Photo Mode interfaces; initialize CINE-owned row logic; rescan strip/page navigation; verify index preservation and delegate counts. Reopening, all-reset, input-device changes and removal must be tested. Existing page reuse also brings its option-specific bindings. No reusable blank-page implementation was established by this research.

## Camera, input and time

The current packages' camera code maps mode 1 to third-person, 2 fixed, 3 tracking and 4 free movement. Free movement uses raw player movement, control rotation, camera speed and vertical input. The camera delta is `min(delta / globalTimeDilation,0.033333)`. It sets ignore-move-input on entry and clears it on revert. The collision setup explicitly writes numeric channel/resolution/sweep settings, but native enum meanings and the damaged default layout still need checking. Source: [component analysis, Camera](../work/photo-mode/astra-a/COMPONENT.md), `CameraState_PhotoMode` function citations.

`IMC_PhotoMode_PlayerControls` suppresses lower-priority mappings. Its 28 default mappings cover movement, rotation, crane, exit, reset, Skip Frame, Take Shot, UI visibility, UI focus, pause and grid. `IM_PhotoMode_UserInterfaceInFocus.ModifyRaw` zeros input when the component's `bIsPlayerUserInterfaceInFocus` is true. It is assigned to move stick, WASD, mouse and keyboard crane; gamepad right-stick and trigger axes use different paths. Normal menu entry does not depend on the developer activation IMCs, which are added behind `AllowRuntimeDebug`. Sources: [all input defaults](../work/photo-mode/astra-a/INPUT-ASSETS.md), [component analysis, Input](../work/photo-mode/astra-a/COMPONENT.md).

The panel separately swaps priority-99 menu mappings. `UpdateMapping(true)` adds `IMC_Menu_Default` and removes `IMC_Menu_Photomode`; false reverses that pair. Open/camera focus uses false, menu focus and close use true. Both changes ignore held keys until release. Preserve this lifecycle when adding CINE input. Source: [widget mapping analysis](../work/photo-mode/astra-a/widgets/WIDGETS.md), `WBP_MGT_Photomode.UpdateMapping@245,431,722,908`.

`Pause` calls `GameplayStatics.SetGamePaused(true)` and `SpartaPlayerController.SetShouldPerformFullTickWhenPaused(true)`. Unpause calls both false. Game Speed runs `slomo <preset value>`. Skip Frame unpauses, waits 0.033333 real seconds through `DelayRealTime`, then pauses. This is not a guarantee of one rendered frame. Actor, animation, physics and audio behavior during that interval remains a live measurement. Sources: [component analysis, Pause](../work/photo-mode/astra-a/COMPONENT.md), [Gamespeed option](../work/photo-mode/astra-a/options/OPTIONS.md).

## Capture, visibility and weather

Take Shot is handled by the panel. It guards against an open prompt or running shutter animation, then plays `ScreenshotAnimation`. Its event channel calls HideCursor, TakeScreenShot and ShowCursor. `TakeScreenShot` calls native `SpartaPhotographyModeComponent.TakePhoto(component.ScreenshotSize,Overlay_UI)` and broadcasts the screenshot event. The screenshot-size option exposes 1..4 with a `{Size}x` display. Neither the native engine capture function, output dimensions nor the absolute save directory was verified. Source: [widget capture chain](../work/photo-mode/astra-a/widgets/WIDGETS.md), [ScreenshotSize option](../work/photo-mode/astra-a/options/OPTIONS.md).

Hide UI changes `Overlay_UI` and navigation/input blocking. Grid, logo, shutter and confirmation are sibling widgets, so UI hiding must not be treated as hiding every photo overlay. Higher screenshot multipliers disable logo options; the shipped string table explains that behavior. Sources: [widget analysis](../work/photo-mode/astra-a/widgets/WIDGETS.md), [option analysis](../work/photo-mode/astra-a/options/OPTIONS.md).

Character visibility is tag-filtered and tracked for restoration. Equipment visibility uses native `WeaponsComponent.HideWeaponsInSlots/ShowWeaponsInSlots` with three slot groups. Screen effects use player post-process enable/disable separately from HUD hiding. Weather VFX selection calls `PhotoModeWeatherVFX(index)` on the player's weather component. Weather presets separately call `SpartaWeatherStatics.AddWeatherPreset(...,9999.0)` and retain a WeatherID for removal. Sources: [component analysis, Visibility and weather](../work/photo-mode/astra-a/COMPONENT.md), [option catalog](../work/photo-mode/astra-a/options/OPTIONS.md).

## Complete option and label references

[OPTIONS.md](../work/photo-mode/astra-a/options/OPTIONS.md) gives one sourced row for every `BP_UIO_Photomode_*`: label key, actual driven property/function, setter statement and page/row. It distinguishes camera post-process writes from lighting post-process writes. Copied but unused helper functions are not treated as active behavior.

[LABELS.md](../work/photo-mode/astra-a/options/LABELS.md) contains all 95 English source strings: 19 core, 69 settings and 7 weather preset labels. [labels-en.json](../work/photo-mode/astra-a/options/labels-en.json) preserves namespaces, package paths and exact keys, including `ST_CaneUp`. These are source strings from exported assets, not a promise about another active locale.

## Patch binding audit and live handoff

The source binding audit is [BINDINGS.md](../work/photo-mode/astra-a/bindings/BINDINGS.md). It records file/line sources, new-list package matches, Blueprint function checks and unresolved native/dynamic targets across CINE, CSSX and CSS. Asset existence is not a working-binding guarantee. The [Lane A report](../work/photo-mode/astra-a/REPORT.md) has the break summary and ordered live checks.

Fable should first refresh native reflection for the photography component and camera collision types. Next inspect live System/panel/row object paths and option initialization. Capture validation must identify the native `TakePhoto` signature, actual engine route and saved output. A later controlled temporary tab/row test should measure navigation rescan, duplicate delegates, focus, layout, reset and removal. CINE's shot lifecycle must restore the native Photo Mode camera/time/settings state as well as its own state. These are open validation tasks, not implementation results.

## Live facts (Fable, 27 October 2026)

Read through the CSSX dev bridge on the patched game (exe dated 2026-10-23), read-only. Raw replies:
`work/photo-mode/live/classes.json`, `classes2.json`, `sigs.json` (script `sigs.py`, `pm.py`).

- The controller (`BP_PlayerController_C`) holds the component as property `Photography Mode Component`,
  class `BPC_PhotographyMode_C`, parent `/Script/Sparta.SpartaPhotographyModeComponent`.
- Native component: 33 properties, 20 functions. Signatures: `TakePhoto(ControlsToHide, ScreenshotSize)`,
  `StepFrame()`, `Pause()`, `Unpause()`, `HideCharacters(CharacterId)`, `IsPlayerInterfaceEnabled() -> bool`.
- Values out of Photo Mode: `bIsPhotoModeActive=false`, `CameraMode=4`, `CameraPriority=3`,
  `TimeDilationPresets=[0.1,0.2,0.5,1,1.25,1.5,2]` at index 3 (1.0),
  `CameraSpeedPresets=[50,100,250,500,1000,2000,5000,10000]`, `ScreenshotSize=1`,
  `CameraAspectRatioPresets` = 4:3, 16:9, 16:10, 21:9, 32:9 (no portrait ratio, so CINE keeps its own 9:16 guide),
  `WeatherPresets[0]=null` then `SpartaWeatherPreset` objects, `XboxCaptureDelaySeconds=0.1`.
- `WBP_MGT_Photomode_C` members match the package tree: `WS_Tabs`, `BP_HBC_Menu_Options`, eight
  `WBP_NB_Menu_*` tabs, eight `WBP_MOT_Photomode_*` pages, `PhotoModeBottomPrompts`, `Overlay_UI`.
- `WBP_MOT_System_C` holds `WBP_NBS_ActivatePhotomode` and the other six `WBP_NBS_*` rows in `BP_VBC_System`;
  rows are `WBP_NB_System_C` (members `Text`, `Button_Text`, `WBP_NavigationButton`, `InitText`).
- Tabs are `WBP_NB_Menu_Image_C` (`IconTexture`, `IconSizeX/Y`, `UpdateImage`, `CommitSize`, `CommitScale`).
- Engine functions CINE will call resolve with these parameters:
  `GameplayStatics.CreateSound2D(WorldContextObject, Sound, VolumeMultiplier, PitchMultiplier, StartTime,
  ConcurrencySettings, bPersistAcrossLevelTransition, bAutoDestroy)`, `SetGlobalTimeDilation`, `SetGamePaused`,
  `IsGamePaused`; `AudioComponent.Play(StartTime)`, `Stop`, `SetPaused(bPause)`, `GetPlayState`;
  `AudioMixerBlueprintLibrary.StartRecordingOutput(WorldContextObject, ExpectedDuration, SubmixToRecord)` and
  `StopRecordingOutput(..., ExportType, Name, Path, SubmixToRecord, ExistingSoundWaveToOverwrite)`;
  `DataTableFunctionLibrary.GetDataTableRowNames`; `KismetSystemLibrary.ExecuteConsoleCommand`.
- Pack data container decision (Fable): stock 5.6.1 exposes no UFUNCTION that writes a `UStringTable`
  (searched engine source for setters; only C++ `FStringTable::ImportStrings`). A `UDataTable` with the engine
  row struct `/Script/GameplayTags.GameplayTagTableRow` carries a plain cooked `FString DevComment`
  (`Runtime/GameplayTags/Classes/GameplayTagsManager.h:33-45`, not editor-only) and is filled by stock editor
  Python (`DataTableFunctionLibrary::FillDataTableFromJSONString`, `Kismet/DataTableFunctionLibrary.h:110`).
  CSSX reads rows with `table.rows`. A table only feeds the tag manager when listed in project settings, so a
  mod table is inert. This replaces the string table plus editor-helper route from lane B.

Still open for the live test: the capture path and save folder of `TakePhoto`, adding a ninth tab and a pause
row live, whether the pause-menu sound mix stays on in Photo Mode, and `StepFrame` frame counts.
