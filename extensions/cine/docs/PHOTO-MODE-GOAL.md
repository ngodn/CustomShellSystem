# CINE 2.0.0: CINE inside the game's Photo Mode

Started 27 October 2026. Prompt: `aaa-prompts/CLAUDE_CINE_PHOTO_MODE_200.md`. Facts: `PHOTO-MODE-RESEARCH.md`
(every claim there has a source). Astra briefs: `aaa-prompts/ASTRA_CINE_PHOTO_MODE_200.md`.
Read this file first after a break, then the checklist at the end.

## What the user asked for

1. An **Activate CINE Mode** entry in the pause menu (System page), next to Activate Photo Mode, replacing
   CINE's current way in.
2. Every CINE feature inside the Photo Mode panel, built from the game's own widgets, reading as part of
   the game: presets, takes, route record, animation (idles, performances, montages, expressions), look
   track, guides, clean scene, breakdown, music scenes.
3. Version **2.0.0** (confirmed 27 Oct: 2.0.0 and the scene packs are not on Nexus).
4. Render a recorded take to a video file from inside the game, if possible.
5. Added 27 Oct: every preset group ships as its own UE mod (`.pak/.ucas/.utoc`) in
   `Content/Paks/~mods/CINE_<Name>_eins0fx_P/`, with its presets and music cooked inside.
6. Added 27 Oct: anything content-specific hardcoded in CINE's C++ moves into data a CINE pack carries.
7. Added 27 Oct: C++ and UE 5.6.1 best practice, no lag, no fps drop, no regression, every case handled.

## Decisions

| # | Decision | Why |
| --- | --- | --- |
| P1 | Packs by song: `IrisOut` (8), `SomethingAintRight` (Showreel 124, Showreel Finale, Commander White Release, Release 9:16), `TheyKnowNotWhatTheyDo` (Profane Proxima), `MarkOfTheBeast` (beast-p0..p4, g0..g4). | User, 27 Oct. |
| P2 | Base presets are packs too: `PoseGlide` (3/5/7/10 s), `Turntable` (+ Breakdown Turntable), `Walks` (Fashion Walk 360, Commander White Walk), `PhotoOrbit`. The main CINE download ships no presets. Fight presets and Profane Proxima Stills stay out. | User, 27 Oct. |
| P3 | Each pack's assets live under `/Game/CSSX/CINE/<Pack>/`. The main CINE pak keeps `/Game/CSSX/CINE/Core/` (breakdown materials, moved from the shared root). | User rule: extension assets under `/Game/CSSX/<EXTENSION>/`; no two paks may own the same path. |
| P4 | Pack data is one DataTable per pack, `/Game/CSSX/CINE/<Pack>/DT_CINE_Pack`, row struct `/Script/GameplayTags.GameplayTagTableRow`, the JSON text in its `DevComment` string. Rows: `manifest`, `preset.<id>`, `recipe.<id>`, `style.<id>`. | Stock editor Python fills it (`FillDataTableFromJSONString`); CSSX reads it (`table.rows`); no editor plugin to build. A string table would need a C++ editor helper (research doc, live facts). The tag manager ignores tables not listed in settings. |
| P5 | Music is a cooked `USoundWave` per track (`/Game/CSSX/CINE/<Pack>/Audio/SW_<track>`), played through `GameplayStatics.CreateSound2D` + `AudioComponent.Play(StartTime)`, with `SoundClassOverride` = the game's `Music` class. | Follows the player's music volume; runs through the game's audio instead of Windows PlaySound/MCI. UI sounds keep playing while the game is paused. |
| P6 | Packs are found by folder name (`~mods/CINE_<Pack>_eins0fx_P` with all three files), then accepted only when the engine loads `DT_CINE_Pack` and its manifest validates. A broken pack is skipped and named in the log and the panel; the rest keep working. New or removed packs need a game restart (paks mount at start). | No dependency on the asset registry; a folder alone is never trusted. |
| P7 | Content moves out of C++ into pack data (`recipe.*`, `style.*`): the built-in breakdown reel, cycle-palettes timing, body envelope and target bones, walk finale timings, transition defaults, guide list, breakdown colours/geometry/annotation texts/caption order, callout and signature style, picture profiles. Engine bindings (`/Game/Sparta/...` classes, game functions, cvar names), safety limits, restore logic and maths stay in code. CINE without any pack shows "No CINE packs installed" and the install line. | User item 6. Full list: Astra lane B report, hardcode table. |
| P8 | The generic pieces go into CSSX as mod-neutral API (CSSX never names CINE): a pause-menu entry an extension declares, and Photo Mode tabs an extension fills from its menu schema. | Prompt rule and decision D15. |
| P9 | CINE tabs appear only in CINE Mode. Activate Photo Mode stays the game's own panel, untouched. | Keeps the base game exactly as shipped; CINE Mode is a superset. User confirmed 27 Oct. |

