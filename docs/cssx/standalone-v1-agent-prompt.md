# CSSX v1.0.0 standalone framework and Cheat Menu handoff

You are taking ownership of CSSX v1.0.0 and CSSX Cheat Menu v1.0.0 for Mortal
Shell II. Implement, test, document and deliver both products. This is an
execution task, not a request for an architecture proposal alone.

## Required outcome

CSSX is a standalone UE4SS mod and extension platform. It installs, starts,
renders its own UI, loads extensions and manages settings without CSS being
installed. CSSX Cheat Menu is a separate extension/download that requires only
CSSX v1.0.0 and the pinned UE4SS runtime. Both must also work alongside the
unchanged published CSS v1.0.0-alpha.1 and Eve Black Pearl sample.

Deeply identify and fix the FPS regression, including the reported slowdown
that predates the per-frame HUD and ABI 2. Redesign the shared UI, its layout,
interaction and extension authoring contract. You may invent a better UI and
new standards. Preserve useful capabilities rather than hiding or deleting
features to obtain a favorable benchmark.

The target is correct, maintainable C++ with no measurable gameplay performance
regression attributable to idle framework work, and no recurring stutter,
crashes, lost input, corrupted settings or leaked gameplay effects. Active
features must have measured, justified costs. Never claim literally zero CPU
cost or universal freedom from bugs from a narrow test. Release only after the
required regression, coexistence and gameplay checks pass.

## Workspace, ownership and toolchain

Repository: `/home/eins0fx/development/mods/msII/CustomShellSystem`

Canonical new project root:
`/home/eins0fx/development/mods/msII/CustomShellSystem/extensions/core`

Keep new framework source, the v1 Cheat Menu port, tests, build scripts and
assets inside that project root. Choose a clear internal layout there. Keep
builds, scratch work, logs, traces, recordings, caches, backups and release
artifacts in short readable subdirectories such as `work/`, `build/`, `dist/`
and `docs/` under that root. Use `docs/cssx/` for the shared handoff and durable
cross-project summaries. The existing `extensions/cheat-menu/` is a legacy
implementation/reference, not a requirement to keep the old host dependency.

Use workspace-local temporary directories rather than `/tmp`. Configure tools'
temporary/cache/output roots explicitly; sandbox or redirect tools that insist
on `/tmp`. Preserve Windows path compatibility with short deterministic asset
and folder names, without long UUID or timestamp chains. New UE content uses
`/Game/CSSX/`; do not introduce CSS authoring paths as a runtime dependency.

Another agent is developing CSS/Eve in this repository. Keep edits and commits
scoped to your ownership. Do not switch/reset the shared checkout, stage the
whole repository, rewrite its release tags, or modify unrelated work. If an
isolated worktree is useful, keep it under your workspace and integrate the
finished changes back to the canonical project deliberately. Read current Git
status before editing. Untracked `catalog/`, the non-CSS guide and Eve authoring
work are not yours to clean up. Commit each coherent milestone separately.

Target the same UE4SS build used by the published CSS alpha, not whichever
upstream build happens to be newest:

- Game: Mortal Shell II, UE 5.6.1, Windows x64 under Steam/Proton here.
- Runtime: UE4SS MS2 NO AOB, `3.0.1-1111-g97b7e501`, GameShippingWin64.
- UE4SS.dll SHA-256:
  `fb1839ee91f71f83d508d44a2763a15ac1bb0c5fb4e504ac0fcfca64376a054a`.
- Existing compatible headers: `d7e7826d415b0332b43439a64e6c87f64019be03`.
- Authoritative pin: `native/ue4ss-runtime.json`.
- Reference SDK: `reference/ue4ss-sdk-d7e7826d/`.
- Import library: `reference/ue4ss-runtime-97b7e501/UE4SS.lib`.

The established native toolchain uses C++23 and clang-cl/MSVC ABI. Confirm it
from the project and query the installed version manager before implementing.
Reuse verified build inputs without depending on CSS being installed or loaded.
If headers must change, demonstrate ABI compatibility against this exact runtime
before deployment. Do not replace the user's UE4SS installation to mask a bug.

## Read the evidence before redesigning

Read these repository files first, then follow specific evidence paths as needed:

