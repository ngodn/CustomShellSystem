# Eve stopping transition

October 6, 2026. Candidate correction installed for the user to test before recording.
Part of the [shared animation goal](../../extensions/cine/docs/ANIMATION-PORT-GOAL.md).

Questman reports a brief jog/back-foot lift when stopping with Eve locomotion
selected. The prior selector maps any speed above 20 cm/s with both walking
and sprinting flags false to Jog. A walk/sprint flag release during braking
therefore selects the jog blendspace before reaching Idle. This code path is
reproducible in the portable runtime tests. Its contribution to the reported
visual defect still needs a matched in-game capture.

The retained native `ABPL_Locomotion_MotionMatching.UpdateMovementState` dump
sets `HasAcceleration` from the acceleration vector length exceeding 5.
Evidence: `work/anim12/movement-state.json`, statements 268 through 370.
CSS reads this flag from the same current-player linked layer as `IsWalking`
and `IsSprinting`; it does not use a global actor lookup or another actor's
movement state.

`AnimationGaitSelector` retains the preceding Walk or Sprint only when both
mode flags are explicitly false, acceleration is explicitly absent, speed is
not increasing, and the ordinary selector would otherwise choose Jog.
It leaves the 20 cm/s Idle threshold unchanged. An explicit new gait, input
acceleration, invalid/missing state, pawn/animation-instance replacement,
selection change or release clears that preservation. The existing legacy
feminine selector and movement speed remain unchanged. Mixed Default/custom
slots still resolve their own gait option; retaining a Default gait does not
invent a custom animation.

UE 5.6.1 `AnimNode_BlendSpacePlayer.cpp`, `UpdateInternal`, reinitializes its
player when the selected blendspace changes. This candidate avoids the
unrequested intermediate asset switch. It does not overwrite engine node
clocks, change source speed, force an idle while still moving, or claim that
all gait phases and stop transitions are already smooth.

Verification:

- 80 animation runtime checks pass, including walk/sprint braking, unchanged
  idle threshold, resumed jog, sprint release while moving, explicit walking,
  partial/reversal input, missing acceleration/flags, increasing speed and
  reset isolation.
- Existing animation catalog and skeleton compatibility tests pass.
- The Windows `css_core` target builds and links successfully with C++23.
- Logs: `work/cine-animation-port/stop-gait-build.log` and
  `stop-gait-windows-build.log`.

Remaining: capture actual stopping flags, selected asset and visible foot
motion for walk/jog/sprint; verify starts, turns, reversals, mixed selections,
controller input, aim/attack interruption, revival/travel and native movement.
Measure frame times and preserve Eve, Unholy Genessa and Commander White's
post-process/physics behavior. If a phase or native-stop issue remains, fix
that separately before claiming the report resolved.

Recording trial installed after the user closed the game on October 6. The core
SHA-256 is `9b983b7441557c1791e81bdc537f8120609bb1cba4f76534091a35a03530b74f`.
The previous selector and DLL are backed up in
`work/cine-animation-port/recording-trial/`; `deployment.json` records the install.
The four animation/skeleton suites passed immediately before installation, and
the Windows core was current. Loader, outfits, CINE and saved settings were
unchanged. The installed loader matches beta.11 release metadata and host ABI 2;
its older loader-contract DLL checksum is stale. Live stopping acceptance is
still pending. The separate shared animation assets have not been installed.
