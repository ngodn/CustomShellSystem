# CSSX Compass & Minimap — design

One native CSSX extension that draws an Elden-Ring-style tape **compass** and a
**minimap** as per-frame HUD overlays, with settings under two tabs (Compass,
Minimap). Combines two Lua UE4SS mods, reimplemented in C++:

- Compass: nexusmods MS2 #193 (KalilaViolette / "MS2LostGloomCompass").
- Minimap: nexusmods MS2 #497 (omuruwi / "Cartographer").

Reference source and full reverse-engineering notes live under
`../reference/` and `../work/cartographer-reference-analysis.md`. Read those for
exact symbol tables; this doc is the plan and the API contract.

Target: CSS **0.4.1** (`main` branch). The installed core is 0.4.1, so all core
work happens here, not on `v1.0.0-beta`.

## The blocker this design solves

A CSSX extension (ABI 1) exports only `create/tick/model/event/stop/destroy`. Its
UI is declarative `menu.json` sections the core renders; `tick` is coalesced to
~10 Hz; there is **no per-frame render and no way to build HUD widgets or paint
the screen**. A compass/minimap is exactly a per-frame HUD overlay. So ABI 1
cannot host this. Decision (user): extend CSSX so an extension can own a HUD.

The core already has every primitive needed, so this is an exposure job, not new
tech:
- per-frame game-thread tick: `loader.cpp:85` `RegisterEngineTickPostCallback`.
- UMG build: `engine.cpp:669` `StaticConstructObject`, `:734` `AddChildToCanvas`,
  `:789` `SetBrushFromTexture`; camera read `:806`; texture import
  `extension_view.inl:64` `ImportFileAsTexture2D`.

## Architecture: CSSX ABI v2 (retained HUD, native hot path)

Bump `CSSX_ABI` to `2`. Backward compatible: the host accepts abi-1 and abi-2
extensions; abi-1 extensions behave exactly as today. The extension DLL is native
and in-process with the core, so the hot path is **direct C function pointers, no
JSON per frame**.

### 1. `CssxExtension` grows one optional callback

```c
int (*render)(void* instance, const CssxFrame* frame);  // per frame, game thread
```
Null-checked like `tick`. Only abi-2 extensions set it. `render` is where the
extension computes cheap per-frame updates and pushes them through the HUD API.
`tick` stays as the ~10 Hz logic/settings path; `render` is per frame.

### 2. `CssxHost` grows a pointer to a native HUD services table

```c
const struct CssxHudApi* hud;   // null when host abi < 2
```
`CssxHudApi` is a versioned vtable (abi,size + function pointers). The core owns
all widgets; the extension only holds opaque `CssxLayer` (uint64) handles and
never touches a UObject directly.

```c
typedef uint64_t CssxLayer;               // 0 = invalid/root
// construction (valid any time; realized against the live HUD by the core)
CssxLayer (*image)(void*ctx, CssxLayer parent);
CssxLayer (*text) (void*ctx, CssxLayer parent);
CssxLayer (*widget)(void*ctx, CssxLayer parent, const char* class_path, size_t); // e.g. SpartaMapWidget
void      (*destroy)(void*ctx, CssxLayer);
uint64_t  (*texture)(void*ctx, const char* path, size_t);   // cached import; 0 = fail
uint64_t  (*layer_object)(void*ctx, CssxLayer);   // $object id of the widget, for request() call/set
// retained appearance — write only when your value changed; core epsilon-gates Slate writes
void (*set_brush)(void*ctx, CssxLayer, uint64_t texture);
void (*set_rect)(void*ctx, CssxLayer, float x,float y,float w,float h,float ax,float ay,int32_t z);
void (*set_translation)(void*ctx, CssxLayer, float x,float y);
void (*set_scale)(void*ctx, CssxLayer, float sx,float sy);
void (*set_angle)(void*ctx, CssxLayer, float deg);
void (*set_pivot)(void*ctx, CssxLayer, float px,float py);
void (*set_opacity)(void*ctx, CssxLayer, float a);
void (*set_color)(void*ctx, CssxLayer, float r,float g,float b,float a);
void (*set_visible)(void*ctx, CssxLayer, int32_t);
void (*set_text)(void*ctx, CssxLayer, const char* utf8, size_t);
void (*set_font)(void*ctx, CssxLayer, float size);
void (*set_clip)(void*ctx, CssxLayer, int32_t);
```

