# Perf Tuner: design and evidence

One goal: more frames per second without losing picture quality. Everything below is
either read from the game's cooked config, the UE 5.6.1 source, or measured; nothing is
taken from a tweak pack on trust. Dates are 2026-10-06 unless noted.

## What the game actually does (from its own files)

Sources: `MortalShell2/Config/DefaultEngine.ini`, `DefaultScalability.ini`,
`Windows/WindowsDeviceProfiles.ini` exported from pakchunk0 with the CUE4Parse tool
(copies in `work/research/ini/`), the reflected class dump (`CXXHeaderDump/Sparta.hpp`),
and the user's live config.

- UE 5.6.1, DX12 only (`PCD3D_SM6`). Lumen GI and reflections, Virtual Shadow Maps, Nanite
  (tessellation only at Environment Ultra), TSR by default with DLSS SR, NIS, Streamline
  (DLSS-G, Reflex), FSR 4 plugins shipped. No XeSS. Substrate off, HairStrands off.
- Lumen runs **software** ray tracing by default (`r.RayTracing.Enable=0` in the game's
  `[ConsoleVariables]`); the Video settings "Hardware Ray Tracing" switch writes
  `r.RayTracing.Enable=1` into the player's `Saved/Config/Windows/Engine.ini` through
  `USpartaGameUserSettings::SetPersistentConfigCVarValue`. Same for Frame Generation and
  Reflex. That file is the game's, so the tuner reads it and never writes it.
- The game tuned Lumen per quality level itself (probe resolution, downsample, radiosity
  update factors, `r.Lumen.Reflections.AsyncCompute=1` in the project). There is no slack
  left in those knobs that does not show.
- `r.Streaming.PoolSize` is 500/600/800 MB at Medium/High/Ultra and
  `r.Nanite.Streaming.StreamingPoolSize=512` with the comment "decreasing VRAM": both
  sized for small cards. `r.PSOPrecaching=0`, `r.ShaderPipelineCache.Enabled=1` (the
  bundled cache is replayed at start). `bSmoothFrameRate=True`, 30..120.
- `r.VRS.Enable=1` with no shading-rate image generator set: VRS does nothing here.
- The user's machine on 2026-10-06: RTX 3060 12 GB, i5-12400F, GE-Proton 11-7, DLSS
  Quality (67 % internal) + NIS, every group High, hardware RT **on** in the live config,
  DLSS-G on in the live config (not possible on an RTX 30). Heavily GPU-bound: 100 %
  internal resolution dropped 52 fps to 35 (CINE picture test, 6 Oct).

## Rules the code enforces

1. The catalog (`tweaks.json`) cannot name resolution, screen percentage, dynamic
   resolution, `sg.*`, frame caps, v-sync, mip or LOD biases, quality levels, the upscaler
   (`r.NGX.*`, `r.Streamline.DLSSG*`, `r.FidelityFX.*`), Lumen/VSM feature switches, the
   VSM page pool, or Nanite detail. `forbidden_reason()` refuses the file and the menu
   shows why. Every tweak must say in words what it changes and why it costs nothing
   visible.
2. Every variable is read before it is set, verified after (a cheat-protected or
   read-only variable in a shipping build does not take; the tweak is then rolled back
   whole and marked unavailable), and put back on stop or when turned off. Console
   variables belong to the process, so the record survives world changes.
3. Nothing is persisted by the engine: only `UGameUserSettings` properties and the
   `sg.*` block are written to `GameUserSettings.ini`; leaf `r.*/s.*/gc.*` variables are
   not. The tuner stays out of `sg.*`, so the player's config stays byte-identical.
4. A variable set from the console gets `SetByConsole` priority, above the game's
   scalability. Consequence: while the tuner has touched `r.Streaming.PoolSize`, the
   Textures quality in the Video menu no longer moves that one variable (it still moves
   mip bias and anisotropy) until the game restarts. Documented in the tweak text.
5. Measurement over trust. Every tweak can be measured off/on/off/on in the same scene
   (15 s windows, the first 5 s of each not counted while Lumen, VSM and the upscaler's
   history settle). A gain needs both on windows faster than both off windows by more than
   1.5 %; otherwise "same" or "unclear". Frame times come from the loader's frame ring via
   `frame.brief` (CSSX 1.3.1 adds a window length and percentiles), so the tuner has no
   per-frame work of its own.
