# Doubles performance, October 5

The author approved beta.11 and separate runtime/shared-assets ZIPs. Packaging
is held for a repeated summon-creation hitch found during the release check.
No beta.11 tag or final ZIP has been produced.

The installed trial6 core is `9d501a3b6345ea1260403a77937b9ce954827a5cebb08285831a2423f4bd7969`.
Evidence is in `work/genessa-doubles/runtime-trial6/active-performance*.json`.
All captures use the developer core's bounded timer, with no pose probes or
input automation running concurrently. Status snapshots are asynchronous and
cannot attribute an individual timed frame to a precise actor activation.

| Capture | Context | Frames | Astral mean / p95 / max (ms) |
| --- | --- | --- | --- |
| 1 | UG Eve skin, Faithful to Stray, menu visited | 1406 | 0.1206 / 0.1603 / 85.3042 |
| 2 | UG Eve skin, no active doubles | 640 | 0.0133 / 0.0386 / 0.0570 |
| 3 | UG Eve skin, Faithful throughout, menu closed, repeated summons | 1126 | 1.5574 / 0.2795 / 181.0419 |

Capture 3 contains 16 Astral frames above 10 ms. Prepared groups increase from
32 to 51, removed from 30 to 49, while pose rebuilds stay at one. This rules out
form switching or pose rebinding as a necessary trigger. After capture 2,
prepared and removed both equal 28 with zero active groups and no errors.
That verifies group bookkeeping, not total engine allocations or GPU cost.

The next diagnostic separates source capture, adapter preparation, material
copying, visual creation, component registration and release. These fixed-size
counters exist only under CSS_INVENTORY_DEV. They do not change runtime behavior.
Registration is nested inside visual creation; do not sum all timing columns.

Predictions, before testing:

1. Repeated source or adapter capture: those timers should account for the hitch.
2. Material uniform copying: the materials timer should dominate.
3. Skeletal component registration: registration should dominate visual creation.
4. Destroying previous visuals: release should dominate.

[Epic's material API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Materials/UMaterialInstanceDynamic/K2_CopyMaterialI-?application_version=5.5)
warns about the general copy operation. The exact local UE 5.6.1 source shows
our existing `bQuickParametersOnly=true` path calls `CopyMaterialUniformParameters`.
Switching to `CopyInterpParameters` without measuring could lose inherited
parameters, so no material-copy replacement is justified yet.
