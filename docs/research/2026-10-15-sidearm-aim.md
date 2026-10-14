# Sidearm gameplay test preparation

Actual Naylshotte aiming/firing is not verified by this record. Two attempted
input routes failed to activate aim, and neither changed the equipped weapon,
infusion, saved CSS state or player/view transform. The
[controlled Burn visibility result](2026-10-15-sidearm-effect-visibility.md)
remains valid but is not a substitute for gameplay input.

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
The user has been asked to bring it forward when convenient.

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

## Next action

Run the guarded keyboard fixture once the game is focused. It requires a draw
to an in-hand socket with visible actor/mesh, followed by hidden stow and no
recent Burn rendering. It checks infusion and settings preservation, releases
Ctrl in cleanup and restores only a bounded stationary aiming transform change.
It does not yet send fire. After aim passes, extend the gameplay check to one
normal shot and effect replacement; current Resolve is 100 and the documented
Naylshotte primary shot cost is 28. Record actual resource changes rather than
silently replenishing them. No runtime binary or installed asset changed here.
