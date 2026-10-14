# Native Genessa CINE completion and cancellation

Original Genessa completes a full Iris Out CINE take and releases the native
montage. A second take also releases it when CINE is explicitly exited during
the clip. Both runs restore Peony, saved CSS/CINE settings and exact player/view
transforms. These are actual CINE takes, beyond the earlier idle-binding check.

## Evidence

- `work/three-outfit-rig-physics/native-cine-live-20261015/`: full take.
- `work/three-outfit-rig-physics/native-cine-cancel-20261015/`: explicit exit.
- Recipe: `tools/diagnostics/check_genessa_cine_take.py --native`.
  Add `--cancel-at 3` for the second case. Each stage retains its executed recipe.
- Installed core SHA-256:
  `d7cb0635d112f5fae73f67bc261c81d07df07eb0fef8797d786cc5d55191072f`.

The diagnostic extends the existing Unholy Genessa CINE fixture with a native
target and explicit cancellation. Default behavior still tests both Unholy
Genessa variants. It uses the existing CINE preset, take and exit actions and
the CSS appearance service. It does not write montage fields or animation poses.

Both live montages have one `DefaultSlot` track and one segment referencing
`/Game/CSS/eins0fx/Shared/Locomotion/NativeHuman/Studies/L5X/AN_IrisOut.AN_IrisOut`.
The segment rate and loop count are both 1. The native body clip is 16.666666
seconds long.

| Case | Result |
| --- | --- |
| Natural end | CINE reports `Take finished.`; the owned performance reaches 16.666666 seconds, stops with no interruption reason and releases its montage |
| Explicit exit | Exit requested at 3.568 seconds; the same token stops at 3.885 seconds and the montage is released |
| Restoration | Both runs restore full saved CSS/CINE JSON, Peony and exact player position/rotation, view target and control rotation |
| Input ownership | Explicit-exit run starts and ends with an empty `ActiveBlocks` array; no keyboard or mouse input sent |
| Temporary preset | Removed after both runs, followed by preset reload |

The natural take retains three inspected portrait screenshots near clip seconds
3, 10 and 14. They show the native head, eyes and upper-body poses. They do not
cover feet or full-body contact. Native facial playback is not mapped by the
current performance implementation; body compatibility does not imply facial
track support.

The performance status and montage position are read sequentially. One sample
straddles the end transition and reads a reset montage position after a late
playing status. The final stopped status, released montage and normal CINE
completion agree. These reads are not a synchronized clock-error measurement.

Both commands exit zero with PID 1944353 unchanged. Receipt inspection verifies
the recorded segment identities, completion/cancel conditions and saved JSON
equality. Each directory retains `verified.json` and `SHA256SUMS`.
No installed binary or cooked asset changed.

Remaining native scope: other clips and native meshes, loop/pose-hold behavior,
gameplay interruption, full-body visual/contact review and performance. This
does not close the full three-outfit release matrix.
