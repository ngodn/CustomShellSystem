# Status — CSSX Compass & Minimap

Living tracker. Read `DESIGN.md` for the plan/API and
`../work/cartographer-reference-analysis.md` for minimap symbols.

## Where we are
- Branch: CSS `main` (VERSION 0.4.1). Beta work parked at `git stash@{0}` on
  `v1.0.0-beta`.
- Decision: extend CSSX to ABI v2 (per-frame `render` + native HUD API), then
  build the extension on it. Minimap = full parity, phased (user choice).
- Research complete: compass math, CSSX framework, minimap architecture — all
  captured in DESIGN.md / work/.

## Phase 0 — CSSX ABI v2 (DONE — builds clean, cheat-menu abi-1 still builds)
Implemented + cross-compiled (build/cssx-native, exit 0, no warnings):
- api.h/hud.h v2; extension_runtime.cpp abi negotiation + render dispatch;
  extension_hud.inl (HudService: overlay in WBP_Player_HUD, layer store, texture
  cache, epsilon-gated writes, teardown-by-identity + generation); engine.hpp
  HudService decl; core.cpp holds HudService, passes hud vtable to extensions,
  computes CssxFrame + calls render() each frame, release() on stop;
  extension_client.hpp render().
- NOT yet staged into the game (install needs user OK + game closed).
- layer_object() returns 0 for now (bridge $object handles come with the minimap).

## Phase 0 — original checklist
- [x] `native/include/cssx/api.h`: bumped `CSSX_ABI` to 2; added optional
      `render` to `CssxExtension`; added `const CssxHudApi* hud` to `CssxHost`;
      added `render` to `CssxRuntime`. New `native/include/cssx/hud.h`
      (CssxHudApi, CssxFrame, CssxLayer). ABI-1 layout preserved through its
      members so old extensions still validate; host stamps each extension's
      `CssxHost.abi` with the abi it declared (so cheat-menu's `host->abi==1`
      check still passes). Min-size check must use `offsetof(CssxExtension,render)`
      for abi 1, `sizeof(CssxExtension)` for abi 2.
- [ ] `client.hpp`: accept abi>=2 host; expose hud table + a thin C++ HUD helper.
- [ ] Core HUD service: root overlay in WBP_Player_HUD, layer store, texture
      cache, epsilon-gated writes, world-teardown rebuild + generation.
- [ ] Runtime (`extension_runtime.cpp`): accept abi-2 extensions; per-frame
      `render` dispatch fed a CssxFrame; keep 10 Hz `tick` as-is.
- [ ] Loader/core wire-up: call the render dispatch from the engine-tick path;
      compute CssxFrame (camera yaw, player loc/yaw/velocity, viewport, flags).
- [ ] Prove: a throwaway test extension that draws one moving image layer.
- [ ] Backward-compat check: cheat-menu (abi 1) still loads and works.

## Phase 1a — compass (BUILT, not yet installed/tested in-game)
Extension `eins0fx.compass-minimap` in extensions/compass-n-mmap/ (extension.json,
menu.json two tabs Compass+Minimap, src/main.cpp, CMakeLists.txt, assets/ = the 6
reference PNGs). Compiles clean to build/compass-minimap/compass_minimap.dll.
- Compass: tape scroll from camera_yaw (continuous wrap), frame + center plate,
  built on the ABI-2 HUD; rebuilds on world_generation change.
- idle/jog/sprint shrink+fade from CssxFrame planar speed (thresholds 300/700 cm/s,
  hysteresis 40, eased ~8/s). All tunable in the Compass tab.
- Minimap tab: placeholder toggle + label (renderer is Phase 1b+).
- layer_object still 0 → SpartaMapWidget setup pending (Phase 1b).
NEXT: install (dev core + cssx_core + this extension) and test in-game; then markers
(Lost Gloom / pings / dungeons), then the minimap.

## Phase 1b — core minimap (BUILT + validated, NOT yet installed)
- Framework: `layer_object` now bridges a HUD widget to a $object handle
  (ExtensionBridge::track, wired via HudService minter_ in core.cpp). css_core
  rebuilt clean.
- Extension: src/minimap.hpp (self-contained; compass file kept byte-identical).
  Minimap = clipped CanvasPanel group + dark backdrop + native SpartaMapWidget,
  player-centered via SetPanCenterNormalized, orientation north/camera/player as a
  render-transform angle (√2 oversized surface), zoom via SetZoom, and the shared
  idle/jog/sprint shrink+fade with its OWN sliders. menu.json Minimap tab expanded
  (enabled/size/zoom/orientation/opacity/jog+sprint size+opacity). Builds clean,
  cssx_validate passes.
- SpartaMapWidget setup (SetTileSet/TileMaterial/SetGlobalAlphaMask/SetRegionBounds/
  SetRevealAmount/SetZoom + MaxResidentTiles/LODBias/bExternalZoomAndPan) is
  reflection via the host request API, from the reference symbols — HIGH RISK,
  unverified. Each step guarded + logged ("[compass-minimap] ...") so a wrong
  symbol degrades to backdrop-only and names itself in the extension log.
- HELD: not installed. Waiting on the compass in-game result (validates the HUD
  framework) before staging compass-fix + minimap together and iterating the tile
  setup from the logs.
- NOT yet: player arrow (needs a bundled arrow PNG or a game-texture brush path),
  markers, dungeon capture, area name, circle mode.

## Phase 2/3/4 (NOT STARTED)
Local discovery / dungeon capture / trails+combat+full settings.

## Decisions & gotchas
- Everything is game-thread + synchronous; no exceptions across the ABI (wrap
  every exported thunk). Never retain UObjects across a `world_generation` bump.
- idle/jog/sprint thresholds (cm/s): idle ≲25, jog ~300–700, sprint ≳700
  (from `native/src/walk_override.inl:29`). Smooth + hysteresis.
- Verify each game symbol still exists in the current build before relying on it
  (SpartaMapWidget fields, WBP_WMI_* classes, T_UI_Icon_Map_* textures).
- Install (core DLL / pak) needs user OK + game closed. Build/test freely.

## Installed 2026-09-18 (game closed; loads on launch)
- core.json -> css_core-dev-b69110613387bb21-... (CSSX-v2 dev core, from main/0.4.1)
- cssx.json -> cssx_core-dev-0713a0004b03a858 (abi 2)
- loader: staged the matching build main.dll (998a92f...) + rewrote
  loader-contract.json. The 0.4.1 RELEASE loader (fb033ff) + old contract are
  backed up at backups/rollback-css041-20260917T160412Z/core/
  (main.dll.release-041, loader-contract.json.prev) to restore pure 0.4.1.
- extension installed at <MOD>/extensions/eins0fx.compass-minimap/ (dll+manifest+
  menu+assets); cssx_validate passed.
- NOT yet verified rendering in-game (first live run of the HUD framework).

## Log
- 2026-09-18: branch -> main/0.4.1; research done; DESIGN.md + STATUS.md written;
  Phase 0 (CSSX ABI v2 HUD) built clean; Compass extension (Phase 1a) built;
  loader-contract reconciled; cores + extension staged; validated; handed to user
  to launch and test.
