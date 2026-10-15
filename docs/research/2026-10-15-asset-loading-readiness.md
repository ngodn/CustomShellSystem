# Asset readiness at the CSS load boundary

The retained reload/rescan crash remains unresolved and is now explicitly
accepted by the user as a nonblocking historical developer-test incident.
This source review found
a separate concrete gap in the same loading path: CSS returned cached or found
objects without checking whether native loading or destruction was in progress.
The new candidate rejects those objects before consumers inspect or bind them.
It is built, tested offline and now installed through the established stationary
reload. It is not claimed to fix the original game crash.

## Evidence and change

The local UE 5.6.1 `UObjectGlobals.cpp`, lines 1407 through 1428, checks
`RF_NeedLoad`, `RF_NeedPostLoad`, `RF_NeedPostLoadSubobjects` and
`RF_WillBeLoaded` before returning a reconciled object. CSS's old `load()`
returned a `StaticFindObject` result or a cache hit without those checks.

Calling the existing blocking soft-reference helper does not alone establish
readiness: `KismetSystemLibrary.cpp:1442` calls `Asset.LoadSynchronous()`, and
`SoftObjectPtr.h:82` returns an already resolved object without invoking TryLoad.
These are the local installed 5.6.1 sources, not assumptions about another engine
version. Their hashes are in `build/asset-readiness-review/candidate.json`.

`asset_loading.inl` now contains the production load function. It checks the
four pending-load flags, destruction flags and unreachable state for cache hits,
find results and blocking-load results. Incomplete new results are not cached.
It does not clear engine flags, force PostLoad, change GC, or write sparse data.

Pending loads use a distinct exception. Facial mesh selection does not record
them as permanent incompatibilities. Shared animation resolution remains
unchecked for retry. Performance setup preserves the exception through cleanup,
and automatic facial idles do not latch a permanent failure for it. Missing
assets and actual incompatibilities keep their existing behavior.

## Verification

The C++23 regression fixture compiles the actual production `load()` function
with fake engine boundaries. It exercises ready cache/find paths, each pending
or destruction flag, unreachable objects, incomplete blocking-load results,
completion followed by retry, and required/optional missing assets.

The old function extracted from commit `2e726d2` fails with
`Cached unfinished/destroying asset escaped the loader`. The candidate passes.
This reproduces the unchecked-return defect, not the original game crash.
Both Windows development and shipping core builds pass, and all 27 host suites
pass. Exact commands/results and candidate hashes are retained under
`build/asset-readiness-review/`. No live injection or gameplay input was used.

The earlier crash dump still lacks the failing class memory. Epic's
[UE-215992](https://issues.unrealengine.com/issue/UE-215992) concerns editor/PIE
child blueprints and excludes standalone, so it is not evidence that this game
shares that bug. Preserve the existing instrumented reproducer as evidence;
the user's later acceptance removes this incident's release gate.
The normal loading/playback check below now passes with this candidate.
The retained crash trigger remains unresolved; these checks do not close it.

## Installed development core

The user explicitly requested installation. The existing stationary reload
procedure activated `css_core-development-1e5d805527c22d4a-asset-readiness.dll`
in Linux process 645797. Loader acknowledgement and process maps verify the
new core is loaded and the prior core is unmapped. Developer mode remains on.
Merrick, original idle and exact saved CSS state are unchanged. No gameplay or
camera input was sent. The previous core remains available for rollback.

The before/after class inventories are not truncated; all observed game
animation classes after activation retain sparse data and clear load/destruction
flags. This checks activation and current bindings, not the intermittent crash.
Receipts: `work/three-outfit-rig-physics/asset-readiness-activation-20261015/`.

## Normal loading and one rescan

The installed core now passes a CW normal-to-facial load, one catalog rescan
and continued Iris Out playback. Three inventories contain 61 animation
instances each, without truncation. Bound game classes retain sparse data and
clear load/destruction flags. The live facial time advances after the rescan;
shared idle, jog and sprint assets remain available with no maintenance or
animation error. Merrick, saved CSS settings, position and camera restore
exactly in process 645797. No gameplay or camera input was sent.

Evidence: `work/three-outfit-rig-physics/asset-readiness-live-20261015/current-walk-path/`.
The parent `check.py` retains the procedure. Two earlier attempts restored
everything but failed an outdated test expectation: CW's accepted walking
correction uses an outfit-local asset, so walk is absent from the shared resolver
status. The second attempt's `loaded-status-timeout.json` establishes this;
it reports available idle/jog/sprint assets and no runtime error. The final
check requires the shared Iris Out binding and inspects all reported shared
entries without requiring an unrelated local walk in that list. Failed receipts
remain in the parent and `settled-status` folders.

This is normal-load and rescan coverage for the changed loader, not a new
motion-acceptance matrix or proof that the intermittent crash is fixed.
