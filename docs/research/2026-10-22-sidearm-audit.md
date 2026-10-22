# Sidearms on game build 26073152

The report "All sidearms weapons have bugs" does not identify a symptom,
outfit, CSS version or reproduction. The installed CSS 1.0.2 passes the checks
below. This does not establish what happened on the reporter's installation,
and no new release or runtime fix follows from this audit.

Evidence: `work/sidearms-update-26073152/verification.json`, backed by request
receipts, recipes and native game screenshots in the same directory. The core
loaded by the game is `css_core-1.0.2.dll`, SHA-256
`d465c84db60f88f9f1d6b305b302ebc48eb187b9691960f832f25478b41e3ad9`.
Steam's installed build is 26073152.

## Completed checks

All eight owned sidearms pass draw, primary activation with Resolve expenditure,
and stow on four appearances: original Genessa, Eve Skin Suit, Commander White
Original and Unholy Genessa Eve. This gives 32 appearance/weapon combinations.
The original-shell control still has CSS loaded; it is not a clean unmodded game.

| Sidearm | Primary observation |
| --- | --- |
| Trebuchaxe | 30 Resolve spent |
| Crossbow | 17 Resolve spent |
| Parasite Gun | Expenditure observed; longer held input fires more shots |
| Cursed Child | Primary activation and expenditure observed, in addition to its passive aiming drain |
| Ballistazooka | 40 Resolve spent after allowing draw/wind-up time |
| Machine Gun | Expenditure observed after allowing draw/wind-up time |
| Lute | 20 Resolve spent |
| Naylshotte | 28 Resolve spent |

Secondary input is also exercised on the current loadouts. Naylshotte spends
25 Resolve in each appearance. An unchanged resource value for the other
secondary inputs is not treated as proof that a secondary ability fired.
Tarstones and progression are not changed to add abilities for this test.

The eight sidearms also pass the visibility sequence: hidden while stowed in
Only when in use mode, visible when drawn, hidden by Hidden mode, visible again
after restoring Only when in use, then hidden after stowing. Trebuchaxe's native
projectile preview remains hidden and unrendered when stowed.

Four additional checks cover Naylshotte on Eve Casual Knitwear and Commander White,
each with the Eve body idle and with an explicit Iris Out performance. Aim draws
the weapon, interrupts active facial playback before the clip ends, permits a
28-Resolve primary shot, and returns to hidden stow. The original custom idle
selection restores afterward.

## Corrections to the diagnostic

- Infinite Resolve was active at entry. Initial samples could miss spending
  because the cheat refills the resource. Later firing checks temporarily disable
  only that refill and restore its original value. God mode and Auto Heal remain
  as configured by the user.
- The first short firing window did not establish Ballistazooka or Machine Gun
  activation on the original shell or Eve. `heavy-ready.json` repeats those four
  combinations with a longer draw wait and hold, and records expenditure on all.
- A first sweep stops before Naylshotte input because desktop focus changes.
  Its seven completed visibility cases remain valid; `idle-visibility.json`
  supplies the eighth case.
- The first appearance matrix stops at a diagnostic performance-stop request
  missing its required token. Its sixteen completed original-shell/Eve cases
  remain valid. Later checks let actual aiming interrupt the animation.
- One background Unholy Genessa/Machine Gun attempt stays stowed across a
  desktop-focus transition. That record does not establish accepted aim input
  and is inconclusive. `unholy-remaining.json` records accepted Ctrl state,
  successful draw/fire/stow and the remaining three cases.
- Enhanced Input vector injection returned without error but did not draw the
  weapon in the tested background state. Keep `background-proof.json` as a
  rejected input route, not a gameplay failure.

`verify.py` checks the accepted rows across the retained attempts, including the
four longer firing repeats and four idle cases. It requires a complete 32-pair
matrix, draw/stow sockets, visible drawn weapons, resource expenditure, eight
visibility checks, interruption assertions and restoration receipts.

## Input route for subsequent sessions

Use the already verified X11 route before foreground automation: resolve the
shipping-game PID, find its visible X11 window, validate `getwindowpid`, then
send `xdotool keydown/keyup --window WINDOW_ID Control_L` and
`mousedown/mouseup --window WINDOW_ID BUTTON`. Address releases to the same
validated window in `finally`. Button 1 is primary and button 3 is secondary.
Check game input and resulting action state separately. Never reuse this audit's
PID, X11 window ID or engine handles after a restart.

The later firing and idle checks use targeted input without focus commands or
pointer movement. The user can keep working in another application. Native
`shot showui` captures only the game. Exact scripts remain with the evidence;
the shared `ue-modding/GAME-CONTROL.md` skill contains the reusable procedure.

## Restoration and limits

Final readback matches the original Parasite Gun definition in both controller
and runtime save object, equipment unlock state, CSS settings, Unholy Genessa
Eve appearance/customization, Health/Resolve and cheat control values. All
sampled aim/fire inputs are released. The core hash is unchanged. The added
UE4SS log segment contains no error, failure, exception or warning matches.

Position and camera changed during the session. The final position differs by
about 602 Unreal units from the initial snapshot; this audit does not attribute
that movement or overwrite the newer view/position. Normal autosaves are retained,
with preflight save copies available. Whole-save/inventory equality is not claimed.
No enemy damage or projectile-hit assertions are made. No mod binaries, cooked
assets or release archives are changed.

To investigate the original report further, request the CSS/outfit versions,
what specifically goes wrong (firing, pose, visibility or crash), and a short
clip or steps. Include UE4SS.log if available. A useful reply is:

> Could you describe what happens with the sidearms, and share your CSS version,
> outfit and a short clip or steps? I checked all eight on the prepared 1.0.2
> build but haven't reproduced the issue yet. UE4SS.log would also help.