`layer_object` is the escape hatch: for exotic setup (SpartaMapWidget
`SetTileSet`/`SetZoom`/`SetPanCenterNormalized`, SceneCapture) the extension gets
the widget's `$object` handle and drives it with the existing `call`/`set`
request ops — off the hot path (once, at build), so JSON there is fine.

### 3. `CssxFrame` — everything the extension needs per frame, precomputed by the core

```c
typedef struct CssxFrame {
  uint32_t abi, size;
  double seconds;                        // delta
  uint32_t world_generation;             // bumped on every world teardown/new HUD
  int32_t  world_ready;                  // playable HUD + pawn present this frame
  int32_t  in_menu;                      // native menu/map open -> suppress HUD
  double camera_yaw;                     // deg, normalized (-180,180]
  double player_x, player_y, player_z;   // cm
  double player_yaw;                     // deg
  double velocity_x, velocity_y, velocity_z; // cm/s (planar speed = hypot(x,y))
  double viewport_w, viewport_h;
  uint64_t pawn, controller;             // $object ids for request() use
} CssxFrame;
```

The core computes camera yaw (`engine.cpp:806`), player loc/yaw + velocity (pawn
`CharacterMovement.Velocity`, same read as `walk_override.inl:182`), viewport, and
the menu/world flags. `world_generation` is the teardown-safety signal: when it
changes the extension forgets its handles and rebuilds — the core has already
dropped the old world's widgets, so the extension never validates a freed UObject
(the invariant the Lua mods enforce by hand).

### 4. Core-side ownership (the hard parts live here, once, for all HUD extensions)
- HUD root overlay injected into `WBP_Player_HUD_C`'s canvas; rebuilt on world
  change; boss-health/menu suppression via `in_menu`/a visibility gate.
- Texture import + cache by path.
- Epsilon-gated Slate writes (translation 1e-3, angle 0.25°, opacity, visibility
  only-on-change) so pushing every frame is cheap.
- Per-frame dispatch from the loader tick into the runtime into each abi-2
  extension's `render`.

### 5. Settings = CSSX menu sections (unchanged mechanism)
Two `menu.json` sections, `compass` and `minimap`, using existing control types
(toggle/slider/choice/number). `model` feeds live values; `event` applies +
persists via `state.save`. `render` reads the extension's own settings struct
(updated in `event`). No new UI tech for settings.

## Compass spec (nexus #193, rebuilt + improved)

Tape geometry & math verbatim from `../reference/.../main.lua` (see compass
report). Constants: HOST 1000x190; TAPE x120 y9 w760 h102; TAPE_FULL_W=5700;
HALF_DEGREES=72; px/deg = 760/144 = 5.2778; TAPE_CYCLE_W=1900.
- Heading: `heading = normalize360(270 + cameraYaw)`;
  `base_tape_x = -((360+heading-72)/1080) * 5700`; wrap by whole 1900 steps to
  keep it continuous across North.
- Marker to tape: `relYaw = normalize(atan2(dy,dx)deg - cameraYaw)`;
  `x = tapeCenter + relYaw*(760/144)`. Aux markers (pings, dungeons) use pawn
  origin and hide beyond ±72°; Lost Gloom uses camera origin and edge-clamps.
- Distance m = `hypot(dx,dy)/100`.
- Targets: Lost Gloom (`BP_DeathSpoils_C`), map pings/trackers, nearby dungeons
  (`BP_STH_Entrance_Dungeon/Gate`, `BPO_STH_Beacon`), radius 500 m, ≤4.
- Textures: the 6 PNGs in `../reference/.../assets/` (bundle in the extension).

