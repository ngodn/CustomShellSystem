# CSSX Teleport: research and design

Reverse-engineered from the game's CXXHeaderDump (UE4SS, MS2 5.6.1) and online
research. The design leans on the game's own teleport systems rather than
rebuilding them.

## How the game already teleports

Two fast-travel systems ship in the game:
- **Marrow Keep launchpad**: fires the Harbinger as a meteor toward a beacon. A
  cutscene plays while the target area streams in behind it, so there is no
  loading screen. This is the animation the mod reuses.
- **Mether's Breath**: later, direct beacon-to-beacon with the normal
  behind-the-character, zoomed-out animation.

## The reflected classes that matter

- **`GA_TeleportPlayerWithStreamingSupport`** (Gameplay Ability): the whole
  teleport-with-streaming sequence. It spawns a `BP_BlockingStreamingSource` (loads
  the target), applies Invulnerability and FallDamageImmunity, fades, teleports,
  waits on `OnActorStreamingCompleted`, then `OnTeleportComplete` and unblocks the
  player. Driven by the global event `GlobalEvent_TeleportPlayerWithStreaming`.
  Reusing this is what gives the meteor animation and the no-loading-screen trick
  for free. Do not rebuild it.
- **`BPC_TeleportManager`** (component): holds `AllHandlers`
  (`TArray<ABP_SpartaTeleportHandler_C*>`), the full list of teleport points; plus
  `CachedCharacter`, `CameraState` (`UCSCameraState`), `bIsInDungeon`, and the
  dungeon respawn data. This is where the mod reads the point list and reaches the
  camera state.
- **`ABP_SpartaTeleportHandler_C`** and the STH variants
  (`BP_STH_Entrance_Dungeon`/`Exit_Gate`/…, `BPO_STH_Beacon`): one per point. They
  carry the transform, so the "teleport in front of it" position comes from the
  handler's entrance transform.
- **`ISpartaTeleportLocationInterface`**: `Teleport()`, `GetLocationName()`,
  `GetCategory()`. Category is how the mod tells beacon from gate from dungeon.
- **`BP_LandingArea_*`**: where the meteor lands. `EligibleForFastTravel()` /
  `CanFastTravel()` gate whether a point is reachable (the "allow locked" setting
  overrides this).

## The map UI to hook

The world map draws each point with `WBP_MapActor_LandingAreaSelector` (over
`WBP_MapActorBase`), which owns:
- `WBP_IL_LandingAreaSelector` (an input listener), `LinkedLandingAreas`,
  `LinkedGate`, and `MyTooltip` (`WBP_MapActorTooltip_LandingArea`).
- `UpdatePrompts()`: builds the on-hover prompt row (the "T Teleport" / "Mark
  Complete" line). This is where the mod adds its Teleport prompt.
- `ShowMapTooltip()` / `HideMapTooltip()` and `ShowHighlight()` on hover.

So the mod adds a prompt through `UpdatePrompts`/the tooltip, reads the hovered
handler off the selector, and detects the key through the existing input listener.

## Design

A native CSSX extension (`eins0fx.teleport`), built to the CSSX extension
performance standard (cache reflected handles, no per-frame bridge polling,
event-driven, all work off the hot path).

1. **Enumerate** teleport points from `BPC_TeleportManager.AllHandlers`, tagged by
   category (beacon/gate/dungeon). Filter out locked/unactivated points unless the
   "allow locked" setting is on.
2. **Map prompt**: on hover of a selector, add a "Teleport" prompt (keyboard `T`,
   controller `RS/R3`) to its tooltip. Detect the press through the selector's
   input listener, not a per-frame poll.
3. **Confirm**: if the confirmation setting is on, show a confirmation dialog
   first.
4. **Teleport in front**: compute a position in front of the handler's entrance
   transform, then fire `GlobalEvent_TeleportPlayerWithStreaming` with it. The
   game's `GA_TeleportPlayerWithStreamingSupport` carries out the meteor +
   streaming.
5. **Custom camera**: before firing, drive `UCSCameraState` to a front-facing
   framing (Harbinger facing the screen), then hand off to the meteor animation so
   the transition is smooth. This is the one piece with no ready-made game path and
   needs the most in-game iteration.

## CSSX menu settings

- `enable`: yes / no (default yes)
- `confirmation`: show / skip (default show)
- `allow_locked`: yes / no (default yes), teleport to unaccessed/unactivated/locked
  points

## Open questions for in-game iteration

- Exact "in front of" offset per category (beacon vs gate vs dungeon entrance).
- Whether `GlobalEvent_TeleportPlayerWithStreaming` accepts an arbitrary target or
  only a registered handler; if the latter, whether we target the nearest handler
  and nudge the landing.
- The camera handoff timing into the meteor animation (front-facing to launch).
- Dungeon points may need `GlobalEvent_DungeonTeleporter_Interact` /
  `LeaveDungeonByTeleport` instead of the plain streaming teleport.