## Way in

```
 SYSTEM                       (pause menu, WBP_MOT_System, BP_VBC_System)
 > Resume
   Activate Photo Mode
   Activate CINE Mode         <- new WBP_NB_System_C row, same font and highlight
   Restart
   Load / Save
   Main Menu
   Exit
```

Confirm runs the game's own chain (System `OnMenuClose`, UI handler `HandleOptionsMenu(0,true)`,
`ActivatePlayerMode`), then adds the CINE tabs to the panel and selects the first. Enabled exactly when
Activate Photo Mode is (`IsPlayerInterfaceEnabled`). The CSSX library card keeps an "Activate CINE Mode"
button that does the same.

## Panel mapping

The game's eight tabs stay first and unchanged. CINE adds six tabs in the same icon style, after a gap:

```
 [gear][cam][lens][sun][color][fx][eye][cloud]  |  [take][route][anim][scene][look][render]
 ---------------------------------------------------------------------------------------
 TAKE                                     (rows are WBP_NB_Photomode / _Slider)
 > Scene                  < Iris Out 9:16 >      pack: Iris Out
   About                    Laid out for Reels, 1:41
   Roll Take                F8
   Countdown              <        3 s       >
   Distance               [=====|----]  1.00x
   Height                 [====|-----]  0 cm
   Field of View          [=====|----]  1.00x
   Frame Guide            <      9:16        >
   Ease In at Walls       <       On         >
   Picture                < Sharp Shadows    >
   Signature              <       Off        >
 ---------------------------------------------------------------------------------------
 Take Shot  Rotate  Crane  Grid  Skip Frame  Hide UI  Reset All  Unpause  |  F8 Roll  F9 Stop
```

| Tab | Rows (Photo Mode voice, title case) |
| --- | --- |
| Take | Scene, About, Roll Take, Countdown, Distance, Height, Field of View, Frame Guide, Ease In at Walls, Picture, Signature |
| Route | Record Route, Route, Use Current View as Final Shot, Final Shot, Clear Route, Clear Final Shot |
| Animation | Library, Animation, Animation Speed, Start At, Expression, Refresh |
| Scene | Hide Characters (Nobody / Enemies / Enemies and NPCs), Keep Lock-On Target, Breakdown (Off / Built-In Reel / From Scene), Labels and Arrows, Include Lock-On Target |
| Look | Look Track (Off / Cycle Palettes / From Scene), This Take |
| Render | Render Take to Video, Size, Frame Rate, Quality, Last Render, Open Folder hint |

Scene choices list every installed pack's scenes, grouped by pack.

Tab strip (user, 27 Oct: "proper icon, scrollable tab, and proper spacing"):
- Six CINE icons drawn in the game's style (white line glyphs on transparent, same weight and optical
  size as `T_UI_Photomode_*_Icon`), not reused game icons.
- The strip scrolls: 14 tabs do not fit the panel. Measured live: each tab is a `WBP_NB_Menu_Image_C`
  with a 100x100 icon in a `HorizontalBoxSlot` padded 20 left and right; the eight game tabs want
  1120 x 100. `BP_HBC_Menu_Options` sits in `HorizontalBox_251` between the LB and RB prompts. CINE Mode
  moves the strip into a horizontal ScrollBox kept at the original 1120 width and scrolls the selected
  tab into view on every tab change; leaving CINE Mode puts the strip back in its own slot.
