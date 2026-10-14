# Naylshotte draw, fire and stow

Actual Ctrl aiming and stowing pass on Peony/Faithful before and after the
user loaded another save. On the new save, a normal primary shot also produces
a 28-point Resolve drop. The permanent Burn effect renders while drawn and
stops rendering after stowing. The infusion survives. This extends the
[controlled visibility result](2026-10-15-sidearm-effect-visibility.md).
Damage, Barrage behavior, effect replacement and the original natural trigger
for owner-flag drift are not established by this test.

## Successful gameplay observations

Core: `css_core-development-d7cb0635d112f5fa-item-visibility.dll`, SHA-256
`d7cb0635d112f5fae73f67bc261c81d07df07eb0fef8797d786cc5d55191072f`.
PID 1944353 remains running. No production binary or asset changed.

- `work/three-outfit-rig-physics/sidearm-aim-focused-20261015`: actual Ctrl
  down/up passes draw and stow, with exact settings, CINE and player/view
  restoration. No firing input in this run.
- The user then changed saves and equipped Naylshotte with permanent Burn.
  Every new-save fixture resolves the new pawn and controller. CINE stopped
  on world change. This is not a reproduction of the cross-save Genessa-form bug.
- `work/three-outfit-rig-physics/sidearm-fire-newsave-20261015-5`: primary
  mouse-button state is observed in game; Resolve falls from 100 to 72. It
  returns to 100 at the next sample without a resource write by the fixture.
  The reason for that recovery was not investigated. Health remains 88.

| State | Socket | Owner/mesh hidden | Burn recently rendered |
| --- | --- | --- | --- |
| Drawn | Socket_Prop_R | Both false | True |
| After shot | Socket_Prop_R | Both false | True |
| Stowed | Socket_NailShotgun_Stowed | Both true | False |

The same permanent effect, asset and tags remain active. Complete saved CSS
state and CINE model match their baselines, all sampled keys are released, and
position, view target and control rotation match exactly. Actor yaw restores
within 1.4210854715202004e-14 degrees. The original fixture exits 1 solely
because it required bit-exact floating-point rotation equality. Its result is
preserved unchanged; a separate `verification.json` rechecks all result gates
using a 1e-8-degree angular tolerance, and passes. A negative control with
0.001 degrees of yaw drift fails. The diagnostic now uses that narrow tolerance
for actor rotation only. No additional shot was sent to repeat this check.

A later passive read, while recording the results, finds a different control
rotation but unchanged position, saved settings and released keys. The fixture
had already verified restoration at exit. The later view change is not attributed
to a cause, and no restoration is applied over that newer view. Its receipt is
`later-passive-check.json`.

`after-shot.jpg` shows the glowing weapon at the left edge; `stowed.jpg` shows
it absent. Framing crops much of the weapon, and the character is low in frame,
so these are diagnostic images. They do not prove projectile impact or damage.
The late last-ability sample remains Aiming Hold and is not claimed as evidence
of the firing ability identity.

## User-requested repeat shot

`work/three-outfit-rig-physics/sidearm-user-shot-20261015/` repeats one normal
shot at the user's explicit request after the broader interruption checks began.
The corrected fixture exits zero: mouse press observed, 28-point Resolve drop,
visible drawn Burn, hidden stowed Burn, unchanged infusion and full settings,
CINE and player/view restoration. No resource write is sent. This is a complete
passing execution of the corrected fixture, in addition to the separately
verified earlier run. It does not establish projectile impact or damage.

## Input and measurement corrections

The new-save directories without a suffix and with suffixes `-2` through `-5`
retain frozen recipes, results, images where available and SHA256SUMS.

- First attempt stops on lost focus before Ctrl. No firing input or resource loss.
- `-2` and `-3` send a short XTEST click but observe no Resolve expenditure or
  ability change. Focus alone did not establish an accepted mouse press.
- Pointer preparation before `-3` changes control rotation. No shot is sent
  during preparation. `-2/pointer-preparation.json` records both rotations;
  `pointer-restoration.json` records guarded restoration through the reflected
  controller SetControlRotation method and exact readback.
