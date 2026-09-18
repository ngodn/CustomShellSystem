# Cartographer (MortalShell2Minimap v0.18.46) — implementation-grade analysis for a native C++ port

Reverse-engineering notes for reimplementing the "Cartographer - Minimap and Dungeon Maps" UE4SS Lua mod (Mortal Shell II, UE5.6) as a native C++ mod. All paths are relative to `ue4ss/Mods/MortalShell2Minimap/Scripts/`. Line refs are `File.lua:line`.

The whole mod is built on one hard rule, repeated in every module: **it is event-driven and retains no live UObject wrappers across ticks.** Every record is Lua primitives captured inside a game callback while the object is alive; removal comes only from game lifecycle events. A C++ port does not need this discipline (it can hold `TWeakObjectPtr`s and use `GetAllActorsOfClass` sparingly), but the *set of game functions it hooks and properties it reads* is exactly what must be reproduced, and that is the bulk of this document.

---

## 1. ARCHITECTURE

### Composition root (`main.lua`)
`main.lua` loads ~30 modules with `loadfile`, each a `Factory.New(ctx)` returning a table, wires them into one `ctx`, then starts the lifecycle. The startup log line (`main.lua:436-446`) is itself a one-line architecture summary. Two hard external dependencies:
- **UE4SS** (the Nexus build for MS2, not stock GitHub).
- **MortalShell2ModUI** — a separate shared mod that draws the in-game settings window. Cartographer will not start without it.

### Dependency graph (essential path in **bold**)
```
main.lua (composition root, hotkeys Ctrl+Del / Ctrl+Shift+Del)
├── Config/Runtime.lua      defaults + normalize + INI load/save        [essential: settings]
├── Config/Schema.lua       declarative tab/row schema for ModUI        [essential: settings UI feed]
├── Runtime/State.lua       one shared mutable state record             [essential]
├── Core/Object.lua         UE4SS reflection wrappers (Unwrap/Valid/…)  [essential]
├── Core/Perf.lua           [PERF] profiler, off by default             AUXILIARY (telemetry)
├── **Runtime/Lifecycle.lua**   scheduler, per-frame tick, visibility, LoadMap quarantine
├── Runtime/WorkBudget.lua  sliced/budgeted background scheduler        [essential for POI/local]
├── Runtime/CombatState.lua COMBAT tab (music-state driven look)        [feature, optional]
├── **Map/NativeWidget.lua**    UMG build, native tile widget, pan/rotate, per-frame pose
│   ├── **Map/Scale.lua**       world↔map calibration constants + math
│   ├── Map/AreaMapState.lua desides dungeon vs overworld               [essential for dungeon]
│   └── Map/DungeonView.lua  SceneCapture2D top-down dungeon view       [feature, optional]
├── **Overlay/TrackerProjection.lua**  player-centered projection + edge policy
│   ├── **Overlay/EdgePolicy.lua**  boundary/clamp math
│   └── **Overlay/IconScale.lua**   distance→size quantization
├── **Overlay/TrackerPool.lua**   5 user-pin images                     [feature]
├── **Overlay/ObjectivePool.lua** capped map-POI images                 [feature]
├── **Overlay/LocalPool.lua**     capped local-interactable images (+plates/chevrons/fade)
├── Overlay/TrailPool.lua    footstep breadcrumbs                       [feature, optional]
├── Overlay/Labels.lua       area-name + cardinal text (fonts)          [feature, optional]
├── Overlay/Frame.lua        map frame ring/bars/vector/compass         [feature, optional]
├── POI/*  (map POIs + user pins)
│   ├── POI/IconResolver.lua icon catalog + category table              [essential for map POIs]
│   ├── POI/Hooks.lua        hooks BPC_WorldMapHandler/Objective         [essential for map POIs]
│   ├── POI/Registry.lua     objective/tracker/filter record store       [essential for map POIs]
│   ├── POI/StateReader.lua  reads objective component state             [essential for map POIs]
│   ├── POI/Audit.lua        sliced sweep of handler.MapObjectives        [essential for map POIs]
│   ├── POI/TrackerLifecycle.lua  user-pin discovery                     [feature]
│   ├── POI/TrackerPersistence.lua  user-pin save-object recovery        [feature]
│   ├── POI/TrackerTelemetry.lua                                          AUXILIARY (drop)
│   └── POI/Projection.lua   zero-rotation world→map (diagnostic helper) mostly superseded by Scale
├── Local/*  (local interactables)
│   ├── **Local/Classifier.lua**  class-name → category, art, tints
│   ├── **Local/Discovery.lua**   hooks ~90 game funcs, event-driven discovery (3455 lines)
│   └── Local/AreaName.lua   region/sub-area name from location volumes  [feature]
├── **UI/ModUI.lua**            adapter to the MortalShell2ModUI host
└── Diagnostics/InteractionProbe.lua                                     AUXILIARY (drop)
```

**Minimal viable on-screen minimap** needs: State, Config/Runtime, Core/Object, Lifecycle, WorkBudget, Map/NativeWidget, Map/Scale, TrackerProjection, EdgePolicy, IconScale, and at least one pool (ObjectivePool for map POIs, or TrackerPool for pins). Everything else is an add-on feature or diagnostics.

**Auxiliary / droppable:** `Core/Perf.lua` (profiler; useful in dev), `Diagnostics/InteractionProbe.lua` (Ctrl+Shift+Del reverse-engineering dump, feeds nothing), `POI/TrackerTelemetry.lua` (Ctrl+Del projection-math dump, renders nothing), `POI/Audit.lua` telemetry parts (but its *sweep* is essential to populate map POIs). `POI/Projection.lua` is a legacy zero-rotation helper superseded by `Map/Scale.lua`.

### Per-frame tick (`Runtime/Lifecycle.lua`)
The whole engine is one self-rescheduling game-thread callback (`ExecuteInGameThreadWithDelay`, `Lifecycle.lua:71-85, 460-482`). `runtime_tick_body` (`Lifecycle.lua:166-443`) each step:
1. If world quarantined/not ready → wait 250 ms (`:169`).
2. Every 3rd step (or when hidden): `UI.ReadActions()` + `UI.ReadShellState()` (`:171-177`).
3. Native-menu gate: if the shared host reports a native menu active, hide once and observe nothing (`:183-192`).
4. Coalesced rebuild debounce (0.30 s) for rebuild-flagged settings (`:214-274`).
5. If not built: `Renderer.Build()` with exponential backoff ladder (1/2/4/8/16 s) (`:292-326`).
6. Visibility decision `should_hide()` (`:120-164, 328-333`).
7. `Combat.Update()` (`:337-341`).
8. If visible: `Renderer.UpdatePose()` → returns `(pose_ok, geometry_changed, pan_due)` (`:344-347`).
9. If geometry changed / refresh requested: `TrackerProjection.Prepare(...)` once, then `TrackerPool.UpdatePlayer` / `ObjectivePool.UpdatePlayer` / `LocalPool.UpdatePlayer` / `TrailPool` / `AreaName.Resolve` / `Labels.Update` / `DungeonView.Update` (`:350-441`).
10. Return `pose_delay()` = 1000/max(panHz,arrowHz) ms (`:58-66`).