- `docs/development/cssx-fps-investigation.md`: measurements, disproved claims,
  the HUD defect, unresolved older regression and the animation reload crash.
- `docs/development/dll-reload.md`: actual lifecycle limits.
- `docs/development/alpha1-delivery.md`: current working CSS/Eve release baseline.
- `docs/extensions/architecture.md`, `host-api.md`, `native.md` and `ui-kit.md`:
  legacy framework features and contracts. These describe the old design, not
  the required new standalone architecture.
- `native/src/extension_runtime.cpp`, `extension_engine.inl`,
  `extension_hooks.inl`, `extension_hud.inl`, `extension_view.inl`,
  `hook_host.cpp` and `native/include/cssx/api.h`: trace the real host coupling.
- `native/src/core.cpp`, `native/src/loader.cpp`: CSS startup and ownership.
- `extensions/cheat-menu/`, existing extension tests, and
  `tools/cssx_release.py`: feature inventory, lifecycle and packaging references.
  Audit their temporary-path and host assumptions before reusing them.

Current facts to preserve:

1. Old CSSX installs inside `CustomShellSystem` and relies on CSS for host/UI
   services. Separate download files did not make it standalone.
2. A HUD preparation path doing unnecessary engine object lookup was reproduced
   and fixed by skipping HUD work when no extension requests frames. That fix
   does not close the earlier ABI-1/foundational performance report.
3. Older measurements include roughly 28 engine Hz with CSSX and 70-77 Hz with
   CSSX disabled. A later 49-52 Hz run had a changed camera/view, so it is not a
   controlled estimate of the remaining penalty. Engine cadence is not GPU
   presentation timing. Read the original captures before quoting conclusions.
4. A user reported about 22 FPS and 35% GPU usage, recovering after removal.
   Low GPU utilization is an observation, not proof of one specific bottleneck.
5. CSSX is currently disabled in the user's working CSS alpha installation.
   Preserve that baseline until a coordinated test deployment.

## Rewrite decision

A clean standalone host/runtime/UI rewrite is authorized and may be the best
way to remove the CSS dependency. First capture the current behavior and
regression, then document which boundaries require replacement and why.
Preserve validated cheat semantics, asset knowledge, lifecycle protections and
regression fixtures where they remain useful. Do not blindly transplant old
hooks or discard the evidence needed to explain the original slowdown.

Choose the rewrite scope using evidence. Build an independently loadable small
core first, then add services, UI and Cheat Menu incrementally. Record frame
cost after each layer so the offending behavior cannot disappear into a large
untested rewrite. A new folder, renamed DLL or successful compilation is not
proof that the foundational problem has been fixed.

## Milestone 1: isolate the performance regression

Inventory the exact loaded DLLs, versions, hashes, hooks and enabled extensions.
Reproduce the issue using known compatible archived binaries when available;
never mix incompatible ABI versions just to recreate an old label. Instrument
outside as well as inside CSSX callbacks, since globally installed hooks can
cost time even with no extension rules active.

Use repeated paired captures with the same save, scene, camera, input sequence,
settings, frame cap, warm-up and background workload. Record at least three
valid A/B pairs for a reproducible case, retain raw samples, and measure natural
run-to-run variation before judging parity. Separate cold-start/loading costs
from steady-state play. Do not improve the result by changing graphics settings,
resolution, gameplay content or the baseline's frame cap.

Compare these configurations with normal restarts as needed:

| Configuration | What it isolates |
| --- | --- |
| Game + pinned UE4SS | Loader baseline |
| Above + CSS alpha/Eve, CSSX absent | Working coexistence baseline |
| Standalone CSSX, no extensions, UI never opened | Core and global hook cost |
| CSSX + Cheat Menu, cheats off, UI closed | Extension registration/polling cost |
| CSSX menu open, navigating, and closed again | Widget, input and cleanup cost |
| Selected cheats/HUD features enabled | Real feature cost and restoration |
| CSS alpha/Eve + new CSSX + Cheat Menu | Combined behavior and contention |

Record frame-time distributions, median, p95/p99, hitches, throughput and CPU/GPU
utilization where available. Label the measurement source accurately. Include
allocations, locks, object scans, reflection calls, file/JSON work, logging and
hook dispatch when investigating a hotspot. Use bounded instrumentation and
measure its own overhead. Use Unreal/Slate Insights if this shipped build
supports them; use a documented external/native fallback when it does not.