- `-4` observes the held mouse button and a 100-to-72 Resolve drop. Its later
  sample is already 100, exposing an incorrect final-value expenditure check.
- `-5` measures the observed drop rather than requiring it to persist. Draw,
  fire and stow pass; the remaining rotation-equality false failure is separately
  verified above. Both `-4` and `-5` register one 28-point drop each.

The test uses a 120 ms press plus one engine key-state read before releasing
in `finally`. It checks Hyprland focus, pointer containment and X11 window PID
before firing. It does not move the pointer or replenish resources. Ctrl is
released in outer cleanup. The diagnostic allows CINE stopped or between takes,
while rejecting an active take.

Input references: [xdotool upstream manual](https://github.com/jordansissel/xdotool/blob/main/xdotool.pod)
and [Hyprland dispatcher documentation](https://wiki.hypr.land/Configuring/Basics/Dispatchers/).
The local Hyprland 0.56.2 focus command is
`hyprctl dispatch 'hl.dsp.focus({ window = "pid:1944353" })'`.
The older focuswindow spelling was rejected before changing focus.

## Earlier unsuccessful preparation

## Keyboard attempt

`tools/diagnostics/check_sidearm_aim_visibility.py` reuses the Ctrl aim binding
from the earlier `work/cssx-native/probe-triarch.py`. The current player's 214
Enhanced Input mappings confirm LeftControl on all six regular/unblocked aim
press, hold and release actions. No mappings were changed.

Stage: `work/three-outfit-rig-physics/sidearm-aim-20261015/`.
The installed core is `d7cb0635d112f5fa`, PID 1944353, Peony/Faithful. CINE is
armed between takes and the native input-block list is empty. The foreground
window is a browser, not the game. The native developer key helper rejects
both down and release with `Key test requires focused game`. It sends no key.

The existing media driver's acknowledgement helper waits on `inventory.json`,
while this rejection is reported through `status.json`, so the fixture records
two timeouts. The frozen recipe and failure are retained. A separate
`focus-rejection-verified.json` confirms no held keys, unchanged complete saved
state, exact position/rotation/control rotation/view target and unchanged PID.
Do not call this a gameplay or visibility failure.

The current diagnostic checks the actual foreground window before any engine
request or input. A negative preflight now exits immediately, sends no command
and creates no output directory. It also checks focus again before pressing Ctrl.
The keyboard route remains the default and requires the game in the foreground.
The later focused check below succeeds.

## Engine-command attempt

Epic documents [Enhanced Input simulation](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine).
The local UE 5.6.1 `EnhancedInputModule.cpp` registers `Input.+key` and
`Input.-key`; `EnhancedInputSubsystemInterface.cpp` routes forced keys through
the player's controller and sends release on removal. This is a targeted game
input path, with no desktop focus change or global keyboard injection.

The diagnostic tried `Input.+key LeftControl` through the reflected
`KismetSystemLibrary.ExecuteConsoleCommand`, then always sent
`Input.-key LeftControl` in cleanup. Stage:
`work/three-outfit-rig-physics/sidearm-aim-injected-20261015/`.
No key-down state or sidearm draw was observed. The sidearm remained stowed and
hidden, and the Burn effect was not recently rendered. This is an unsuccessful
simulation attempt, not a passing aim test. The command call returning is not
proof the shipped game accepted it. Whether rejection is caused by the shipping
command path or background input handling is not established.

Full saved CSS state, infusion, CINE model, player/view transform and PID restore;
all sampled keys are up. The console route is retained as an explicitly
unverified diagnostic option, not a product setting or a claimed automation fix.
Continuous action injection was inspected but not called: its `FInputActionValue`
payload has no reflected fields in the local engine header, so guessing its
binary layout would be inappropriate. The initial continuous-injection map was
empty and no action injection was installed.

## Remaining scope

Effect replacement, Barrage projectiles, damage and other sidearms still need
separate checks. Preserve this draw/fire/stow result without claiming the entire
Tarstone report or full release goal is complete.