`should_hide()` reasons: disabled, settings-preview (own settings keep rendering), modui (foreign shell), native-menu, menu (controller cursor / IsInGameMenu), cinematic (`bCinematicMode`), combat.

---

## 2. MINIMAP RENDERING (`Map/NativeWidget.lua`)

### The key trick: it reuses the game's own world-map tile widget
Rather than draw tiles itself, it constructs the game's native `SpartaMapWidget` and drives it with external zoom/pan. Assets (`NativeWidget.lua:18-29`):
- Tile DataTable `/Game/Sparta/UI/World/Map/Blueprints/DT_MapTiles`
- Tile material `/Game/Sparta/UI/World/Map/Materials/MI_SpartaMapTile`
- Global mask `/Game/.../Textures/T_UI_MapGlobalMask`
- Fog material/texture `MI_UI_MapFog` / `T_UI_Map_Fog`
- Circle mask material `Mat_UI_Circle_Background_Inst`
- Player arrow `T_UI_Icon_Map_PlayerIndicator`

UMG classes used (`:31-39`): `/Script/UMG.Default__WidgetBlueprintLibrary`, `UserWidget`, `WidgetTree`, `CanvasPanel`, `Image`, `RetainerBox`, and **`/Script/Sparta.SpartaMapWidget`** (the native tile renderer).

### Widget tree (built in `runtime.Build`, `:506-736`)
```
UserWidget (AddToViewport, ZOrder 40)
└ WidgetTree.RootWidget = CanvasPanel "Root"          (labels + frame parent, outer)
  └ [circle mode only] RetainerBox "CircleOpacity"    (carries opacity when <1)
     └ RetainerBox "CircleMask" (EffectMaterial=circle_effect, TextureParameter="Texture")
        └ CanvasPanel "Frame" (Clipping=1)             (map content, clips)
           ├ Image "Backdrop"     dark plate (RGBA 0.015,0.015,0.015,0.80), ZOrder 0
           ├ Image "NoMapCloud"   fog plate, ZOrder 1 (collapsed unless Cloud mode)
           ├ SpartaMapWidget "NativeTiles"  ZOrder 2, pivot 0.5/0.5
           └ Image "PlayerArrow"  ZOrder 10, pivot 0.5/0.575
```
Square mode: the Frame is placed directly on Root at `(OffsetX, OffsetY, Size, Size)`; there is no RetainerBox. Circle mode wraps it in two RetainerBoxes (mask + opacity) — see the opacity note below.

### SpartaMapWidget configuration (`:659-671`) — native fields/functions
- `MaxResidentTiles = Config.MaxResidentTiles` (default 8)
- `LODBias = Config.LODBias` (default 2)
- `bAllowMouseInput = false`
- `bExternalZoomAndPan = true`  ← lets the mod drive it
- `TileMaterial = <MI_SpartaMapTile>`
- `SetTileSet(<DT_MapTiles>)`
- `SetGlobalAlphaMask(<T_UI_MapGlobalMask>)`
- `SetRegionBounds({0.5,0.5},{1.0,1.0})`
- `SetRevealAmount(1.0)`
- `SetZoom(<native zoom>)`
- `SetPanCenterNormalized({X,Y})`  ← per-frame
- `SetRenderTransformAngle(deg)`   ← per-frame rotation
- `SetRenderTransformPivot({0.5,0.5})`

### Where on screen / shape / size
Position `(Config.OffsetX, Config.OffsetY)` from top-left (defaults 28,28). Size is a square `Config.Size` px (default 300, range 160–500). Shape: square or circle (`Config.MapShape`). Circle clips the whole subtree to its inscribed circle via the RetainerBox effect material. Size changes are applied **in place** (slot writes, no rebuild) since v0.18.43 (`apply_size_layout`, `:304-350`); position and shape changes still force a rebuild.

### Orientation (north-up vs rotating)
`Config.MapOrientation` = `north` | `camera` | `player` (`:243-257`). Rotation is written to the tile widget's render transform at pan cadence (`UpdatePose`, `:951-978`):
- north: angle 0.
- camera: `MapScale.CameraHeadingMapAngle(cameraYaw) = normalize(90 - cameraYaw)`.
- player: `MapScale.PlayerHeadingMapAngle(playerYaw) = normalize(90 - playerYaw)`.

A rotating **square** viewport is oversized to its diagonal (`× √2`) so corners stay covered (`map_surface_size`, `:258-265`); a circle viewport is not (only the inscribed circle shows).

### Player arrow
Native `T_UI_Icon_Map_PlayerIndicator` Image, size `max(20, round(Size*0.095))`, pivot (0.5, 0.575) with a compensating slot offset so the circular part stays exactly centered (`:52-53, 235-241`). Arrow angle = `normalize(playerYaw - 90 + mapRotationAngle)` (`:1006-1010`): north-up shows facing; camera-up shows facing relative to camera; player-up keeps it pointing up.

### Per-frame pose path (`runtime.UpdatePose`, `:876-1030`)
1. Read pose: `player:K2_GetActorLocation()` + `:K2_GetActorRotation()` → world x,y,z + yaw (`read_pose`, `:843-848`).
2. Push pose to `AreaMapState.ObservePose`.
3. Fractional-phase scheduling: independent pan/arrow channels at their own Hz over a shared base rate, so any 5–60 Hz choice is real (`:919-935`).
4. Camera yaw read only when needed: `PlayerCameraManager:GetCameraRotation().Yaw` (`read_camera_yaw`, `:854-874`).
5. Rotation write (pan cadence, epsilon 0.25°).
6. Pan write: `pan_x,pan_y = WorldPanNormalized(x,y)`, epsilon-gated (sub-pixel), only when `area_map_available ~= false`. Writes `SetPanCenterNormalized` (`:980-1004`).
7. Arrow angle write (arrow cadence, epsilon 0.5°).

Player/controller resolution (`resolve_player`, `:469-485`): `FindFirstOf("BP_PlayerController_C")` then `controller:GetPawn()`, fallback `FindFirstOf("BP_PlayerCharacter_C")`.