- Same spacing as the game's tabs (slot padding copied from a game tab), no gap hacks.

Live proof (27 Oct, Fable + user): a `WBP_NB_Menu_Image_C` tab and a `WBP_Menu_Option_Tab_C` page added
with `AddChildToHorizontalBox` / `AddChild` (parameter `content`) and a `GetNavigableChildren` rescan
gave 9 navigable tabs; `NavigateToCustomIndex(8,0)` moved selection and switcher together; the user
cycled the tabs by hand with Q/E and it wrapped cleanly; removal restored 8/8. The pause row was proven
the same way (`AddChildToVerticalBox`, `NavigateToCustomIndex` highlight). `UPanelWidget.InsertChildAt`
is not callable in shipping: insert by re-adding children in order (slots have no overrides).

## What CINE reuses from Photo Mode and what it takes over

| Part | Outside a take | During a take |
| --- | --- | --- |
| Camera | Photo Mode free camera, for framing; "Use Current View as Final Shot" reads it | CINE's own camera actor is the view target; Photo Mode camera state is saved and put back |
| Pause / Game Speed | The game's (Photo Mode pauses on entry) | CINE unpauses for the take and restores the paused state and Game Speed after |
| Hide UI | The game's (Tab) | The panel is hidden for the take and shown again after |
| Take Shot | The game's | Not used |
| Visibility, weather, colour, lens tabs | The game's | Left as the player set them; CINE does not reset them |
| Clean scene | CINE's hide (frozen, untouchable) | Same; Photo Mode's own Hide Characters is not used, so the two never fight over the same actors |

## Input

Photo Mode keeps all its keys. Free while the camera has focus, so they stay CINE's: F8 roll, F9 stop /
cancel, F10 guide, F7 final shot; D-pad up roll, D-pad down stop, D-pad right guide. In the panel, CINE
rows use the panel's own navigation (up/down, left/right, confirm, LB/RB tabs). The key bar gets CINE's
prompts on CINE tabs only.

## Life cycle and failure

| Event | Response |
| --- | --- |
| Photo Mode closed (Esc/B, Unpause path, Reset) | Any take ends, everything CINE changed is restored, CINE tabs removed |
| Panel rebuilt (every activation creates a new `WBP_MGT_Photomode`) | CINE tabs added again; all caches reset (menu-reopen rule) |
| World change / loading | Handles dropped, restore what is still valid, tabs re-added when Photo Mode reopens |
| Death, shell severed, pawn change | Take aborted and restored (rows already listen for death) |
| Pack missing or broken | That pack's scenes are not listed; the reason is in the log and on the About row |
| CSS missing or old | Look and wardrobe steps off with a visible reason; the camera works |
| Bridge call fails | Take aborted, restored, the error shown on the Take tab |

## Video render

Research: `work/photo-mode/video-render-research.md` (sources, plus encode tests under Wine 11.17,
Proton Experimental 11 and GE-Proton 11-7 on this machine).

- Media Foundation H.264/AAC encode fails under Proton (no GStreamer H.264 plugin, no AAC encoder) and on
  Windows N editions. Not used.
- Encoder: an LGPL `ffmpeg.exe` run as a separate process (no linking; CINE stays out of the LGPL). Encoder
  picked by a one-frame test at render start: Windows `h264_nvenc`, `h264_amf`, `h264_qsv`, `h264_mf`,
  `h264_vulkan`, then `libopenh264`; under Wine `h264_vulkan`, `h264_nvenc`, then `libopenh264`. Audio `aac`.
  Delivery (user, 27 Oct: "134mb unzipped is fine"): bundle the official BtbN LGPL `ffmpeg.exe` (pinned
  release, 134.5 MB, 55.7 MB zipped) in the CINE download under `eins0fx.cine/ffmpeg/`, with the GPLv3 and
  LGPLv3 texts and the exact source link.
