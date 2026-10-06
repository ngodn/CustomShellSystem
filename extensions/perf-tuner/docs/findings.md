# Findings, measured in the game

Machine: RTX 3060 12 GB, i5-12400F, GE-Proton 11-7, NVIDIA 610.57, DLSS Quality (66.6 %
internal, 1705x960 of 2560x1440), all quality groups High, hardware RT on in the player's
config. Frame times from the loader's frame ring (`frame.brief` / `frame.stats`).

## 2026-10-06, the tavern area (Fallgrim tavern, 12 NPCs, 33 lights)

- `stat unit` (read from an engine screenshot): Frame 34.1 ms, Game 8.6, Draw 5.8, RHI
  thread 17.6, GPU 30.7 ms, 1480 draws, 538 K prims, 6.36 GB memory. The GPU is the limit;
  the RHI thread (VKD3D translating DX12 for 1480 draws plus the driver) is the next.
- Probe: 35.0 ms at 66.6 % became 39.4 ms at 100 %. The frame hardly scales with pixels:
  its weight is in work that is the same at any resolution (22 shadow-casting lights on a
  512-page VSM pool, Lumen scene lighting, ray tracing). The probe's first wording called
  this "CPU-bound"; it now says "does not scale with pixels".
- Hardware RT on/off/on/off (`r.Lumen.HardwareRayTracing`), 10 s windows after 5 s settle:
  on 28.7 / 28.5 ms, off 31.4 / 30.7 ms. **Hardware tracing is faster here** (the game sets
  `r.Lumen.TraceMeshSDFs.Allow=0`, so software Lumen has only the global distance field).
  The HWRT finding text no longer calls it the biggest GPU cost.
- The default tweak set (texture pool 2758 MB, Nanite pool 766 MB, no forced GC on stream
  out, 120 s purge), off/on/off/on, 10 s windows: off 33.1 / 34.2 ms, on 33.9 / 35.6 ms;
  p99 94.1 vs 93.6 ms. Verdict: unclear, no gain in this scene. The hitches (12 to 14 per
  10 s, up to 97 ms) are not texture streaming; they clustered around render-state changes
  and the player's own movement, and dropped to about one per 10 s standing still.
- World audit: 59 actors of the audited families, 20 ticking (all NPC and player
  blueprints), 23 skeletal meshes of which only the player's two and two props animate
  while unseen: the game already uses the engine default on NPCs. 33 lights, 22 casting
  shadows, none movable. 11 particle systems, 3 active. The "animate only what is seen" fix
  changed 2 meshes and put them back.
- CSSX's own share of the frame: 0.1 ms mean, 1.3 ms p99 (the dev channel serving these
  probes), never a quarter of any hitch.

What would move this scene is a picture choice, not a tweak: Shadows Ultra (2048-page
pool, fewer page overflows with 22 shadow lights) is quality up and may be faster or
slower; fewer shadow-casting lights is quality down. Reflex off, Nanite shadow raster on
async, and Nanite without mesh shaders are still to be measured.

## 2026-10-06, the optional switches (same session, a lighter area, 21 to 28 ms frames)

- Reflex off: off 28.1 / 27.2 ms, on 27.5 / 27.1 ms. No change. Reflex stays on.
- Nanite shadows on the compute queue (`r.Nanite.AsyncRasterization.ShadowDepths 1`): a first
  run read +9.3 % but the rounds disagreed (the scene changed under it); the rerun standing
  still: off 21.6 / 21.6, on 21.4 / 21.4 ms. No change.
- Nanite without mesh shaders (`r.Nanite.MeshShaderRasterization 0`): off 21.5 / 22.8, on
  21.5 / 21.4 ms, **+3.4 % frame time, consistent**. The VKD3D mesh-shader path is the slower
  one on this card; the tweak stays off by default (Windows and other drivers differ) with
  the number in its text.