### Circle opacity quirk (hard to reproduce, worth knowing)
The circle effect material draws the retained surface's color but ignores alpha and premultiplies, so opacity compounds. It compensates with `opacity^(1/curve)` on an outer effect-less RetainerBox, `curve = Config.CircleOpacityCurve` (default 5.8, measured) (`apply_opacity`, `:120-145`). Square mode is linear (opacity on the frame). A native C++ port that draws its own material can skip all of this and set widget opacity directly.

---

## 3. WORLD → MINIMAP PROJECTION

### Calibration constants (`Map/Scale.lua`)
From the game's `BP_WorldMapSettings_2D` (runtime/FModel proven):
```
WORLD_SPAN_UNITS = 315552.0      (cm; = 3155.52 m across the world map)
WORLD_ORIGIN_X   = 38050.053612
WORLD_ORIGIN_Y   = -134391.872342
MAP_LOGICAL_PIXELS = 4096.0      (Blueprint projection atlas)
TILE_SIZE_PIXELS = 1024, TILE_LEVEL_COUNT = 5
NATIVE_TILE_SURFACE_PIXELS = 1024 * 2^4 = 16384   (finest tile surface)
BASE_NATIVE_ZOOM = 0.25
MAP_INSET_PIXELS = 4.0
```

### World → normalized pan (`Scale.WorldPanNormalized`, `Scale.lua:24-29`)
```
pan_x = 0.5 - (world_x - WORLD_ORIGIN_X) / WORLD_SPAN_UNITS
pan_y = 0.5 - (world_y - WORLD_ORIGIN_Y) / WORLD_SPAN_UNITS
```
This is fed to `SpartaMapWidget:SetPanCenterNormalized`. Player centering is implicit: the pan center is the player's world position, so the player sits at map center. In-bounds test `WorldInsideNativeBounds` (`:31-37`): pan_x/pan_y within [0,1]±ε (used by AreaMapState).

### Native zoom + scale (`Scale.lua:88-98`)
```
NativeZoom(zoom_m) = BASE_NATIVE_ZOOM * (WORLD_SPAN_UNITS/100) / zoom_m
                   = 0.25 * 3155.52 / zoom_m
NativePixelsPerWorldUnit(zoom_m) = NATIVE_TILE_SURFACE_PIXELS/WORLD_SPAN_UNITS * NativeZoom(zoom_m)
                                 = 16384/315552 * NativeZoom
EffectiveVisibleMeters(size,zoom_m) = (size - 4) / NativePixelsPerWorldUnit / 100
```
`Config.ZoomMeters` (25–50000, default 1000) is the "map range" the user sets; the *effective* visible span is much smaller (this scale is the pixels-per-cm at which overlays are drawn).

### Overlay projection (`Overlay/TrackerProjection.lua` + `EdgePolicy.lua` + `IconScale.lua`)
This is the player-centered projection every icon pool uses.

**Prepared context** (`TrackerProjection.Prepare`, `:33-66`): caches `scale = NativePixelsPerWorldUnit(zoom)`, `half_size = Size/2`, rotation basis `(cos, sin)` from the map angle, `circle` flag, `visible_fraction` (edge visibility), and `map_circumscribed_pixels = half_size` (circle) or `half_size*√2` (square).

**Per-marker projection** (`PositionPrepared`, `:81-176`):
```
delta = (marker_world - player_world)                 # cm
offset = rotate( -delta * scale, cos, sin )           # screen px, player at center
distance_m = |delta| / 100
standard_inside, frame_limit = EdgePolicy.Standard(size, offset_x, offset_y, circle)
# on-map:
position_x = size/2 + display_x - rendered_size/2
position_y = size/2 + display_y - rendered_size/2
```
Note the negation: `-delta * scale`. Screen +y is down; world +y is screen up in the native north-up projection, and the rotation basis matches the tile widget's `SetRenderTransformAngle` so overlays stay aligned with the map.

**Edge clamping** (`Overlay/EdgePolicy.lua`): three modes MODE_MAP / MODE_EDGE / MODE_HIDDEN.
- `Standard` (`:104-115`): inside test — circle: `|offset| <= size/2`; square: `max(|x|,|y|) <= size/2`.
- `anchor_distance` (`:91-102`): the visible-depth policy. 50% = marker center exactly on boundary; 100% = whole icon inside; <50% clips more than half. `desired = midpoint - (2*fraction - 1) * inset`, where `inset = midpoint - full_fit`.
  - `midpoint_distance`: circle → `half`; square → `min(half/|ux|, half/|uy|)` along the bearing.
  - `full_fit_distance`: circle solves all 4 rectangle corners against the circle (`circle_corner_root`); square → `min((half-hx)/|ux|, (half-hy)/|uy|)`.
- Off-map markers clamp to `offset * anchor_factor` and get MODE_EDGE, sized down by distance.

**Distance→size** (`Overlay/IconScale.Quantized`, `:6-25`):
```
progress = clamp((distance - boundary_distance) / (max_distance - boundary_distance), 0, 1)
scale    = 1 - (1 - min_scale) * progress
rendered_size = max(1, round(normal_size * scale))     # quantized to whole px
```
So an off-map POI shrinks from full size at the boundary down to `edge_min_scale` at `edge_max_distance`, then hides beyond.

**Broad-phase reject** (`RelevantRadiusSquared`, `:68-79`): `radius = max((circumscribed + marker_radius)/scale, edge_range_cm)`; pools reject markers beyond `radius²` before doing any rotation/sqrt.

`POI/Projection.lua` is a simpler zero-rotation `WorldToMap` returning map/normalized coords, kept mainly for diagnostics (`TrackerTelemetry`). The live path is `Scale` + `TrackerProjection`.

---

## 4. POI / TRACKERS / ICONS

There are **two** distinct discovery systems:

### 4a. Map POIs + user pins (POI/*) — the world map's own icons
**Game classes** (`POI/Hooks.lua:25-28`):
- `BPC_WorldMapHandler_C` (`/Game/Sparta/UI/World/Map/Blueprints/BPC_WorldMapHandler`) — an ActorComponent on the player controller, reached via controller property **`"World Map Handler"`** (note the spaces).
- `BPC_WorldMapObjective_C` (same folder) — the per-POI objective component.
- `WBP_MGT_WorldMap_C` (`.../Widgets/WBP_MGT_WorldMap`) — the map widget.
- `BP_DeathSpoils_C` (`/Game/Sparta/Core/Player/Ability/Death/BP_DeathSpoils`) — dropped-gloom actor.

