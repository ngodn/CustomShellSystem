# After Brute Battle repeat investigation

The user reported a game crash near the end of this idle on Unholy Genessa.
The crash remains unresolved. No save corruption has been established.

The cooked clip has 821 finite key poses and 388 tracks. Its root is stationary,
but pelvis displacement from the last pose to the first is 1092.79 cm with
130.42 degrees of rotation. Independent source readback agrees. This is scene
travel, not established compression corruption. Evidence:
`work/cine-animation-port/after-brute-loop-audit-1`.

## October 7 isolated simulation

`CSSPerformanceEvaluation.cpp` supports an opt-in commandlet diagnostic with
`CSS_IDLE_REPEAT_AUDIT=1`. It evaluates the original outfit post-process idle
graph at 60 Hz for three loops, waits for animation and cloth tasks, then
checks bone transforms, cloth positions/normals and component bounds.
It does not save assets or change the game installation.

The first fixture produced zero cloth frames because preview physics was
disabled. Its exit 255 is a failed test setup, not evidence about live cloth.
Enabling physics in the isolated preview scene produces six cloth sections
and 3,420 particles on every one of 2,470 frames. Three wraps complete with
finite data throughout, exit 0. All five fixture asset hashes remain unchanged.

The first wrap increases the bounding sphere radius from 166.46 cm to
631.68 cm, then returns to 152.76 cm on the next frame. The three wrap radii
are 631.68, 633.13 and 631.87 cm. Peak particle distance from the component
origin is 1282.23 cm during the scene travel. It is not a per-frame displacement
or a cloth error measurement.

Evidence: `work/cine-animation-port/idle-repeat-ue-2`, including commands,
exit receipts, protected asset hashes and all frame samples. The earlier failed
fixture is retained as `idle-repeat-ue-1`.

This NullRHI commandlet does not exercise the game's GPU rendering, map,
customization or full gameplay animation graph. It cannot clear the reported
GPU crash or establish the bounds jump as its cause. Next investigation needs
to distinguish rendering behavior at the repeat from invalid simulation data
in the actual runtime. Keep the complete clip and source motion intact.
