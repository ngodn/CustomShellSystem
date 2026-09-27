# CCS takeover review, 27 September 2026

Reviewed Gemini commits `a08d31e` and `d041bc5` and the complete CCS tree against its docs, `extensions/core` (CSSX), `native` (CSS), and the retained UE4SS SDK. The feature commit does not represent a working combat mod. Several mechanics and performance statements in the design spec exceed the evidence.

## Standards findings

Ten concrete findings in the original implementation:

1. Writer flush could deadlock because the worker never notified its waiter. Queue emptiness also omitted the write in progress. Reproduced with a five-second timeout; now waits for completed sequence numbers and reports I/O failure. The queue and message size are bounded.
2. The engine callback captured a destructed loader, was never unregistered, and could call an unloaded core. The new callback retains a shared gate and lifetime token and is unregistered before core destruction.
3. Loader destruction removed widgets through reflection on an unspecified thread. Core destruction now releases CPU resources only. A distinct stop entry handles game-thread cleanup before a future normal unload.
4. Exceptions could escape C ABI entry points. Entries now contain exceptions; tick/hotkey failures are logged once through the background writer, disable normal dispatch and retain game-thread cleanup retries at 250 ms intervals. Loader logging also uses a bounded background writer, rather than opening/flushing a file from callbacks.
5. Rooting called AddToRoot/RemoveFromRoot as UFunctions, swallowed failure and retained raw pointers. SDK SetRootSet/ClearRootSet now manage only roots acquired by CCS, with weak handles. The path-keyed weak cache was replaced with object-array owner guards, following the reference mod's crash findings.
6. Enabled ticks traversed every preset slot and could block on asset loading repeatedly. This work is removed. No combat override is currently applied or reported as successful.
7. Navigation destroyed and recreated the whole page and accessed disk. The prototype remains behind `CCS_EXPERIMENTAL_MENU=OFF`; pooling and asynchronous menu storage remain work items.
8. Preset paths permitted traversal and Windows reserved names. Reads, writes and deletes now validate bounded filename components. Reads are limited to 1 MiB. Parsing checks schema, chain/slot roles, types and string sizes and preserves move origin.
9. Atomic-write fallback could report success after replacement failed and used non-atomic copying. Writes now check stream write/flush/close and use platform atomic replacement without the copy fallback. Settings parsing is transactional.
10. ABI tables lacked size validation and mandatory callback checks. Loader and core now share ABI 1 with sized host/API tables and reject incompatible tables. Staging checks confirmation, game processes and the runtime pin, verifies copies, uses the source VERSION and an atomic selector, and preserves existing presets.

Lower-priority smells remain in the inactive menu, especially duplicated header builders and string action dispatch. No broad cosmetic refactor was made.

## Spec findings

Seven requirements missing or incorrect in the original implementation:

1. No combat hook or routing implementation exists. The attack interceptor returns false. The previous apply function merely loaded assets. Status now explicitly reports combat unavailable.
2. UI had an empty player context, an unreachable initial attachment branch, no F7 open call and no navigation dispatch. The engine helper can now resolve player context from the engine. The prototype is still inactive by default; attachment and input are not claimed as fixed or tested.
3. Enemy compatibility was assumed. Retained montage exports do not prove that all humanoid enemy moves work on the player. Enemy entries are excluded from the generated catalog pending skeleton, track, notify, trace, warping and payload evidence.
4. Slot filtering treated light/heavy and chain stages alike and inferred charged/finisher compatibility from damage thresholds. Catalog slot roles now derive from `selectors.json` combo positions and linked additional/hold abilities. These are original selector roles, not proof of arbitrary cross-weapon compatibility. Sliced attacks remain unimplemented.
5. Tarstone prose contradicted cooked stats. `work/damage-pipeline/other_classes.txt:4864` records Clerik Critical `[0.3,0.35,0.4]`, not guaranteed crit or +50 poise; `:4874` records Tyrant Weak `[4,5,6]`; `:5017` records Zealot Resolve `[1.4,1.7,2.5]`, not 1.5 bars. The catalog preserves raw stat keys and levels plus item display/compatibility data from the source exports. These are not live effect values. Hold-item catalog support still needs extraction.
6. Save/share/text entry, delete confirmation, current-weapon reset, mesh preservation and HUD notifications are absent or unused. They remain queued. A Hadern preset with Tarstones is not a universal vanilla reset.
7. No CCS performance evidence supports the claimed 0.0004 ms attack cost or zero FPS drops. That guarantee is withdrawn from the spec. In-game correctness and frame-time measurements remain required.

## Content and UI approach

`data/catalog.json` contains 107 classified player move records linked to extracted selectors, montage evidence and source hashes. Two additional Scythe attacks remain unclassified rather than being guessed from names. The fixture also retains the four finisher Tarstones' exported stat levels and display data. `work/takeover/generate_catalog.py` produces it deterministically. Runtime initialization no longer loads this snapshot. The experimental menu uses bounded live selector observations; none are combat eligible yet. The default tick does not load content, scan UObjects or decode JSON.

The user's latest [UI screenshot](ui-reference.md) supersedes the earlier sketch. The existing prototype does not yet match it. A native pooled implementation will use real item display data and controller mappings.

## Validation and limits

- Before the writer fix: the standalone regression exited 124 under `timeout 5s`. After the fix: exit 0.
- Before preset validation: the portable test failed with `preset write escaped its directory`. After the fix: runtime tests pass.
- Host CTest covers ordered flush completion, file replacement/failure, preset origin/legacy parsing, malformed schema/slot data, invalid settings atomicity, catalog loading/indexing, retained Tarstone values, catalog regeneration, staging confirmation and live-game refusal, and verified-copy failure.
- Windows DLLs build with the repository's C++23 clang-cl 22.1.8/xwin/MSVC ABI toolchain and pinned SDK. Host tests use GCC 16.2.1; tools run with Python 3.14.7. Toolchain versions were queried on this machine.
- Ten host CTest suites pass normally and under AddressSanitizer/UndefinedBehaviorSanitizer, including 27 Python tests. These sanitizers cover portable CPU code, not engine memory or in-game callbacks.
- The optional menu and all combat paths are unverified in game. The first read-only probe was subsequently staged with stopped-playing confirmation; see [the live result](discovery-probe-01.md). No FPS result is claimed.

Ten standards findings and seven spec findings were identified. The worst standards failure was unsafe callback/unload lifetime; the worst spec failure was absent combat routing. See [work queue](work-queue.md) for the required implementation and validation sequence.

Engine guidance consulted: [Epic's UE 5.6 CPU/memory performance considerations](https://dev.epicgames.com/documentation/en-us/unreal-engine/common-memory-and-cpu-performance-considerations-in-unreal-engine?application_version=5.6), [UObjectBaseUtility AddToRoot](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/CoreUObject/UObjectBaseUtility/AddToRoot), and [UE4SS hook documentation](https://docs.ue4ss.com/dev/lua-api/global-functions/registerhook.html). The retained SDK and tested CSS/CSSX patterns determine the mod ABI and reflection implementation.