**Hooked UFunctions** (`Hooks.lua:29-57`):
| Function | Purpose |
|---|---|
| `BPC_WorldMapHandler_C:AddMapObjective` | new objective (arg1 = component) → `Registry.CaptureObjective` |
| `BPC_WorldMapHandler_C:AddMapTracker` (arg1 actor, arg2 index) | user pin add |
| `BPC_WorldMapHandler_C:RemoveMapTracker` / `:DestroyMapTracker` | pin remove |
| `BPC_WorldMapHandler_C:LoadTrackers` | pin bulk-load (POST preferred; deferred-PRE fallback) |
| `BPC_WorldMapHandler_C:SaveLoaded` | bootstrap observe (no scan) |
| `BPC_WorldMapHandler_C:UpdateAreaID` | → `AreaMapState.ScheduleRefresh` |
| `WBP_MGT_WorldMap_C:HasAreaMap` (POST, returns bool) | → `AreaMapState.ObserveResult` |
| `BPC_WorldMapObjective_C:RevealObjective` (PRE) | set `{revealed=true}` |
| `BPC_WorldMapObjective_C:CompleteObjective` / `:UncompleteObjective` (PRE) | `{completed=true/false}` |
| `BPC_WorldMapObjective_C:ShowArea` (PRE) | `{show_area=true}` |
| `BPC_WorldMapObjective_C:SaveLoaded` / `:SessionReset` (POST) | full re-read |
| `BP_DeathSpoils_C:GiveGloom` | retire spoils by owner (NOT the delegate stub `:OnRetrieved`) |
| `WBP_MGT_WorldMap_C:BndEvt__...OnFilterToggle...` | filter state |

**PRE vs POST matters:** state hooks are PRE and capture only the component *address* + apply a hardcoded boolean, because state is only reliably readable before the native transition. `SaveLoaded`/`SessionReset` are POST (component still alive, re-read).

**Registry** (`POI/Registry.lua`): records indexed by primitive UObject address; weak `refs` (`__mode="v"`); generation + quarantine reset on LoadMap-pre (`:535-567`). Records get location/category via `StateReader.ReadFull`. Reconciliation: `RefreshObjective` (full or lightweight state re-read), `CaptureObjective` (event-driven), `ApplyObjectiveState` (apply booleans by address, no read), `RetireObjectiveByOwner` (spoils), `RetireUnseen` (audit sweep). A coalescing FIFO work queue drains in `WorkBudget.RunSlice(4, 0.20, …)`.

**Audit** (`POI/Audit.lua`) — essential populate path: resolves the handler, reads `handler.MapObjectives` (a TArray), walks it in `WorkBudget.RunSlice(4, 2.0, …)` slices rescheduled every 25 ms, retains zero wrappers between slices, dedups by address, retires unseen on finish. Streaming-safe (transient zero-length arrays never wipe a populated registry).

**StateReader** (`POI/StateReader.lua`) reads a `BPC_WorldMapObjective_C`:
- `IconClass` (UObject; short name → IconResolver)
- gameplay tags `ObjectiveTag`, `BiomeTag`, `EquipmentTag`, `OptionalCustomTag`
- owner class short name (`GetOwner()`)
- state via `Core/Object.ObjectiveState` (`Object.lua:241-256`):
  - on component: `Enabled` (bool), `bSaveLoaded` (bool), `RuntimeData` (sub-object)
  - on `RuntimeData`: `Visible`, `Revealed`, `Completed`, `ShowArea`
- location via `component:GetOwner()` → `actor:K2_GetActorLocation()`; fallback `component:K2_GetComponentLocation()`.

**IconResolver** (`POI/IconResolver.lua`) — map POI categories (`Resolver.Categories`, `:72-190`), one per native widget-info class `WBP_WMI_*_C`:
| key | icon_class | default texture (T_UI_Icon_Map_*) | discovery |
|---|---|---|---|
| Hub | WBP_WMI_HUB_C | Spire | only(false) |
| Boss | WBP_WMI_Boss_C | Gate_Locked → Gate_Cleansed (alt Gate_Open) | only |
| Dungeon | WBP_WMI_Dungeon_C | Dungeon_Uncompleted → Completed | show_when_enabled |
| EvilStatue | WBP_WMI_EvilStatue_C | EvilStatue_Discovered → Completed | only |
| LandingArea | WBP_WMI_LandingArea_C | Beacon_Revitalized → Cleansed (alt Uncleansed) | only |
| SmallBeacon | WBP_WMI_SmallBeacon_C | MiniBeacon_Uncleansed → Cleansed | none |
| MapStation | WBP_WMI_MapStation_C | RukStatue | show_when_enabled |
| Shell | WBP_WMI_Shell_C | Shells | hidden(true) |
| Sidearm | WBP_WMI_Sidearm_C | Sidearms | hidden |
| Traversal | WBP_WMI_Traversal_C | Traversal_Unlocked (alt Locked) | only |
| Weapon | WBP_WMI_Weapon_C | Weapons | hidden |
| Spoils | WBP_WMI_Spoils_C | MapActor_GloomLost | none, hide_when_completed |

`Describe(icon_class, objective_tag, owner_class)` (`:249-268`) resolves category by: exact icon_class → tag (`BY_TAG`, e.g. `UI.MapActor.Boss`, `UI.MapActor.LandingArea*` → Landing/SmallBeacon) → owner class (`BP_MapObjective_MapStation_C` → MapStation) → unresolved. Textures under `/Game/Sparta/UI/World/Map/Textures/`. `NativeDimensions`/`FitSize` keep non-square art's aspect ratio (e.g. Spire 32×102). `Paths()` = every distinct texture (for bounded preload).

**User pins** (POI/TrackerLifecycle + TrackerPersistence): live pins from `handler.MapTrackers` (a `Map<int, Actor>`, max `handler.MaxTrackers` default 5); each read via `actor:K2_GetActorLocation()`. `LoadTrackers` capture is POST-hook preferred, deferred-PRE fallback. Save-layer recovery: `handler:GetMySaveData(out{bSuccess})` returns a save object with a `Trackers` map (read via `ForEach`/`Contains`/`Find`); locations from `Object.Vector`, synthetic `actor_id = "saved-index:N"`.

### 4b. Local interactables (Local/*) — event-driven, near the player
**Discovery** (`Local/Discovery.lua`, 3455 lines) hooks ~90 game UFunctions and never scans (except a per-world seed for classes that begin play before hooks arm).

**Hooked classes/functions** (constants `:34-153`, spec table `:212-603`). Grouped by kind:

*Interaction lifecycle* (admit/gate/retire interactables) on `BPC_InteractionHandler_C` (`/Game/Sparta/Core/Common/Interactions/BPC_InteractionHandler`):
`:Initialize`, `:CustomInitialize` (begin), `:ReceiveEndPlay` (end), `:RegisterInteractable`, `:UnregisterInteractable`, `:DisableInteractions`/`:EnableInteractions` (gate marker), `:Invalidate` (consumed → retire Pickup/Chest/Loot/MapFragment by category), `:LockInteraction`/`:UnlockInteraction` (evidence).
Plus `BP_Interaction_C:ReceiveBeginPlay`/`:ReceiveEndPlay` and `BP_NPC_C:ReceiveBeginPlay` (actor-begin/end).

