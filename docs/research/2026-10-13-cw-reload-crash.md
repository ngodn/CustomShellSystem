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

## Fresh-process transition comparison

`cw-class-lifetime1/check.py` completes three Original/Iris Out cycles with ten
class-state snapshots spanning 62 seconds. Both current animation instances
retain sparse class data and clear load/destruction flags. Previously used
normal-rig classes can expire after switching to the facial mesh; those weak
handles are recorded separately, not mistaken for the currently bound class.
CSS settings match the baseline and Linux process 3103064 remains unchanged.

`cw-class-rescan1/check.py` then performs one catalog rescan without a DLL
reload. Thirteen snapshots over 88 seconds pass, including all selected shared
animation availability. This narrows the failure: a rescan in a fresh core is
not by itself a reliable reproduction. It does not prove that rescans or normal
transitions are always safe. The accumulated prior reload state remains relevant.

A direct inventory of minidump memory streams confirms that neither the failing
instance nor its generated class or sparse-data field is included. See
`cw-class-lifetime1/dump-ranges.json`. More inspection of that same dump cannot
recover the absent class name. The development-only `animation.class_inventory`
probe therefore records current main/post-process instances for skeletal
components, their addresses, class identity/flags and sparse-data presence.
It excludes retired instances, reports truncation at 1,024 rows and its scan
cost, and runs only on explicit request. Native classes without sparse data are
not automatically classified as broken.

The development build and cold installation pass. Installed core SHA256:
`93e582f77c7c7d517efc43ba560d1f4fa51727dc6ff33cbb07703df67d973c1a`.
`cw-class-inventory1` retains the command recipes and receipts. The startup
helper initially found pending shader compilation and stopped before issuing
Continue. It then waited for that query to clear, without restarting again.
The first inventory was taken during loading and saw the native Sester rig;
it validates the instrument, not completed CW restoration.

Two subsequent catalog rescans pass six snapshots. Final verification checks
that every snapshot contains CW's facial class with sparse data, exactly four
selected animation slots and all four available. Facial performance remains
active, sunlight remains reduced with one owned light, and CSS saved state is
unchanged. All game animation classes observed have sparse data; the native
AnimSingleNodeInstance legitimately has none. The scan traverses about 322,000
objects and costs 34 to 69 ms in these observations, so keep it at explicit
checkpoints, never a per-frame monitor. Addresses are process-local evidence
and must not be reused after restart.

No crash repair is claimed. Next compare one controlled DLL reload against
this fresh-process baseline, retaining a class inventory immediately before
and after. Use the longer loader acknowledgement window and finish its actual
activation before restoring idle or starting another request. If the fault
returns, correlate the new dump's class/instance addresses with that process's
inventory before changing asset loading or class ownership.

## Three controlled reload cycles

The `cw-class-reload1`, `cw-class-reload2` and `cw-class-reload3` recipes reload
the identical 93e582f77c7c7d51 core under distinct filenames. Each first selects
the original idle, checks a stationary pawn with no montage, captures bound
classes, and waits up to 120 seconds for the exact loader acknowledgement.
Only confirmed activation permits restoration of Iris Out. Actual module maps,
process identity and complete CSS saved state pass every cycle.

Each cycle then performs two rescans with twelve class-inventory checkpoints,
followed by one reflected `KismetSystemLibrary.CollectGarbage` request and five
more checkpoints. UE 5.6.1 KismetSystemLibrary.cpp:2852 calls
`GEngine->ForceGarbageCollection(true)`; UnrealEngine.cpp:1837 schedules the
purge through the engine. The live zero-argument signature is checked first.
The object inventory counts decrease from 323758 to 323320, 323772 to 323324,
and 323705 to 323322 respectively after those requests. All 51 post-reload
checkpoints retain available idle/walk/jog/sprint choices, and all observed
bound game animation classes retain sparse data. Final CW facial playback and
sunlight ownership pass. Exact CSS state restores unchanged in the same Linux
process 3113971. Aggregate receipt: `cw-class-reload3/aggregate.json`.

The earlier crash and availability failures did not reproduce in this bounded
sequence. This is not a crash repair. Keep the release gate open, keep the
instrumented reproducer, and return to the primary foot-rig investigation.
Current DLL: `css_core-development-93e582f77c7c7d51-class-reload3.dll`.

Epic's [UE-215992](https://issues.unrealengine.com/issue/UE-215992) also describes
null sparse class data, but explicitly scopes it to editor/PIE child blueprints
and excludes standalone builds. It does not establish the cause of this shipped
game crash or justify applying its editor workaround here.
