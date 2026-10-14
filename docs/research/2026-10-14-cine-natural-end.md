# CINE natural completion survives face restoration

An actual Genessa portrait take exposed a CSS playback bug: Iris Out reached
its natural endpoint, then the normal appearance-mesh restoration replaced its
successful outcome with `Appearance mesh is changing`. CINE treated that reason
as an interruption and aborted the take. The repaired development core now
completes the same take on both Genessa variants.

## Cause and fix

`PerformancePlayer::prepare_mesh_change` calls `interrupt` during mesh
restoration. Previously, `interrupt` assigned a failure reason even when the
session had already stopped. `PerformanceSession::interrupt` now returns whether
it interrupted an active session. The player assigns the reason only in that
case. Cleanup still runs every time, including deferred restoration retries.
Holding and paused playback count as active; a naturally completed or fully
stopped session does not. The command path keeps its explicit binding-error
message independently of the stored completion reason.

This does not suppress CINE errors or change its interpretation of real
interruptions. No mesh, material, motion preset, CINE source or cooked package
was changed.

## Verification

The focused host tests pass: 45 performance-clock checks and 96 session/resolver
checks. Coverage includes active holding and paused interruptions, repeated
cleanup, natural completion, explicit stop, and retained token/clip/endpoint.
The Windows development core builds successfully with `CSS_INVENTORY_DEV=ON`.

Build and controlled activation evidence:
`work/three-outfit-rig-physics/performance-end-fix-20261014`.
It contains source hashes, host-test output, build receipt, activation recipe,
loader acknowledgements, before/after animation-class probes and checksums.

- New core: `css_core-development-54387cf5e27911f8-performance-end.dll`.
- SHA-256: `54387cf5e27911f83c64df97d8f7fb3d59c37d97e04024ee20cb0dcb09f5c8d1`.
- Prior core retained: `css_core-cw-walk-acab6363906f0e88.dll`.
- Same game process, PID 1944353, survived the stationary reload. New code was
  mapped and old code unmapped. Full saved CSS state and idle choice restored.

This is another controlled stationary reload result, not evidence that the
earlier reload crash is resolved in all animation-worker states.

The actual CINE fixture is `tools/diagnostics/check_genessa_cine_take.py`.
It creates a temporary 20-second photo preset with Iris Out, captures three
portrait stills per variant, and observes the owned performance token through
natural completion. It uses the supported CINE take action, not a direct
performance start. The saved CINE look is temporarily set to Off to allow the
stationary performance and then restored.

| Variant | Endpoint observed | Inactive samples with empty reason | CINE outcome |
| --- | ---: | ---: | --- |
| Eve Skin | 16.66666603088379 s | 6 | Take finished |
| Genessa Skin | 16.66666603088379 s | 5 | Take finished |

Successful evidence:
`work/three-outfit-rig-physics/genessa-cine-takes-20261014-3`.
The recipe, preset, state snapshots, sampled status, six stills, independent
receipt verification and checksums are retained. Player position/rotation,
controller rotation and view target compare exactly before and after each
take. Peony and both complete CSS/CINE saved JSON objects restore exactly.
The temporary preset was removed only after verifying its bytes, then the
preset list was reloaded. No movement input, form switch or save-game edit was
sent.

The six stills show visible glowing eyes and differing facial poses, including
an open mouth in the Eve Skin late sample, without an obvious collapsed face.
The capture threshold names are not exact pose timestamps: screenshot latency
means the two variants' stills are not matched-frame comparisons. The dark scene
also has harsh shadow patches on face, hair and body. These images do not close
the lighting defect, continuous lip-sync, neutral-pose, gameplay interruption,
both-form doubles, lifecycle or performance gates.

## Earlier attempts and cleanup

`genessa-cine-takes-20261014-2` retains the reproducible pre-fix failure: token 25
stopped at the exact endpoint with `Appearance mesh is changing`. Its recovery
restored Peony and both saved states exactly and removed the temporary preset.

The first attempt, `genessa-cine-takes-20261014`, failed because the fixture
initially left CINE Look enabled. Its partial variant switch also exposed a
diagnostic restoration mistake: outfit-level remembered customization follows
the last variant, even when the per-variant memories remain intact. The retained
recovery receipt verifies exact restoration after selecting the original
matching memory variant. The fixture now handles both conditions. These were
fixture errors, separate from the reproduced natural-end bug.

The removal list was recorded in
`../development/cleanup-2026-10-14-genessa-cine-test.md` before testing. Only the
temporary installed preset was deleted; sources, evidence and rollback remain.
`df -h` before this work reported root 92G and theark 67G available; afterward it
reports root 91G and theark 66G. These are rounded shared-drive observations,
not a claim that this small diagnostic alone consumed the difference.