*Retire-on-use* (`owner-retire`, context = the actor):
- Chests: `BP_Interactable_Chest[_General/_Skin/_Seal]_C:OnOpenComplete`/`:OnChestOpen` → chest-opened.
- Pickups: `BP_PickupBase_C`, `BP_CraftingItemPickup_C`, `BP_CurrencyBag_Pickup_C`, `BP_Pickup_Artifact_C`, `BP_Currency_Gold_C` — `:PickUp` / `:SetAsCollected` → pickup-collected.
- Bear trap `BP_BearTrap_C`: `:TriggerTrap` (sprung), `:DisableTrap`, `:PlayDisarmSFX`; admit via `:ListenToPlayerDeath`/`:RearmTrap`.
- Explosive barrel `BP_ExplosiveBarrel_C:Explode` → barrel-exploded.
- Bag teleport `BP_BagTeleport_C:Deactivate` → bag-used.

*Enemies* — see §6. AI classes `BP_AICharacter_C` / `BP_NPC_C` / `/Script/Sparta.SpartaAICharacter` / `/Script/Sparta.SpartaCharacter`. Admission `:ReceiveBeginPlay` (kind `enemy-seen`); death `BndEvt__..._DeathEvent__DelegateSignature` + `:DespawnEnemy` (kind `enemy-dead`); refresh hooks (movement) flagged `refresh=true`.

*Hidden/breakable walls* (`Secret`), none are interactables:
- `BP_HiddenWall_C` (+ `_Base_Natural_Rock`, `_CannonBall`, `_Spikes`): `:DisableCollision` → wall-revealed (open signal); admit via variant `:ReceiveBeginPlay`. State read: `collision_off(actor, "WallCollision")`.
- Rubble `BP_DestructiblePlaceholderWall_C` / base `BP_DestructiblePlaceholder_C`: `:ManualSpawnItem`/`:PercentageSpawnItem` → wall-broken. **It announces its break through nothing** — retirement comes from a 1 Hz `wall_recheck_tick` (`:2013`) that re-acquires by path and checks `BlueprintCreatedComponents` for the solid `Mesh` + `TargetingCollision` being gone (`rubble_wall_broken`, `:1797-1820`).

*Hittable plants/cages* (`Loot`): `BP_FlowerChest_C` (collision `HitDetectionCollision`), `BP_AttackFlower_C` (collision `Capsule`). Hooks: `:ReceiveBeginPlay` (admit), `:SpartaApplyHit` (post; re-read state), retire via `:DropItem__FinishedFunc`/`:OnItemPickedUp`/`:SaveItemCollected`/`:DisableTargeting`/`:DisableFlower`; regrow `:RestartFlower`/`:InitRegrow`. State read `plant_spent_state` (`:1847-1862`): `RewardExtracted` (bool), `HitsReceived`/`RequiredHits` (int), collision-off.

*Env-shooting objects* (barrels, torches) `BP_EnvShootingObjectBase_C`: admit via lock-on component `BPC_LockOnTarget_EnvShooting_C:ReceiveBeginPlay`; state hooks (post) `:OnWorldStateLoaded`/`:GotESOactivated`/`:Activate`/`:ActivationFinished`/`:SpartaApplyHit`/`:Invalidate`; retire `:InvalidateCollision`/`:Invalidate`. State read `env_object_state` (`:1870-1885`): `GotActivated`, `IsReady`, `CanPlayerShootIt`, `CanPlayerTargetIt`, `bCanBeTargeted`.

*Merchants/service*: shop handlers detected by component name containing "Shop"; `BP_Blacksmith_C`/`BP_NPC_Genessa_Training_C:ReceiveEndPlay` (owner-end).

**Hot-hook governor** (`governor_admits`, `:973-1010`): a sliding 1 s window per hook label. If a label exceeds 90 events/s for 3 consecutive windows → retire it ("hot"); >450 in one window → retire ("runaway"). Refresh hooks additionally get a min-gap floor of 1/60 s between serviced events. Runs before quarantine and before touching `context`, so a per-frame hook costs two increments + a clock read. This is how it safely arms speculative movement hooks.

**Budgeted breadcrumb** (`trace`, `:912`): per-window (10 s) crash-diagnosis logging, only when `Config.LocalDiscoveryTrace` is on; brackets every native call so a crash log names the last call. Auxiliary.

**Gate/admit/quarantine:** default-admit ("if I can interact with it, show it") minus world-map duplicates (`Classifier.map_duplicate`). A record is *suppressed* (not deleted) while `hidden` or a handler has `DisableInteractions` (enemies/bosses exempt) (`refresh_suppression`, `:1083-1094`). During LoadMap quarantine, *removal* events are still processed (begin events are not), so load-spawned-and-destroyed actors don't leak. Category-off check `Config["LocalShow"..category]` before enumerating a seed class (`:337-361`).

**Retire triggers summary:** chest → OnOpenComplete/OnChestOpen; pickup → PickUp/SetAsCollected; trap → TriggerTrap/DisableTrap/PlayDisarmSFX; barrel → Explode; plant → DropItemFinished/OnItemPickedUp/SaveItemCollected/DisableTargeting/DisableFlower (+ hit state read); wall → DisableCollision (hidden) or 1 Hz component-gone recheck (rubble); enemy → DeathEvent/DespawnEnemy/ReceiveEndPlay/corpse-check; env object → InvalidateCollision/Invalidate/GotActivated.

**Classifier** (`Local/Classifier.lua`): `Classify(class_name, source_hint)` (`:447-512`) — a priority-ordered `contains()` chain. Source hints (from the hooks) override name matching. Hint precedence `HintRank` (`:515-527`): boss(4) > enemy/trap/secret(3) > merchant(2) > npc/shootable/barrel(1) > 0. `CategoryList()` (`:586-605`) is the 20-category list used to build config keys and the LOCAL settings tab. `ResolveArt(key, iconChoice, colorChoice)` (`:392-405`) → texture path + tint. `IsFriendly` (`:145-150`) keeps friendly NPCs off the enemy layer. `CategoryScale` per-category size multiplier (Trap 0.6, Enemy 0.5, Boss 0.85, MapFragment 2.0, …).

**Rendering pools** — all follow one pattern (see TrackerPool as the archetype):
- Preallocate N `/Script/UMG.Image` widgets as children of the Frame canvas.
- Per tick call `TrackerProjection.PositionPrepared` → `(x, y, rendered_size, mode, visible, …)`.
- `SetPosition`/`SetSize`/`SetVisibility` on the slot, epsilon-gated (0.001 px²) so unchanged markers cost nothing.
- Broad-phase squared-distance reject before projecting.