Before evaluating a candidate, state the noise floor and acceptance budget in
milliseconds and relative frame time. Closed-menu/unused CSSX must show no
repeatable degradation beyond the baseline's noise, and enabled features must
stay within their declared budget without spikes. An inconclusive comparison
is not a pass. Document the cause, intervention, before/after evidence and a
regression check. Keep the historical ABI-1 question open if it remains unproven.

## Milestone 2: standalone runtime and CSS coexistence

Provide a distinct UE4SS mod installation root, activation marker, loader,
versioned ABI, settings, logs and extension directory. CSSX must own its engine
access, scheduling, UI and input lifecycle. It must work with CSS completely
absent, including no CSS DLLs, state, catalog, artwork or request bridge.

Design explicit extension ownership, bounded requests, deterministic teardown,
version negotiation and migration. Separate product version 1.0.0 from ABI and
manifest schema versions. Either provide a tested legacy adapter or clearly
reject incompatible extensions without executing them. Do not advertise native
DLL plugins as sandboxed or promise recovery from arbitrary memory corruption.

The published CSS alpha remains unmodified during compatibility testing. It
still contains legacy CSSX support. Its startup checks for
`CustomShellSystem/cssx.json` or `CustomShellSystem/cores/cssx_core.dll` before
starting that host. Merely placing the new framework elsewhere does not disable
an already-active legacy host.

Implement a reversible migration and duplicate-host policy. Back up and retire
only confirmed legacy CSSX activation files during a normal stopped-game
installation; preserve CSS core.json, loader/core DLLs, wardrobe settings,
profiles, assets and all unrelated mods. Verify mapped modules and runtime
ownership after restart, not just files on disk. Safely detect mixed old/new
installations, both load orders, stale selectors and repeated startup. Exactly
one CSSX host may own an extension or its hooks. If a conflicting active legacy
host cannot be neutralized safely, leave it untouched and fail the new instance
with a clear actionable diagnosis. Never unload another host's live DLL blindly.

The new UI must not steal CSS navigation, Y/I lighting, Back, mouse input,
controller focus or game actions. Establish exclusive focus/input ownership,
restore the prior mode on every close/cancel/travel/destruction path, and test
both menu orders. CSS must still work if CSSX is disabled, removed or fails.

## Milestone 3: rebuild the shared UI and UX

Audit actual screens and interactions, then implement a coherent reusable UI
system. You may replace the old structure, navigation, visuals and schema.
Match Mortal Shell's readability and visual context while fixing the old poor
spacing, hierarchy, sizing and behavior. Product screens should show useful
player information, not development implementation details.

Cover long and short labels, many extensions, empty/loading/error states,
search/filter, scrollable lists and option pickers, selection visibility,
contextual help, confirmation and editable values. Place control hints according
to their context in consistent reserved areas; status/error text must not
collide with controls. Every action needs correct mouse, keyboard and controller
behavior, including held buttons, repeat, focus changes and Back/cancel. Keep
unavailable actions explainable and prevent input reaching obscured layers.

Validate the intended resolution/aspect-ratio and UI-scale range, menu
exit/reentry, travel, pause, and controller/keyboard switching. Use retained
widgets and change-driven updates where supported; avoid rebuilding widget trees,
searching the object registry or formatting/logging unchanged values each frame.
Define when rendering/ticking is actually needed, with no hidden UI work after
closing. Publish the components, schema, extension example and authoring guide.
The Cheat Menu must consume this shared system rather than ship a second UI kit.

## Milestone 4: CSSX Cheat Menu v1.0.0

Inventory the legacy menu's actions and implement the supported feature set on
the new standalone contract. Document intentional replacements and unavailable
features; do not silently drop difficult cheats. Correct wrong labels, grouping,
feedback, focus and apply/reset behavior as part of the redesign.

Preserve safe default-off behavior, draft/Apply/Discard semantics where relevant,
clear active-effect status and Disable all. Capture and restore original game
values rather than assuming defaults. Distinguish reversible toggles from
persistent inventory/progression changes and irreversible actions in the UI.
Never run a cheat because its page was opened, because CSS selected an outfit,
or because an extension loaded. Restore owned hooks/effects on disable and on
supported lifecycle transitions without overwriting unrelated game/mod changes.