### The improvement the user asked for (idle / jog / sprint)
Not in the reference mod (it reads no movement). Drive from `CssxFrame` planar
speed with the game's real thresholds (from `walk_override.inl:29`): idle ≲25,
jog ≈300–700, sprint ≳700 cm/s.
- Idle: full size (current Scale), full opacity.
- Jog: shrink to `JogScale` (default 0.85) and fade to `JogOpacity` (0.50).
- Sprint: shrink to `SprintScale` (default 0.75) and same/lower opacity.
- Smooth with a time constant (lerp toward target each frame, ~8/s) so it eases,
  not snaps; hysteresis on the thresholds to avoid flm near boundaries. Apply via
  `set_scale` (pivot 0.5,0.0, top-center like the reference) + `set_opacity` on
  the compass root layer. All three tunables exposed in the Compass tab.

## Minimap spec (nexus #497) — FULL PARITY, PHASED (user choice)

All math/symbols in `../work/cartographer-reference-analysis.md`. Reads no player
velocity; player = location + yaw; camera yaw from PlayerCameraManager.
Calibration: span 315552 cm, origin (38050.053612, -134391.872342),
MAP_LOGICAL_PIXELS 4096, BASE_NATIVE_ZOOM 0.25, span 3155.52 m.
- Pan: `pan = 0.5 - (world-origin)/315552` -> `SpartaMapWidget.SetPanCenterNormalized`.
- Native zoom: `0.25 * 3155.52 / zoom_m`.
- Overlay icon: `rotate(-(marker-player)*scale)`, player-centered; EdgePolicy
  clamp (50%=center-on-edge,100%=whole-in) + IconScale distance shrink.
- Orientation: north / camera (`90-cameraYaw`) / player (`90-playerYaw`), applied
  as render-transform angle; square surface oversized ×√2 when rotating.
- **Idle/jog/sprint shrink+fade, same as the compass (user request 2026-09-18):**
  the minimap group scales down and fades while jogging/sprinting and eases back
  when idle, on the same eased, hysteretic gait model. Factor the compass's
  gait→(scale,opacity) computation into a shared helper both HUD widgets call from
  a single `CssxFrame` speed read. The Minimap tab carries its OWN jog/sprint size
  + opacity sliders (independent of the compass's), so each can be tuned separately
  while sharing the gait detection.

**Phase plan** (each phase builds, installs, and is verified in-game before the
next):
- **Phase 0 — framework.** CSSX ABI v2 (headers, core HUD service, runtime
  dispatch, loader per-frame wire-up). Prove with a trivial test HUD layer.
- **Phase 1 — compass + core minimap.** Compass tab fully (tape, markers,
  idle/jog/sprint). Minimap: SpartaMapWidget tile map, square/circle,
  north/camera/player + arrow, area-name label (`BP_LocationNameDisplay_C`),
  world-map POI + objective + Lost-Gloom + user-tracker markers with edge
  indicators. Both settings tabs.
- **Phase 2 — local-interactable discovery.** Event-driven hooks (chests,
  pickups, traps, barrels, hidden walls, plants, enemies, NPCs), Classifier,
  LocalPool, hot-hook governor. Needs the core hooks API (`hooks.add`) or new
  render-side hook support.
- **Phase 3 — dungeon SceneCapture view.** Ortho top-down capture, ceiling-cut
  near-plane, render-target-to-brush. Capture must stay on the game thread; port
  the lighting-rig swap cautiously (known crash source — prefer capture-component
  show-flags/post-process).
- **Phase 4 — polish.** Footstep trail, combat dimming (music-state manager),
  full per-category settings, labels/frame cosmetics, Capture advanced tab.

## Build / install / verify

Extension builds like `examples/extensions/native-counter` (clang-cl toolchain,
C++23, include `native/include` + `native/vendor`). Core changes rebuild via
`tools/cssx_dev.py stage` (dev) or the release path. Install (staging a core DLL
or a container) needs the user's OK and the game closed — see the
install-confirmation rule. Building and tests never need to ask.

id: `eins0fx.compass-minimap` (never change across releases — storage namespace).
Extension folder: `extensions/compass-n-mmap/` (source), packaged to
`CSSX_CompassMinimap_eins0fx`.