TrackerPool (`Overlay/TrackerPool.lua`): fixed 5, texture `T_UI_Icon_Map_Tracker_Color`. ObjectivePool (`Overlay/ObjectivePool.lua`): capped (`Config.POIMaxIcons` default 192), sliced construction (batch 4 / 50 ms), textures from IconResolver, per-category edge indicators. LocalPool (`Overlay/LocalPool.lua`): capped (`Config.LocalInteractableBudget` default 64), per-category art via `Classifier.ResolveArt`, aspect-fit, **texture pinning** (a collapsed "keeper" Image holds each texture's engine reference so cached wrappers can't be GC'd), plates (soft black disc `T_UI_Icon_Shell_BG_Black` behind icons), height chevrons (`T_UI_InteractIndicator`, up/down for above/below player by `LocalHeightThresholdMeters`), and height fade (icon+plate+chevron fade together past threshold from `FadeStart` to `FadeMin` over `FadeRange`). TrailPool (`Overlay/TrailPool.lua`): distance-gated breadcrumbs, screen-gap-aware spacing, quantized age fade, max 48.

---

## 5. AREA NAME & DUNGEON MAPS

### AreaMapState (`Map/AreaMapState.lua`) — dungeon vs overworld
Decides `state.area_map_available` (true=overworld tiles, false=dungeon/unmapped, nil=unknown, fails open).
- Authoritative: `WBP_MGT_WorldMap_C:HasAreaMap()` — but only once `widget.CachedMapHandler` is valid (construction guard) (`:116-133`). Reached via `FindFirstOf("WBP_MGT_WorldMap_C")`.
- Fallback: `MapScale.WorldInsideNativeBounds(x,y)` — point-in-rect against the BP_WorldMapSettings_2D bounds.
- Boundary-crossing watcher `ObservePose(x,y,z)` (`:195-219`): recomputes inside/outside each pose; only re-resolves on a crossing (primitive-only, no native call while inside).
- Area id: controller property `"World Map Handler"` → `CurrentAreaID` (via `Object.Tag`).
- Push: `Renderer.SetAreaMapAvailable(value, reason)` on change only, deferred via `WorkBudget.Schedule(0, …)`.

When `area_map_available == false`, the renderer collapses the tile widget and the no-map background applies (Dungeon view / Transparent / Cloud).

### DungeonView (`Map/DungeonView.lua`) — live top-down capture
A `SceneCapture2D` renders the rooms around the player into a render target that becomes the minimap background brush.
- **Setup** (`:892-998`): `UKismetRenderingLibrary:CreateRenderTarget2D(controller, px, px, format, {0,0,0,1}, false, false)`; `world:SpawnActor(SceneCapture2D, playerPos + {0,0,cameraHeight}, {Pitch=-90})`; component `CaptureComponent2D` written: `TextureTarget`, `bCaptureEveryFrame=false`, `ProjectionType=1` (ORTHOGRAPHIC), `OrthoWidth`, `bOverride_CustomNearClippingPlane=true`, `CustomNearClippingPlane = height - slice` (ceiling cut), `CaptureSource` (default 9 in config / doc default 3 = SceneColorSceneDepth), `bAbsoluteRotation=true`, `bAlwaysPersistRenderingState`. Attach `K2_AttachToActor(pawn)`; rotation `K2_SetRelativeRotation({Pitch=-90})`.
- **Texture → brush** (`:1000-1043`): `Image:SetBrushFromTexture(renderTarget, false)` (a `UTextureRenderTarget2D` is a `UTexture`), ZOrder 2 (replaces the collapsed tile layer), pivot 0.5/0.5, rotated to match `map_rotation_angle` + `-90°` offset.
- **Ceiling cut** (auto, `:469-628`): 5 upward `KismetSystemLibrary:LineTraceSingle` probes (center + 4 dirs ±3 m) on `TRACE_TYPE_VISIBILITY`; highest hit sets the near plane so the ceiling doesn't hide the floor; low in corridors, high in halls; fractional smoothing; disables auto after 20 failures.
- **Lighting-rig borrow** (`:635-850`): around each capture it rewrites the level's ONE lighting rig through Blueprint setters and restores it before the player's view renders (all in one game-thread call, since `CaptureScene()` flushes light state). Components found via `FindAllOf`: `DirectionalLightComponent` (`SetIntensity`, default map value 0 lux), `SkyLightComponent` (`SetIntensity`, 1.0), `SkyAtmosphereComponent` (`SetSkyLuminanceFactor`, ×1), `ExponentialHeightFogComponent` (`SetFogInscatteringColor`/`SetFogInscatteringLuminance`, 0.4 grey). Reads the four values first, writes map values, captures, unconditionally restores. If restore fails it disables the override (a dark world is forbidden). Re-finds the rig every 5 s.
- **Rate** (`:1127-1150`): `Config.DungeonViewRate` Hz (default 30, range 2–60); gated on `moved_enough` (25 cm) or idle refresh (3 s). The capture is issued only from the pose tick — `CaptureScene` from any other thread crashes the renderer (proven 3/3).
- **Engages** when `area_map_available == false` (dungeon) with `NoMapBackground=="dungeon"`, or everywhere if `DungeonViewEverywhere`. Brightness tint via `Image:SetColorAndOpacity` (dungeon 1.0 / open-world 0.55 × `DungeonCaptureTint`).

**Hard to reproduce natively?** No — a SceneCapture2D + render target is straightforward in C++. The lighting-rig borrow (mutating the level's actual light components per capture) is the fragile part; a native port could instead give the capture its own post-process/exposure and skip the borrow.

### AreaName (`Local/AreaName.lua`) — region + sub-area text
Source is the named box-volume actor `BP_LocationNameDisplay_C` (`/Game/Sparta/Core/Common/Interactions/Blueprints/`), NOT the on-screen notify (which is contaminated). Reads: `Box` (UBoxComponent, `GetScaledBoxExtent`), `LocationID` (FName), `LocationText` (FText). Inverted-field rule (`:144-151`): text present → region name (kind "outer"); id present, no text → dungeon/sub name (kind "sub"). Hooks on `BP_LocationNameDisplay_C`: `:ReceiveBeginPlay`, `:TriggerRegularNotify`, `:TryTriggerDungeonNotify`. Beacons `BP_LandingAreaBase_C:GetAreaName()` + `UniqueID` (`LandingArea_HUB` is the anchor). `Resolve(x,y,z)` (`:394-504`) is pure Lua over cached volumes/beacons, gated on a 150 cm positional delta. One-shot per-world seed enumerates the two classes once (`FindAllOf`). **Reads no player velocity/speed** — only a positional delta as a "moved enough to recompute" gate.

**Labels** (`Overlay/Labels.lua`) render the name + cardinal letters as `/Script/UMG.TextBlock` on the outer canvas. Fonts are game `UFont` composites (Trajan/Crimson variants); a `UFontFace` draws nothing. Text must be an FText via `UKismetTextLibrary:Conv_StringToText` (this UE4SS build has no `FText` global; `SetText` with a raw string is a hard crash). Color shape (`FSlateColor` vs `FLinearColor`) is settled on a throwaway probe widget. This whole file is the fiddliest to port and is optional.

---

## 6. MOVEMENT / COMBAT STATE

**Does NOT read player locomotion.** There is no reading of velocity, speed, gait, or a movement component anywhere for the player. The player pose is `K2_GetActorLocation` + `K2_GetActorRotation` (position + yaw), and AreaName/DungeonView use a positional delta only as a "moved enough" gate.

**Combat state** (`Runtime/CombatState.lua`) — reads the game's music state machine, not the pawn:
- `SpartaGameInstance.MusicManager` → `BP_MusicManager_C`; the live state is `manager.m_CurrentState` (a `SpartaMusicState`).
- Resolved once via `GameplayStatics:GetGameInstance(controller)` → `instance.MusicManager` (`resolve_manager`, `:186-222`).
- Two sources: Blueprint hooks on `BP_MusicState_Combat_C:Enter`/`:Exit` (`/Game/Sparta/Core/Audio/Music/States/`), and a 0.5 s poll of `m_CurrentState` class name (catches `BP_MusicState_BossCombat_C`, which has no BP Enter/Exit). `classify(name)` (`:101-110`): contains "combat" and not "precombat"/"postcombat" → in combat; "bosscombat" → boss.
- The look: writes render transform + opacity on the renderer's **root canvas** (whole minimap fades/shrinks about its center). Hide → fade to 0 then collapse via `WantsHide()`. Restore delay keeps the look after combat ends.

**Enemy dot movement** (the closest thing to reading AI locomotion, `Local/Discovery.lua`): the enemy marker follows the AI by hooking movement-adjacent functions on the AI classes and re-reading `actor:K2_GetActorLocation()` each time (throttled to 0.2 s). Refresh hooks (`refresh=true`, `:254-351`): `BP_AICharacter_C:OnEncounterStarted`, `:SpartaApplyHit`, `:UpdateFightTarget`, `:PushPlayer`, `:PlayFootstepVFX`; `SpartaAICharacter:OnPreAggro`/`:OnAggro`; `SpartaCharacter:OnFoleyLinearVelocityChangeExceedThreshold`/`:OnFootDown`. Death: `BndEvt__..._SpartaHealth_DeathEvent__DelegateSignature`, `:DespawnEnemy`; corpse check `:CheckCorpseRemoval` reads `HealthComponent:IsDeadOrDying()` / `:GetDeathState()`. Flying enemies (bats) have no per-movement hook (no feet); the fallback is a 500 ms `enemy_poll_tick` that re-reads location for tracked, near, stale enemies within budget. **How this game exposes AI movement: there is no clean per-frame movement callback** — the mod triggers off gameplay events (hit/aggro/footstep/foley-acceleration) and polls. `OnFoleyLinearVelocityChangeExceedThreshold` fires on *acceleration*, not steady movement. For a native port, `USceneComponent::GetComponentVelocity()` or the movement component on `SpartaCharacter` would give real speed directly (the Lua mod avoided it to not retain wrappers, not because it's unavailable).

---

## 7. CONFIG / SETTINGS

### Schema (`Config/Schema.lua`) — every user-facing option
Tabs: GENERAL, MAP, ICONS, LOCAL, COMBAT, INPUT, MOD, CAPTURE (CAPTURE + several rows are Advanced-gated). Each row has `key`, `values`, `format`, and an `apply` action string (or `rebuild=true`). Defaults are in `Config/Runtime.lua`.

**GENERAL:** Enabled(bool,true); Size(px,{160..500},300,apply=size); ZoomMeters(m,{25..50000},1000,apply=zoom); Opacity({0.20..1.00},0.90,apply=opacity); ShowArrow(bool,true,apply=arrow); OffsetX/OffsetY(px,rebuild); MapShape(square|circle,circle,rebuild); MapOrientation(north|camera|player,camera,apply=orientation); MapFrame(off|line|vector|ornate,vector,apply=map-frame); MapFrameColor(bronze|white|red|blue|green,bronze).

**MAP:** NoMapBackground(dungeon|transparent|cloud,dungeon,apply=area-presentation); DungeonViewBrightness(0.40..1.50,1.00); DungeonViewOpenWorldBrightness(0.20..1.00,0.40); DungeonViewEverywhere(bool,false); DungeonViewRate(2..60 Hz,30,Adv); DungeonViewCeilingAuto(Auto/Fixed,true,Adv); MapLabels(0..3=Off/Area/Cardinals/Both,3); MapLabelSize(0=Auto..28,16); MapLabelFont(8 choices,trajanbold); MapLabelColor(8 colors,white); MapLabelOutline(0..3,0,Adv); MapLabelShadow(bool,false,Adv); MapLabelCardinals(North-only/NESW,1); FootstepTrail(0..48 marks,16); FootstepTrailSize(25..200%,100); LocalIconPlates(bool,true); LocalHeightMarkers(bool,true); LocalHeightThresholdMeters(1..20 m,3); LocalHeightMarkerSize(40..150%,60); LocalHeightFade(bool,true); LocalHeightFadeStart(75%),Min(35%),Range(15 m) (Adv).

**ICONS:** ShowPOIs(bool,true); EdgeVisibility(0..100%,75,Adv); POISettingsCategory(selector); per-selected-category: Visible, Discovered-only/hidden, IconSize, EdgeIndicators, EdgeMaxDistance(Adv), EdgeMinScale(Adv). User pins: TrackerIconSize(px,48), ShowTrackerEdgeIndicators(true), TrackerEdgeMaxDistance(150,Adv), TrackerEdgeMinScale(0.40,Adv).

**LOCAL:** ShowLocalInteractables(bool,true); LocalInteractableIconSize(px,20); LocalInteractableBudget(8..64,64,Adv); LocalSettingsCategory(selector over 20 categories); per-category: show, size(%), icon (from Classifier shortlist), icon color (white-art only).

**COMBAT:** CombatHide(bool,false); CombatOpacity(0.10..1.00,0.90); CombatSize(0.50..1.00,0.90); CombatTransition(0..1 s,0.5,Adv); CombatRestoreDelay(0..8 s,2).

**INPUT:** MenuKeybind (default `LeftControl+Period`); ControllerMenuBind (`Gamepad_LeftThumbstick+Gamepad_FaceButton_Bottom`); ModifierSidesEquivalent(true,Adv); ControllerSettings (opens shared host screen).

**MOD:** PauseGameWhileSettingsOpen(true); DebugLog(false,Adv); LogPerformance(false,Adv); AdvancedSettings(false); POIMaxIcons(192,rebuild,Adv); MaxResidentTiles(8,rebuild,Adv); LODBias(2,rebuild,Adv); PanUpdatesPerSecond(30,Adv); ArrowUpdatesPerSecond(30,Adv).

**CAPTURE (all Adv):** map lighting (rig on/off, sun/sky/sky-factor/fog), capture source/format/tint/unlit/resolution/LOD, exposure (own-post-process/method/bias/min/max/speed/physical/bloom/vignette/persist), camera (height/ceiling-cut/auto-cut-ceiling).

INI-only (no row): LocalDiscoveryTrace, EnemyRefreshHooks, MapLabelInk, MapLabelInset, CircleOpacityCurve, LocalHeightThresholdMeters fractions, EnemyDecaySeconds. Every tab has a "Reset this tab" row.

### Persistence (`Config/Runtime.lua`)
Plain-text `MinimapConfig.ini` next to the mod. `load()` (`:501-548`) parses `key = value` lines, coercing to the default's type; migrations for older keys (RotateWithCamera→MapOrientation, TransparentNoMapBackground→NoMapBackground, AdvancedSettings default-visible for pre-existing INIs). `normalize()` (`:327-496`) clamps/whitelists every value against `Defaults`. `save()` (`:550-693`) writes every key explicitly. Saved on every setting change (from ModUI's `apply_setting`).

### In-game UI (`UI/ModUI.lua`)
**The settings window is NOT drawn by this mod.** It is entirely delegated to the shared `MortalShell2ModUI` host. This file is an adapter that:
- Loads the host by `loadfile` (`mods_dir\MortalShell2ModUI\Scripts\ModUI.lua` and `\Scripts\Host\ConsumerClient.lua`, `:218-228`) and calls `client_factory.BindSettingsProvider(modui, ModRef, {descriptor})` (`:233`). `ModRef` (the UE4SS mod handle) is an identity token passed to the host, not the IPC channel.
- Feeds a **snapshot** (`settings_snapshot`, `:745-812`) — serialized labels/values/tabs/selection — the host renders. Reads back named actions (`client.ReadActions()`) and shell/menu state (`client.HostMetadata()` / `ReadVisibilitySnapshot()`).
- Session/lease/generation handshake with the host (`Publish`, `Activate`, `Lease`, `Session`, `NativeInput`, `ControllerInput`), ownership arbitration (auto-closes if another mod's menu takes over), native-menu gate, pause-while-open (`PauseNativeGameplay`), and live preview (`apply_setting` → `Renderer.ApplyLiveSetting`).
- Binding-capture state machine (chord/repeat recording) for the two hotkeys.

**For a C++ port:** you must rebuild the schema model, snapshot serialization, action routing, and the host handshake — OR draw your own settings UI. The host contract is implicit (defined only by how this file calls `client.*` / `modui.Settings.*` / metadata keys); reproducing it means having the `MortalShell2ModUI` source. A native mod would likely draw its own settings window (ImGui or its own UMG) and skip the host entirely. Hotkeys default `Ctrl+.` (keyboard) and `L3+Cross/A` (controller), registered via `RegisterKeyBind`.

---

## 8. PERFORMANCE

### Work budget (`Runtime/WorkBudget.lua`)
All non-pose work goes through `WorkBudget.Schedule(delay_ms, cb, label)` (wraps `Lifecycle.Schedule` → `ExecuteInGameThreadWithDelay`), which defers again if the native-menu gate is active, and pcall-guards. `RunSlice(max_items, max_cpu_ms, step)` (`:36-52`) processes items until an item count or a wall-clock CPU budget (ms) is hit — this is how the objective audit and registry queue avoid hitches (e.g. 4 items / 2.0 ms for the audit, 4 / 0.20 ms for the queue).

### Per-frame throttling (`Map/NativeWidget.lua` + `Lifecycle.lua`)
- Pan and arrow have independent Hz (default 30 each, 5–60) via fractional-phase accumulation.
- Pan writes epsilon-gated to sub-pixel; rotation to 0.25°; arrow to 0.5°.
- Overlay pools run only when geometry changed (a pan/rotation write) or a refresh was requested (`overlay_due`), not every frame.
- Each pool does a squared-distance broad-phase reject before projecting, and epsilon-gates every slot write.
- Icon construction is sliced (batch 4, 30–50 ms apart) off the pose path.
- Rebuild-flagged settings are coalesced over a 0.30 s debounce (a held arrow key = one rebuild, not 30).
- Build retry uses an exponential backoff ladder (1/2/4/8/16 s) so an unbuildable world doesn't sweep every second.

### Profiler (`Core/Perf.lua`)
Off by default and free when off (`Begin()` returns nil, `End(nil)` is a no-op). When on, each section accumulates count/total/max/spike; a `[PERF]` summary every 10 s. Sections named `tick.pose`, `hook.<label>`, `schedule.<label>`, `native.find`/`native.load`, `io.config.save`, etc. `os.clock()` on the target Windows build ticks in whole ms, so counts/rates are reliable but sub-ms `max` is not. The single most expensive recurring cost is the dungeon capture (30 Hz); README says drop it first.

---

## Things hardest to reproduce natively (flagged)
1. **The ModUI host contract** — an implicit cross-mod protocol (session/lease/generation/native-input epochs, metadata ownership keys) with no schema file. Either keep talking to `MortalShell2ModUI` (needs its source) or draw your own settings UI. A native mod almost certainly draws its own.
2. **Text/FText/fonts (`Labels.lua`)** — FText construction, font-asset resolution (UFont vs UFontFace), and the FSlateColor-vs-FLinearColor ambiguity are UE4SS-reflection landmines; in native C++ these are normal API calls, so this gets *easier*, not harder.
3. **Circle opacity compensation** — only needed because it drives the game's retainer/effect material; a native mod that owns its own mask material can skip it.
4. **Dungeon lighting-rig borrow** — mutating the level's real light components per capture is fragile; prefer giving the capture its own exposure/post-process.
5. **The exact game symbol set** — the ~90 local hooks, the `BPC_WorldMapHandler`/`Objective` surface, `SpartaMapWidget` fields, the `BP_WorldMapSettings_2D` calibration constants, and the `WBP_WMI_*` / `T_UI_Icon_Map_*` catalog. These are the real payload of this port and are enumerated in §2–6 above; verify each still exists in the current game build before relying on it.
6. **Rubble walls announce their break through nothing** — a native port still needs a periodic component-existence recheck (or a physics/destruction delegate if one can be found) to retire them.
