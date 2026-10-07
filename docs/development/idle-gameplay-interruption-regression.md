# Idle interruption and blocked gameplay, October 7

The installed facial-idle build could cancel native gameplay while releasing a
cosmetic idle. The user reported attacks, aiming, doubles and Genessa Form failing,
then controls remaining blocked after changing back to Eve Default Idle.

## Evidence

Read-only capture in `work/cine-animation-port/gameplay-cancel-regression-1` found
Commander White with Default Idle, no main montage or active notify state, and two
`SpartaInputComponent.ActiveBlocks` entries with `BlockBehavior=BlockAll`. Movement
and look were not ignored. These entries explain the blocked actions; their precise
creation history was not captured. No input blocks, abilities or save data were
cleared by the probe.

UE 5.6.1 `SetOverridePostProcessAnimBP(..., true)` calls
`InitializeAnimScriptInstance()` with its default force-reinitialize flag.
`UAnimInstance::UninitializeAnimation()` stops and destroys montages. Keeping the
same main-instance pointer therefore does not prove that gameplay survived.
The old release path ran this setter when an incoming gameplay montage interrupted
an idle. The isolated engine test reproduced that montage loss with the pointer
unchanged. It does not reproduce Sparta's complete input-block lifecycle.

## Changes

- Disable the cosmetic pose and restore facial values and weapon visibility immediately.
- Defer graph restoration while a montage, input block, aiming, movement, travel,
  menu or other existing gameplay guard is active. Retry cleanup at 100 ms intervals.
- Retain the adapter and source assets until cleanup completes or the binding changes.
- Capture current linked weapon layers and customization at restoration time, since
  either can change while cleanup waits. Keep the snapshot through a failed restore.
- Recheck gameplay after loading an adapter and before its initial graph replacement.
- Keep regular custom idles out of gameplay input blocks and montage blend-out.
- Stop owned playback for a requested Genessa Form change and prevent idle restart
  until that request completes or expires. Native form events are still sent once.

## Verification

The isolated UE 5.6.1 montage fixture passed 18 checks, including reproduction of
old cleanup losing a montage and deferred restoration preserving playback and
blend-out. Source asset hashes were unchanged. This is engine-level evidence,
not live game acceptance. All 23 host CTest suites passed, including recovery,
profiles, animation runtime, skeleton compatibility, MISC and Astral lifecycle.
The Windows runtime build passed. Candidate `db5207d4722ca6af` is staged;
installation and live acceptance are pending.

No changes to outfit packages, CW eye materials, shared animation data, profile
shortcuts, Heart of Vatra visibility, SHELL row layout or the earlier gate/aiming
repair paths are included. Their relevant existing host checks passed. The UI and
native gameplay paths still require the checks below in the installed candidate.

## Live checks still required

- Start from the new process, attack and aim with Default Idle.
- During a body-only new idle and a facial idle, attack, aim, dodge, summon and walk.
  Each action must take over and finish. Idle and face may resume only afterward.
- Request Faithful and Stray forms during a new idle. Check form, controls and both
  doubles modes afterward. Preserve Commander White's visible eyes.
- Change weapon and appearance after interruption, then aim and attack again.
- Confirm gate/launchpad recovery, Harbinger appearance and selected movement,
  profile Enter save, SHELL status placement and Heart of Vatra visibility.

The user deferred the After Brute Battle crash investigation. This patch does not
claim to resolve that historical report or guarantee that no regressions remain.