Test new player pawns, death/respawn, beacon travel, shell changes, load/save,
combat, damage, parry, sidearm aiming and menu exit. The old symptom of enemies
hitting through an apparently invulnerable character must be checked with all
cheats off. Record actual actor/property ownership, not only menu toggle values.

## Native engineering requirements

Use the pinned UE/UE4SS source and verified reflection signatures for layout,
thread and callback assumptions. UObject access and mutations belong on the
appropriate engine thread. Track weak object identity across GC, level changes
and pawn replacement; establish explicit strong ownership when it is necessary.
Use RAII and bounded resources. Keep C ABI ownership and exception boundaries
clear. Remove callbacks and finish queued work before freeing their code/data;
failed cleanup must prevent unsafe unloading. Use normal game restarts until
safe reload has been independently demonstrated for this exact implementation.

Load lazily, cache with explicit invalidation and schedule work on demand. Avoid
unbounded per-frame scans, shared ProcessEvent interception without measured
need, frequent filesystem polling, busy loops and verbose steady-state logging.
Identify concurrency/reentrancy risks and test the failure paths. Do not
introduce a game-save dependency or alter CSS's accepted rig, motion or assets.

## Verification, documentation and delivery

Coordinate a live test window with the user and the active CSS agent before
changing the shared game installation or sending controls. Reuse existing
session authorization where applicable; do not repeatedly ask about routine
builds. Keep screenshots/recordings and all backups in your workspace. Prefer
Steam screenshots without changing focus; for motion, record only the game
window while the user plays. A menu still is not movement or performance proof.

Run meaningful host tests, Windows builds and exact-binary in-game checks. Include
fresh install, legacy migration, upgrade, removal, missing/malformed extensions,
load-order permutations, repeated open/close and a sustained play/leak check.
Verify CSS alpha/Eve alone still works after CSSX removal. Preserve saves and
personal settings through test restoration. Read each process's exit status;
a log line or a request timeout alone does not prove completion or a crash.

Maintain concise durable files under `extensions/core/docs/`: decisions and
rejected alternatives, performance investigation/raw-evidence index, UI/input
standard, ABI/schema and migration contract, integration tests and work queue.
Record confirmed fixes so another agent does not repeat failed experiments.
Keep hypotheses separate from proven causes. Cite primary sources and pin
version-sensitive details. Commit coherent milestones, not unrelated files.

Deliver separate clean install-ready ZIPs for CSSX v1.0.0 and CSSX Cheat Menu
v1.0.0, with versioned source, installation/upgrade/removal instructions, release
notes, dependency declarations and SHA-256 checksums. Package no UE4SS binary,
CSS runtime, personal state, logs, scratch files or debug tooling. Build from a
clean tagged source tree, unpack and validate both archives, then test their
exact DLLs in game. Public upload must follow the user's release authorization;
preparing a release locally is not itself proof of publication.

Completion requires all requested features, a demonstrated regression fix,
standalone operation, unchanged-CSS coexistence, visually reviewed UI, verified
Cheat Menu behavior, documented limitations and reproducible release artifacts.
Do not stop at a rewrite proposal, compilation, one FPS counter screenshot, a
core-only benchmark, or UI mockups. Keep working through concrete milestones.

## Primary research starting points

Read relevant upstream documentation, but verify API availability against the
local UE 5.6.1 source and the exact UE4SS revision above. Current documentation
may describe a newer build.

- [UE4SS C++ API](https://docs.ue4ss.com/dev/cpp-api.html)
- [UE4SS source](https://github.com/UE4SS-RE/RE-UE4SS)
- [Epic UMG optimization](https://dev.epicgames.com/documentation/unreal-engine/optimization-guidelines-for-umg-in-unreal-engine)
- [Epic Slate Insights](https://dev.epicgames.com/documentation/unreal-engine/slate-insights-in-unreal-engine)
- [CSS alpha baseline release](https://github.com/ngodn/CustomShellSystem/releases/tag/v1.0.0-alpha.1)
