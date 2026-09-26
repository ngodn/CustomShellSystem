# CSSX Teleport: build design (locked)

This is the implementation contract, grounded in the RE report (`../docs/RESEARCH.md`)
and live inspection of the running game (core 1.1.0, dev channel). Every signature
below was confirmed live unless marked "(dump only)".

## What the mod does

On the world map, hovering a point (beacon / gate / dungeon / well) shows a
"Teleport" prompt (keyboard `T`, controller `R3`). Pressing it (with an optional
confirmation) teleports the player to that point using the game's own
streaming teleport, so there is no loading screen. Three settings plus an
optional front-facing camera.

## Extension shape

`eins0fx.teleport`, native, ABI 3. Pure JSON-bridge + HUD extension, exactly like
the Cheat Menu: it links only the client SDK (`cssx/client.hpp`, `cssx/hud.h`,
`cssx/abi.h`) and nlohmann/json, never the UE4SS SDK. All engine access is through
the host bridge; all drawing through the HUD API. This keeps it small, portable and
testable, and means it never holds a raw engine pointer across a core swap.

Files: `extension.json`, `menu.json`, `src/{main,teleport,runtime,overlay}.cpp`,
`src/teleport.hpp`, packaging (`README.txt`, `LICENSE`, `THIRD_PARTY_NOTICES.txt`,
`CREDITS.txt`), `assets/` (panel + key-badge PNGs).

## Settings (menu.json + model/event + state.load/save)

- `enable` toggle, default on.
- `confirmation` choice `show` / `skip`, default `show`.
- `allow_locked` toggle, default on. When on, points you have not unlocked or
  activated can still be teleported to. When off, only fast-travel-eligible points
  show the prompt.
- `camera` choice `front` / `default`, default `front`. `front` swings the camera to
  face the character before the jump; `default` keeps the game camera. The camera
  step is always best-effort: any failure is caught and the teleport still runs.

## The teleport call (verified live)

`BPFL_WorldStreaming_C` CDO (via `class_default` / `find`
`/Script/...Default__BPFL_WorldStreaming_C`), function
`TeleportPlayerWithStreaming`:

    Location        FVector   (struct {X,Y,Z})
    Rotation        FRotator  (struct {Pitch,Yaw,Roll})
    ControlRotation FRotator
    ScreenTransitionClass  TSubclassOf<UWBP_LS_TransitionBase_C>  (null = default fade)
    TransitionZOrder int32
    FadeInDuration  double
    FadeOutDuration double
    OptionalZoneData USpartaZoneData* (null ok)
    __WorldContext  UObject*  (the world handle from `player`)

No return. The granted `GA_TeleportPlayerWithStreamingSupport` reacts to the
broadcast, spawns a blocking streaming source at the target, fades, moves, waits for
the stream, fades back. That is the "no loading screen" behaviour, for free.

## Destination transform

Map points are `ABP_LandingAreaBase_C` (an `ASpartaPlayerStart`). The hovered
selector gives it via `GetSelectedLandingArea(out)`. The destination comes from the
landing area's `TeleportHandlerObject` (`UBPO_STH_Beacon_C`), whose base
`UBPO_STH_C` exposes `GetTeleportLocation()`, `GetTeleportRotation()`,
`GetTeleportControlRotation()`. Those are the authored landing spot, i.e. already
"in front of" the point, so no manual offset math is needed. Fallbacks if a getter
is missing: landing area `GetDesiredPlayerStartLocation/Rotation` /
`GetStartTransform`, then the selector's own actor transform.

## Map detection + hover (render loop)

`render(frame)` runs once per rendered frame while `world_ready`.

1. If `!enable` or `!frame.in_menu`: hide the prompt overlay and return. This is the
   whole per-frame cost outside menus: one branch plus (rarely) an epsilon-gated
   `set_visible(false)`. Zero reflected calls on the gameplay path.
2. While `in_menu`, throttle the scan to ~20 Hz with a `frame.seconds` accumulator.
3. Scan (cached handles, re-fetched only when expired or on `world_generation`
   change):
   - controller (`player`), `Teleport Manager`, `World Map Handler`.
   - `World Map Handler.MapActorWidgets`. For each `...LandingAreaSelector...`
     component that `IsShown`, read its `MapActorWidget` (the WBP selector) and its
     `HasFocus`. The focused one is the hovered point. If none: not on the map (or
     nothing hovered) -> hide, done.
   - `GetSelectedLandingArea(out)` on the focused selector -> landing area.
   - Gating: `CanFastTravel()` / `EligibleForFastTravel()` / `IsUnlocked()`. If the
     point is locked and `allow_locked` is off, show nothing.
4. Show the "Teleport" prompt overlay for the hovered point (its name from
   `Area Name` / `GetLocationName`).
5. Read input `T` and `Gamepad_RightThumbstick` via `input.keys` on the controller;
   edge-detect a press. Confirmation is a small modal overlay when `confirmation ==
   show`; `T`/`R3` confirms, `Escape`/`Gamepad_FaceButton_Right` cancels.
6. On confirm: read the destination transform, close the map
   (`HandleGameMenu(0,true)` on the UI handler), optionally push the front camera,
   then fire `TeleportPlayerWithStreaming`.

Cost: outside menus, nothing. Inside a menu, a bounded burst of coarse bridge ops at
20 Hz, which is not a gameplay hot path. Matches the CSSX extension performance
standard (event/state-driven, cached handles, no per-frame reflected work during
play).

## Front-facing camera (best-effort, setting-gated)

`PlayerController.PlayerCameraManager` is an `ACSCameraManager`
(`APlayerCameraManager` subclass). Approach: `SetDesiredViewFromDirection(BlendTime,
false, dir)` where `dir` faces the character from the front (opposite the pawn's
forward), a short blend before the jump; the streaming fade then covers the hand-off,
and the camera returns to gameplay on arrival. Everything in this step is wrapped so
a failure never blocks the teleport. Exposed as the `camera` setting.

Note: the exact "meteor-flash" set-piece (the Marrow Keep launchpad) is a specific
authored transition. v1 uses the game's default streaming transition (clean, no
loading screen). Passing a specific `ScreenTransitionClass` to reproduce the meteor
overlay, and the precise camera blend feel, are the one thing that needs a focused
live pass with the game window in front (the game throttles its tick while
unfocused, so it cannot be tuned from a background session).

## Lifecycle / safety

- `stop()`: destroy every HUD layer, remove any pushed camera state, clear state.
  Returns true (nothing persists into the world that must be retried).
- All handles cached and revalidated; a teleport never fires without a live
  controller + world + a resolved destination transform.
- Every engine call is wrapped; a failed scan degrades to "no prompt", never a crash
  or a stuck state. Confirmation prevents an accidental jump.
- Nothing writes to disk on the frame path; settings save only on an explicit edit.