6. Platform fit. The machine is read once (Wine's `ntdll` export, the DXGI adapter with the
   most dedicated memory; the adapter name decides the maker because DXVK can hide
   NVIDIA's vendor id). A tweak may say `platform`, `gpu`, `min_vram_mb`; values may scale
   with memory (`from_vram`). Steam Deck gets CPU-side tweaks only: its graphics settings
   are pinned by the game's Deck device profile and its memory is shared.

## The catalog, with reasons

| Tweak | Sets | Why it costs nothing visible | Status |
| --- | --- | --- | --- |
| Texture pool | `r.Streaming.PoolSize` = clamp(VRAM × 0.225, 800, 3000) | More resident mips, never fewer; the engine's VRAM clamp stays. 8 GB and up. | on |
| Nanite streaming pool | `r.Nanite.Streaming.StreamingPoolSize` = clamp(VRAM/16, 512, 1024) | Same geometry, fewer page swaps. Not read-only in 5.6 (`NaniteStreamingManager.cpp:55`). 8 GB and up. | on |
| No forced purge on level unload | `s.ForceGCAfterLevelStreamedOut 0` | Engine help text: the forced GC is "at the expense of a hitch". The game streams dungeons seamlessly. | on |
| GC pacing | `gc.TimeBetweenPurgingPendingKillObjects 120` (engine 61.1) | Half as many purge pauses; spacing, not fps. | on |
| Nanite shadows on the compute queue | `r.Nanite.AsyncRasterization.ShadowDepths 1` (default 0) | Same shadows; overlap depends on card and driver. | off, measure |
| Nanite without mesh shaders | `r.Nanite.MeshShaderRasterization 0` | Identical picture; under VKD3D mesh shaders are translated and NVIDIA has had slowdowns (vkd3d-proton #1632, #2429). Proton only. | off, measure |
| Reflex off | `t.Streamline.Reflex.Enable 0` | Latency, not picture. NVIDIA only. | off, measure |

Considered and rejected (each is in the engine source or Epic's docs as a look change):
`r.Lumen.Reflections.RadianceCache` (rough reflections from the cache: changes their
look), `r.Shadow.Virtual.NonNanite.IncludeInCoarsePages 0` (distant non-Nanite casters drop
out), `r.Nanite.MaxPixelsPerEdge > 1`, any Lumen downsample or probe resolution, TSR
history percentage, `r.VolumetricFog.GridPixelSize`, `r.HZBOcclusion 1` (late pop-in),
`r.OneFrameThreadLag 0` / `r.GTSyncType` (latency for throughput, the wrong direction),
`r.Lumen.*AsyncCompute` (already 1 by default or by the game), `r.VRS.ContrastAdaptiveShading`
(shading rate cut), `r.Skeletal­MeshLODBias`, `a.URO.ForceAnimRate`.

Dead in 5.6 (lines seen in packs that do nothing): `r.AsyncCompute`, `r.ParallelRendering`,
`D3D12.MaximumFrameLatency`, `D3D12.AsyncDeferredDeletion`, `gc.MultithreadedDestructionEnabled`,
`r.Nanite.VSMMeshShaderRasterization`, `r.UseShaderCaching`, `r.UseShaderPredraw`, every
`r.XGEShaderCompile.*`, `r.Shaders.*` and `b*` build setting. The Engine.ini reader finds
them by reading the variable back: `GetConsoleVariableStringValue` returns an empty string
for a name the engine does not know.

## The world audit

`GetAllActorsOfClass(Actor)` once, then per actor `GetObjectClass`/`GetClassDisplayName`,
`IsActorTickEnabled`, `K2_GetComponentsByClass` for SkeletalMeshComponent, LightComponent,
NiagaraComponent, and per component `VisibilityBasedAnimTickOption`,
`bEnableUpdateRateOptimizations`, `WasRecentlyRendered`, `CastShadows`, `Mobility`,
`IsActive`. About 300 reflected calls a tick, so a level of a few thousand actors takes a
few seconds with no hitch. The report goes to `world-audit-report.txt`.

The experimental option: meshes set to `AlwaysTickPoseAndRefreshBones` or `AlwaysTickPose` on actors
other than the player are set to `OnlyTickMontagesWhenNotRendered`.
This is not the engine default: `SkeletalMeshComponent.cpp:435` and
`SkinnedMeshComponent.cpp:448` initialize `AlwaysTickPoseAndRefreshBones`.
Montages continue unseen, but ordinary pose updates stop. Gameplay and moving shadows
can depend on those updates, so automatic Tune leaves this option alone. It is
reversible, re-applied after an audit of each new world, and measurable like a tweak.
Kept off by default until it has been played with; a game that relies on non-montage
animation driving gameplay off screen would show it.

Not done on purpose: update-rate optimization on NPCs (visible stepping at distance),
turning off shadow lights, culling particle systems, disabling actor tick (gameplay).

## Environment facts the tuner can only point out

Proton: DX12 through VKD3D-Proton; RT about half the Windows speed on NVIDIA; `VKD3D_CONFIG=descriptor_heap`
crashes this game on launch (ProtonDB); `single_queue` would kill every async win;
`DXVK_FRAME_RATE`/`VKD3D_FRAME_RATE` are second frame limiters; the NVIDIA driver cache
(`__GL_SHADER_DISK_CACHE_SIZE`) and `vkd3d-proton.cache` next to the exe carry the
pipeline compile; fullscreen gets direct scanout on Hyprland. Windows: NVIDIA Shader Cache
Size (the developer's own stutter advice), HAGS needed for DLSS-G, Radeon Boost/Chill are a
hidden resolution cut and a second limiter. Sources and the per-vendor table are in
`work/research/` (the two research reports) and the commit message.

## Measurement limits

Shipping build: `stat gpu`/`ProfileGPU` are compiled out; `stat unit` works on screen but
cannot be read back. The tuner measures whole frame time from the loader ring.
The separate resolution test measures pixel sensitivity, not CPU/GPU attribution.
Tune never runs it. A short trial cannot reliably measure GC spacing or streaming hitches.

## Menu direction (Codex takeover, 6 Oct)

Native Player Menu page for players who want one Tune action. ENERGY 1,
RHYTHM 1, MOTION 1. Uses the existing CSSX game widgets and their input bindings.

- Tune comes first because it is the main task. Enabled and Frame rate remain beside it.
- Manual Tests have their own tab so they do not compete with Tune.
- Stop stays available during scans and measurements. It cancels and restores originals.
- Labels name the action; descriptions explain the effect in a few sentences.
- Technical cvars stay with individual tweaks, where they help a manual decision.
- Results distinguish lower frame time from higher FPS; no automatic CPU/GPU verdict.
- No new visual assets, colors, typography, animation or host layout changes.
- CSSX displays label descriptions and button hints, and ignores their value fields.
  The tuner includes readings and results in those rendered fields. Label descriptions
  stay on one text line before wrapping; embedded newlines expose native rich-text tags.

## Takeover safety changes

- Saved originals survive failed writes, refused restores, world loss and unload retries.
  Restore succeeds only after independent read-back. A failed stop blocks DLL unload.
- Kismet's console CDO is cached. Its UE 5.6.1 implementation routes cvars through
  `IConsoleManager` with a null world context (`KismetSystemLibrary.cpp:539`), so cleanup
  can run after the pawn disappears. This still needs the new build's live check.
- Resolution diagnostics verify the change before interpreting samples. Failed changes
  produce no new verdict. Native menu confirmation explicitly describes the temporary change.
- Tune rejects optional gains when p99 frame time worsens by more than 1.5%. It disables
  the tuner and restores originals if the combined set loses or worsens p99 by that margin.
  Samples without frames and unbalanced sample sets are inconclusive.
- Scene scanning stops between actors when its native clock budget reaches 500 microseconds,
  in addition to the reflected-call cap. One actor's work can exceed that budget.
- The release packager includes only the manifest, DLL, catalog, README and notices.
- The catalog now uses the Engine.ini picture classifier as well as its forbidden-name
  rules. Fog detail, tessellation, ray tracing, NIS and secondary screen percentage are
  covered by regression checks.

## Still to do (live)

Run the revised Tune flow end to end, inspect the page with keyboard/controller/mouse,
verify Stop and unload restore live cvars, and measure extension cost during the scene scan.
The experimental animation option needs combat and shadow checks before automatic use.
Existing RTX 3060 findings are in `findings.md`; other platforms remain unmeasured.
