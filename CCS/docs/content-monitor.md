# Installation change monitoring

The experimental menu now checks the current installation on a dedicated CPU worker before publishing loaded-move observations. The passive build and discovery/attack probes do not construct this worker. The monitor is compiled locally and has not been installed or exercised inside the Windows game.

The executable path comes from `GetModuleFileNameW(nullptr)` with failure/truncation checks. Like CSS's startup resolver, the worker searches nearby ancestors for `Content/Paks`, independently of UE4SS's Mods directory. It inspects the executable, recursively finds `.pak`, `.utoc`, `.ucas` and `.sig` files including nested mod directories, and includes a loose `Content/AssetRegistry.bin` when present. Each entry records relative path, byte count and modification timestamp. The manifest retains the absolute executable/package roots so a different installation does not reuse the same observation.

The worker takes two consecutive complete manifests and rejects disagreement, missing files, inaccessible directories, links, empty package sets, paths longer than 1,024 UTF-8 bytes, trees beyond eight levels or more than 4,096 inspected entries. It checks cancellation and a five-second deadline between filesystem operations. An individual operating-system call can still block; the deadline is not an interruptible I/O guarantee. No package archive or executable bytes are read, no asset is loaded, and no game object is accessed by the worker.

## Scheduling and lifetime

The worker starts idle. Requests are scheduled only while CCS is visible, at most once per minute after an accepted request. The game thread polls a published immutable CPU result at most four times per second. Request and poll use try-locks and do not wait for directory inspection. Only one scan can be pending/running; the monitor has no growing queue. Windows background processing mode is requested on the worker, and its result is exposed in status rather than assumed to succeed under Wine. Publication releases prior manifests outside the shared gate. Repeated suspended states use string views and do not allocate temporary error strings each frame.

A failed inspection removes loaded observations and pauses discovery. A later successful initial/recovery scan can resume. Once a complete manifest differs from the first successful manifest, `restart_required` stays latched even if files are returned to the earlier state. The running game may still have old containers mounted, so CCS does not combine those observations with new files on disk. UI empty states distinguish inspection pending, unavailable and changed. Existing presets stay intact.

Stop requests cancel pending/active inspection. `Core::stop` returns incomplete until the worker reports that it exited; it does not join a filesystem scan on the game thread. Destruction joins the CPU worker after cancellation, without touching Unreal objects. Unexpected worker failures report a stopped worker and suspend observation instead of escaping the thread and terminating the process. Full core switching still needs the separately tracked unload/restore verification.

## What this evidence establishes

This is a disk metadata change detector. It is not a cryptographic content identity, a Steam build lookup, a mounted-container inventory or a native binary compatibility certificate. A same-size edit that restores the timestamp is deliberately shown as undetected in the host tests. Two scans also cannot create an atomic filesystem snapshot. Runtime mounts outside the discovered package root, linked mount locations, and native adapter call bytes require separate validation.

There is still no persisted discovery cache consumed by CCS, and the extracted catalog remains offline-only. A future cache needs content digests, discovery schema and verified live interface/mount provenance; it cannot enable a move based on this manifest. All currently observed moves remain combat-ineligible. Live selector/property checks remain necessary after every game/object generation change. See [runtime discovery](runtime-discovery.md) and CSS's [game-build compatibility record](../../docs/development/game-build-compatibility.md).

## Validation

Twelve host suites pass normally and under ASan/UBSan. The content suite checks nested/case-insensitive containers, loose registry inclusion, stable scans, unrelated files, changed timestamps, same-metadata byte changes, linked entries, cancellation, initial failure recovery, latched changes, lost executable failures, queued/running shutdown, oversized trees and excessive depth. Its inspection mode ran the same portable reader against the actual installation without changing game files: 84 tracked records in 1,472 microseconds. Raw metadata and the capture-tool hash are in `CCS/work/content-monitor/installation-observation.json`. This is host filesystem evidence, not in-game worker timing or an FPS comparison.

Windows path lookup, filesystem behavior, background priority, menu state transitions, native menu/player travel and shutdown still require stopped-game installation and later live checks. The pending full registry, mounted-content, byte identity and cache work remains in the work queue.

References: [Microsoft GetModuleFileNameW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew), [Microsoft background thread priority](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority), [Epic Zen Loader](https://dev.epicgames.com/documentation/en-us/unreal-engine/zen-loader-in-unreal-engine?application_version=5.6), and [CSS startup resolver](../../native/src/startup.hpp).
