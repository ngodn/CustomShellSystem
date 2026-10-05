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
was initially uninstalled when that review ran.

Trial8 is now installed and loaded as
`f1334ef040e4a4609ce63b7bacfa941ef6844f5a1b771662dc981ac989970c3b`.
Both Windows builds and the 18 host tests pass. The installed selector, loaded
process mapping and hash match; all 15 protected files remain unchanged.

The Faithful capture `runtime-trial8/reuse-performance1.json` spans 1,284 frames
and 30 seconds, with CSS closed and the same Unholy Genessa Eve appearance.
Five visual groups were constructed overall; reuse ultimately reached 24.
The capture began at three constructed groups and records two registration
spikes, 84.83 and 86.29 ms, in its first 1.3 seconds. Construction then stays at
five. The fixed final 20-second window has 895 frames: Astral mean 0.1576 ms,
p95 0.2701 ms and maximum 4.395 ms. The complete capture mean is 0.3081 ms,
including initial construction. Saved settings are unchanged.

After activity ceased, all five groups expired: prepared=removed=5, active=0,
cached=0, reused=24, error empty. This proves bookkeeping cleanup for that run,
not total engine or GPU memory reclamation. The author accepts Faithful cloth
and movement, then Stray cloth, legs and attacks on this candidate. First-use
cloth allocation remains expensive; do not describe the result as hitch-free
or as a whole-game FPS guarantee.


The Stray-form capture `runtime-trial8/stray-performance1.json` contains 1,265
frames over 30 seconds. Astral mean is 0.2353 ms, p95 0.2024 ms and maximum
97.1213 ms, with two frames above 10 ms (90.0336 and 97.1213 ms). Settings
remain unchanged. Asynchronous counters start at prepared=9/removed=5 and end
at prepared=15/removed=13, while reuse stays at 24. Actor kinds were not captured
per frame; Faithful actors can coexist with Stray, so not every construction
can be attributed to a Stray clone. A later read-only trace caught only one
surviving AstralCopy, not an active Stray clone lifecycle.

The cooked `GA_AstralClones_Action` RemovePrimaryClone/RemoveSecondaryClone paths
call the spawner's RemoveAstralAI, which removes its registry entries and calls
K2_DestroyActor. `GA_AstralGenessa_CopyAnyAttack::OnFadeOutCompleted` also removes its
actor. The cache cannot retain a component owned by a destroyed actor. Keeping
CSS visuals independently across clone lifetimes would require a separate pool,
validated pose-source rebinding and explicit context/expiry cleanup. No actor
lifetime or component ownership changes were attempted during this test.

Release decision: the author accepts beta.11 with the cloth-creation hitch
documented and explicitly defers further optimization to beta.12. Keep full
cloth physics. The authored visuals pass the user's check; there is no claim
of zero bugs, zero FPS loss, or exhaustive travel/death/hardware coverage.
