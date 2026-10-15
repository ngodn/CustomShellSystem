# Native Genessa playback controls

Original Genessa's native human mesh passes the following checks while the
player remains in Stray form:

- Default Idle crosses its seven-second loop boundary and continues advancing.
- Confident reaches the end of its 0.2-second pose and holds both the CSS clock
  and the actual montage position.
- Both clips pause without position drift, seek, resume and release their
  temporary montages on explicit stop.
- A normal mapped Q parry interrupts native Iris Out at 1.138 seconds of its
  16.667-second duration. Two raw ability observations show the actual
  `GA_Parry_Action_Single`, rather than its always-active handler. The parry
  finishes with no remaining montage, action or input block.

The native interruption reason is `Player or animation binding changed`.
The native performance checks montage ownership before checking gameplay
blocks, so this differs from the custom facial-layer interruption reason.
The test observes the native action and early release together; it does not
claim an incoming hit, riposte or damage response.

Each montage uses `DefaultSlot`, one segment, rate 1 and the exact native-human
asset declared in the current shared manifest. No facial mapping is claimed.
The appearance uses `SK_Sester_Genessa_V6` and the game's
`SKEL_Human_Skeleton`. No Genessa form, upgrade or save-progression change was
needed for these tests.

The first run passed looping, then stopped because the fixture incorrectly
required every clip to exceed half a second. Its raw pose sample already showed
the valid 0.2-second clip holding. All restoration checks passed. The corrected
run tested only the outstanding pose and parry cases; it did not repeat the
accepted loop case or alter production code.

Evidence directories:

- `work/three-outfit-rig-physics/native-playback-controls-20261015/`
- `work/three-outfit-rig-physics/native-playback-controls-20261015-pose-parry/`

`check_native_playback_controls.py` drives the controls and uses the existing
guarded action fixture with its new explicit `--native-body` option. The default
facial path is unchanged. `verify_native_playback_controls.py` checks both runs
against 252 raw request/response pairs and verifies the parry's native ability
records. It checks actual montage positions and manifest asset paths, not only
the diagnostic's success flags. The failed duration assumption remains recorded.

Both runs restore the complete saved CSS state, Peony, equipment, resources,
position/view, CINE and released input. The separate final read-only check also
compares against the pre-core-activation baseline, including original idle,
nested weapon visibility, zero active/cached clones, inactive developer trials
and the installed/mapped core. All 15 conditions pass.

The game remains PID 1944353 with development core
`a5f4fef63056904f5821cb74aad8fd2ce3ea884ab970291a3d57781139b35d06`.
No production core or cooked assets changed. These results add representative
native control and parry coverage. Other native meshes, gait/contact quality,
continuous visuals and broader gameplay/lifecycle coverage remain separate.
