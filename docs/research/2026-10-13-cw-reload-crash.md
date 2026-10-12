# CW reload and catalog-rescan crash

The October 13 continuation makes progress from the previous turn's completed
menu verification and recovered animation playback. It does not establish a
repair for the intermittent asset failures or CW's feet.

## Trigger and observations

The same diagnostic core, SHA-256
`9b8cac120625639cc93db37c833774abb16fb2e76467c4db78411d54ee528331`,
was loaded a third time under a fresh filename. Its activation exceeded the
script's 20-second acknowledgement deadline. The loader later acknowledged the
new core and the live process mapped it. The original idle restoration raced
that delayed activation. A separate acknowledged request restored the exact
saved CSS state, and fifteen follow-up samples passed. See
`work/three-outfit-rig-physics/cw-asset-evidence3/late-recovery.json` and
`late-verification.json`. The initial failed verification is not an asset-failure
reproduction: the intended idle had not yet been restored.

A smaller loop in `cw-reload-rescan1/check.py` requests catalog rescans without
reloading the DLL or changing saved settings. The first rescan passes its
animation/facial check. After the second request, the process exits before its
acknowledgement. The retained log records one completed catalog reload. The
crash is therefore after a successful rescan, not evidence that the second
rescan handler itself faulted.

## Crash evidence

The Windows crash report is
`UECC-Windows-39AA02BE442D9BC3F945EB9ED7B01B16_0000`, timestamped
2026-10-13 03:06:16 local, Windows process 372 (Linux process 2877148).
It records an access violation reading `0xa8`. The October 10 report
`UECC-Windows-9D9FE36243F2DD2703BFA8A8A1D35E75_0000` has the same
module-relative stack signature. That match does not prove the same trigger.
No Linux coredump exists for this process, and the checked kernel-log interval
contains no OOM kill. Available memory after the exit was about 19 GiB.

LLDB loads the original minidump and exact game executable/PDB. The game-thread
stack reaches `UAnimInstance::UpdateAnimation`, through
`UAnimBlueprintGeneratedClass::ForEachSubsystem`, then faults while loading the
subsystem virtual table at `RCX=0xa8`. The pinned engine implementation computes
that subsystem from `GetConstantNodeData()` plus the reflected property offset;
`GetConstantNodeData()` returns `SparseClassData`. Missing constant class data
is the leading inference. No CSS callback appears in the faulting frames, but
that does not exclude an earlier CSS lifetime error.

The small dump does not contain the animation instance's memory, so its class
and owning component cannot be identified from those optimized frame arguments.
Do not name CW's post-process graph as the culprit without new evidence.
PDB field offsets agree with the running UE4SS layout: `SparseClassData=0x118`,
`SparseClassDataStruct=0x120`.

Evidence, commands, all thread stacks, disassembly, the last CSS log interval
and pre-recovery state are in `cw-rescan-crash1`. The original minidump and its
bundled save backups remain in the game's crash folder. No broad dump copy was
made.

## Next discriminating check

Ranked hypotheses:

1. A class or its dependencies are released while an animation instance still
   uses them. Predict a class identity/flag change or missing constant data
   after restoration/rescan.
2. A partially loaded class is returned from a cache. Predict load/post-load
   flags or missing constant data before the class first ticks.
3. A stale cache entry names another object. Predict an identity/path mismatch
   or failed object-array lifetime guard.

The development-only `object.state` probe reports object identity and flags,
its class handle, and, for classes, whether sparse data and its struct exist.
It uses the SDK's versioned accessors, whose offsets were checked above. It
never writes class data. The probe is not a repair or a public control.

## Recovery

Before restarting, saves and CSS state were copied and hashed into
`cw-rescan-crash1/recovery`. Only this test's exact pending rescan request was
removed. The new diagnostic core is
`d4d4f6318b568b41eb3709c9620e232de2fa69e496a51a6d1201b8f196114879`.
The crash reporter initially kept the Steam app session active. After its exact
PID and crash GUID were verified, it was terminated and Steam was asked to
start the game again. Fresh process 3088830 (Wine 388) reached the main menu, but its menu widget was
absent and the viewport black. Calling the handler's reflected CreateMainMenu
created it. Normal ContinueGame loaded the existing Core World save. The live
ABP_Player and CW ABP_FacialPerformanceStrands1 classes both have sparse data
and no load/destruction flags. All selected locomotion assets are available;
CSS state matches the backup exactly. Receipts: recovery/continue.jsonl,
animation-class-state.jsonl, world-verification.json and world-recovered.jpg.
No save files were overwritten during recovery.