- Timestep: `GEngine.bUseFixedFrameRate` + `FixedFrameRate` (reflected, works in shipping) for the render
  pass, so game time advances exactly 1/fps per frame however slow the frame is. Restored after.
  Skip Frame is not used (it unpauses for 0.033 s of real time, not one frame).
- Frames: one `Shot` console command per frame (shipping keeps it) into a CINE render folder, the take
  played at the fixed rate. ffmpeg then encodes the numbered frames plus the music. Pixels in memory would
  need a D3D12 Present hook or pattern-scanned engine addresses; not worth the risk for 2.0.0.
- Music: a second, real-time pass of the take with the scene's SoundWave recorded through
  `StartRecordingOutput` / `StopRecordingOutput` on a CINE submix (WAV), trimmed to the take and muxed.
- Movie Render Queue's runtime is in the game and could do all of this, but needs a level sequence built at
  runtime; untested, kept as a later option.

## Done criteria (user item 7)

- CSS host 14/14 and CSSX host suites pass; CINE logic and director checks pass; new checks cover pack
  loading, the pause entry and tab model, and every failure path above.
- Windows builds: no new warnings (CSS has 2 known, CSSX 0).
- Frame cost measured in game with `frame.brief`: Photo Mode idle with and without CINE tabs, mid-take,
  and outside Photo Mode (CINE must cost nothing there). No per-frame bridge traffic or allocation outside a
  take (extension performance standard).
- Live in Marrow Keep: open from the pause menu, every CINE row, a take per pack, route record, a take
  rendered to video, menu reopen, world change, death, and leaving Photo Mode returns the game exactly as
  it was (camera, pause, Game Speed, HUD, look, materials). Steam screenshots through CSSX for each UI check.

## Checklist

Worked through without hand-backs: drive the game through the bridge, fix, continue (user, 2026-10-28).

1. [x] Research: Photo Mode classes, wiring, labels, input, patch binding audit (lane A); hardcode audit and
   pack pipeline (lane B); live signatures (Fable).
2. [x] Video render route chosen and written here; renderer and ffmpeg fetch built (lane D, b3f6892).
3. [x] CSSX API: pause entry + Photo Mode tabs. Live-checked 2026-10-28: tabs, whole-tab scrolling, rows in
   the game's own look and spacing, keys, Reset All recovery, close returns the game's panel untouched.
   Cost on a CINE tab about 46 us a frame; opening spread to steps of 3 to 5 ms.
4. [ ] CINE:
   - [x] menu split into tabs, pack loader, SoundWave music
   - [ ] pause-menu way in, driven live (row, confirm, Photo Mode opens on the CINE tab)
   - [ ] a take in Photo Mode: unpause, panel hidden, camera, music, return to the panel
   - [ ] Render tab wired to the renderer (size, frame rate, quality, Render Take to Video, Last Render),
     plus the `SM_CINE_Render` submix in the core pak
   - [x] hardcoded content moved to data (lane B table, Astra lane E, dcd3472): shared style and recipes in
     `DT_CINE_Core` in CINE's main pak; a scene pack may override any of them for its own scenes
     (user, 28 Oct)
5. [x] Pack pipeline: authoring (DataTable + SoundWave), cook, pack, verify; eight packs built.
6. [ ] Tests and Windows builds (done criteria), frame cost in and out of Photo Mode.
7. [ ] Live checks: a take per pack, route record, rendered video, menu reopen, world change, death, leave
   Photo Mode with the game exactly as it was. Plus, after lane E: the cooked pak reads back
   `DT_CINE_Core` (15 rows, schema 1, bytes equal `assets/core/content.json`) and the inline font
   `FF_CINE`; in game the labels' font, both guide menus, a breakdown and a sequence look as before,
   a pack override applies only to its own scenes, CINE without the pak falls back with one
   warning, and frame cost in and out of CINE Mode; the HUD is hidden during takes; every row of
   Animation, Scene and Look pressed.
8. [ ] Release files: `extensions/core/dist/cine-v2.0.0/` and one folder per pack, changelog, texts, tag moved.
