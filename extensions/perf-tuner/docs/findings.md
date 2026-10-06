# Findings, measured in the game

Machine: RTX 3060 12 GB, i5-12400F, GE-Proton 11-7, NVIDIA 610.57, DLSS Quality (66.6 %
internal, 1705x960 of 2560x1440), all quality groups High, hardware RT on in the player's
config. Frame times from the loader's frame ring (`frame.brief` / `frame.stats`).

## 2026-10-06, the tavern area (Fallgrim tavern, 12 NPCs, 33 lights)

- `stat unit` (read from an engine screenshot): Frame 34.1 ms, Game 8.6, Draw 5.8, RHI
  thread 17.6, GPU 30.7 ms, 1480 draws, 538 K prims, 6.36 GB memory. The GPU is the limit;
  the RHI thread (VKD3D translating DX12 for 1480 draws plus the driver) is the next.
- Historical resolution probe: 35.0 ms became 39.4 ms after requesting 100% from 66.6%.
  That build did not verify the changed resolution. These numbers cannot establish
  pixel sensitivity or identify a bottleneck. The revised diagnostic verifies writes
  and ignores stored verdicts from that older implementation.
- Hardware RT on/off/on/off (`r.Lumen.HardwareRayTracing`), 10 s windows after 5 s settle:
  on 28.7 / 28.5 ms, off 31.4 / 30.7 ms. **Hardware tracing is faster here** (the game sets
  `r.Lumen.TraceMeshSDFs.Allow=0`, so software Lumen has only the global distance field).
  The HWRT finding text no longer calls it the biggest GPU cost.
- The default tweak set (texture pool 2758 MB, Nanite pool 766 MB, no forced GC on stream
  out, 120 s purge), off/on/off/on, 10 s windows: off 33.1 / 34.2 ms, on 33.9 / 35.6 ms;
  p99 94.1 vs 93.6 ms. Verdict: unclear, no gain in this scene. The hitches (12 to 14 per
  10 s, up to 97 ms) clustered around render-state changes
  and the player's own movement, and dropped to about one per 10 s standing still.
- World audit: 59 actors of the audited families, 20 ticking (all NPC and player
  blueprints), 23 skeletal meshes of which only the player's two and two props animate
  while unseen: the game already limits unseen animation on NPCs. This is a game choice,
  not the UE 5.6.1 constructor default (the constructor initializes option 0). 33 lights, 22 casting
  shadows, none movable. 11 particle systems, 3 active. The "animate only what is seen" fix
  changed 2 meshes and put them back.
- CSSX's own share of the frame: 0.1 ms mean, 1.3 ms p99 (the dev channel serving these
  probes), never a quarter of any hitch.

Shadows Ultra raises the page pool to 2048 and changes picture quality. The actor count
does not establish page overflow, and a larger pool has not been measured safely here.
The optional switches were measured in the later pass below.

## 2026-10-06, the optional switches (same session, a lighter area, 21 to 28 ms frames)

- Reflex off: off 28.1 / 27.2 ms, on 27.5 / 27.1 ms. No change. Reflex stays on.
- Nanite shadows on the compute queue (`r.Nanite.AsyncRasterization.ShadowDepths 1`): a first
  run read +9.3 % but the rounds disagreed (the scene changed under it); the rerun standing
  still: off 21.6 / 21.6, on 21.4 / 21.4 ms. No change.
- Nanite without mesh shaders (`r.Nanite.MeshShaderRasterization 0`): off 21.5 / 22.8, on
  21.5 / 21.4 ms, **3.4% lower frame time, consistent**. This is one scene on one driver.
  The tweak stays off by default and Tune measures it on the player's machine.

## 2026-10-06, Codex live acceptance (19:46 to 19:53)

Revised Perf Tuner only, on the same installed CSSX core and loader. Menu closed for
the full Tune run, near the tavern. Off/on/off/on, 5 seconds settling and 10 seconds
sampling per window. Results below are the averages of the two window medians/p99s.

| Comparison | Off median | On median | Off p99 | On p99 | Decision |
| --- | ---: | ---: | ---: | ---: | --- |
| Async Nanite shadows | 22.74 ms | 22.68 ms | 27.11 ms | 26.01 ms | No measured gain, off |
| Nanite without mesh shaders | 22.75 ms | 22.63 ms | 26.19 ms | 25.67 ms | No measured gain, off |
| Reflex off | 22.94 ms | 23.15 ms | 27.26 ms | 27.32 ms | No measured gain, Reflex restored |
| Default set together | 22.67 ms | 22.76 ms | 25.96 ms | 27.26 ms | Rejected, p99 5.0% worse |

Tune completed all five steps, then automatically disabled the tuner and restored
the originals. Independent bridge reads matched all 26 watched values after rollback,
after Stop tuning, and after Stop test during the changed-value window. Resolution
stayed at 66.6%; quality groups, DLSS, dynamic resolution, frame cap, v-sync, HWRT,
VSM pool and tessellation stayed unchanged. The resolution confirmation was inspected
and cancelled; no resolution diagnostic or animation fix was run.

Idle Perf Tuner tick cost averaged 13.4 microseconds over 15 seconds. CSSX overall
averaged 0.099 ms per frame, p99 0.617 ms, in the captured idle window. During a
50-actor scene scan, extension work peaked at 1.135 ms and CSSX at 3.417 ms. The
three-second scan window had no frames above twice its median, p99 26.80 ms and
maximum 27.12 ms. Individual actors can exceed the 500-microsecond scan budget.

The combined test's final capture included a 19.08 ms extension peak during its
transition/cleanup window, paired with a 27.60 ms game frame. Developer read-back
requests also affected this capture. This is not evidence of zero transition cost;
the five-second settling period excludes transitions from comparison windows.

These measurements do not establish a gain on other scenes, platforms or drivers.
