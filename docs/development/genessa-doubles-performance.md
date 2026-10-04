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

Trial7 isolates the repeated cost to component registration. The saved
`runtime-trial7/creation-timings.json` records 31 preparations and removals,
zero active groups, zero errors and zero pose rebinds. Registration totals
2617.4453 ms over 31 calls (84.43 ms mean, 96.5529 ms maximum); material
creation totals 22.948 ms (0.74 ms mean, 0.9638 ms maximum). Adapter setup
has a 109.339 ms initial maximum but settles to 1.3918 ms on the latest call.
Its initial maximum does not explain the repeated registration hitch.

Next isolate registration's cloth allocation, post-process initialization and
render/physics setup using hidden private components. Keep the source mesh,
player, camera and current appearance untouched; remove each test component.
The installed UE 5.6.1 source calls InitAnim and RecreateClothingActors from
USkeletalMeshComponent::OnRegister. Disabling cloth simulation alone does not
skip clothing actor allocation, which has a separate bAllowClothActors flag.

The hidden-component comparison is saved in
`runtime-trial7/registration-isolation.json` and its request log. With cloth
allocation off, request roundtrips were 239-255 ms; with it on, 324-348 ms.
Post-process enablement did not produce a similar difference. These roundtrips
include request dispatch and polling; the direct registration timer above is
the engine-duration measurement. All eight diagnostic components were destroyed
and the player's owned component list returned to its original value.

The author explicitly rejected removing cloth simulation from doubles. The
candidate retains full cloth on native cached doubles instead. On deactivation,
it hides the owned visuals, stops their ticks, restores native rendering and
material bindings, and releases the copied MIDs. Reuse requires the same live
actor/component/mesh identities, captured structure and settings, and valid pose
sources. Each activation makes fresh materials and resets dynamics/cloth before
resuming. Entries expire after 30 seconds and are discarded on context changes,
removal from the native spawner, disabled options or incompatible state. It
does not extend native actor lifetime. First-time registration and reconstruction
on pose-source changes still allocate cloth; this is not a cold-spawn fix.

The review also found that transient source unavailability could suppress all
future preparation. A missing source now retries after one second; a rejected
source can retry on a later activation after the same cooldown. Failed material
restoration retains its records and originals until cleanup succeeds.

Host tests cover cache identity/expiry and the source retry decision. They do
not execute Unreal cloth, park/resume or destruction. Measure actual cache hits,
registration counts, active and cached counts, idle tick cost, menu/customization
changes, Default controls, and both forms in game before release. The candidate
has not yet been installed or accepted.
