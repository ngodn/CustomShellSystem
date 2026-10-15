# Asset readiness at the CSS load boundary

The retained reload/rescan crash remains unresolved. This source review found
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
shares that bug. The existing instrumented reproducer and crash gate remain.
Next runtime work must verify normal loading/playback with this candidate and
then address the retained crash trigger; passing host tests alone cannot close it.

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
