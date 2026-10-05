# Shared performance service candidate

2026-10-06. Native candidate, not installed or accepted in game. This is an
addition to `css.customize` v1 through the existing CSSX service bus. It does
not add CSS behavior to CSSX core. CINE integration is being added in the actual
checkout while preserving the other session's controller changes.

The backend can bind a verified adapter for an installed outfit revision. The
shared catalog names the source outfit, class, skeleton, container-table SHA-256
and bulk size. Package discovery records the verified source revision once;
selection resolves that record without filesystem work in the frame loop.
Loose catalogs, mismatched releases and conflicting adapters are unavailable.
This path is compiled but has not been installed or tested in game.

## Requests

All calls use the existing game-thread service contract. Performance tokens
are transient and must not be saved in a profile, preset or take file.

```json
{"action":"performances"}
{"action":"performance","op":"start","id":"css.eve.dance.blackhole","rate":1.0,"seconds":0.0}
{"action":"performance","op":"status"}
{"action":"performance","op":"pause","token":1,"paused":true}
{"action":"performance","op":"seek","token":1,"seconds":4.0}
{"action":"performance","op":"rate","token":1,"rate":0.6}
{"action":"performance","op":"stop","token":1}
```

Use an ID returned by `performances`. The example selects Black Hole. Start
returns the new token, ID, phase, seconds, duration, rate, pause state and blend
weight. A stale token cannot stop, seek or change the next take. An active take
must be stopped before another starts. The list includes shared sequences and
idles, excludes movement BlendSpaces, and reports outfit compatibility. Each
entry includes its library ID and name for bounded, grouped browsing. Current
gameplay readiness is checked separately at Start; an open menu must not make
all the browser's entries look permanently unsupported.

The service validates a canonical catalog identity, the declared and actual
skeleton, AnimSequence type, non-additive data and disabled root-motion
extraction. Exact-rig assets precede the audited base-rig alternative. It does
not accept arbitrary asset paths or reinterpret an outfit's legacy ID as a
shared performance. Blocking loads happen only on explicit Start, with asset
references retained and player/component/post-process ownership rechecked
after loading.

The world must be grounded, stationary, outside menus/travel/traversal, and
free of an active montage or sidearm use. Movement, binding/form changes,
menus, gameplay interruptions, appearance changes, catalog reload or core stop
end the take. These guards are implemented but still need live acceptance,
including all death/revival and traversal cases.

## CSS browser

Locomotion now includes Dances & Photo Poses, separate from saved idle and gait
choices. It reuses the searchable keyboard/controller picker. Multiple libraries
get a library selector first, keeping each list within the existing search bound.
Missing packs and incompatible outfits have explicit messages.

Play closes the native player menu, waits at least 200 ms and requires gameplay
input to resume. The request expires after five seconds. Player, appearance,
catalog generation, outfit or variant changes cancel it before playback. A
queued menu request cannot compete with a direct CINE Start. Core stop clears
the queue. The pending-selection regression suite has 43 session/resolver checks;
all 21 CSS host suites and the Windows core build pass.

## CINE candidate

CINE uses the same catalog through the existing service bus. Refresh animations
reads compatibility explicitly; take start checks it again. Library/clip, speed
and start time persist, but ownership tokens do not. Off adds no service calls
to ordinary takes. CSS missing or an unavailable saved clip stays visible and
does not prevent choosing Off.

Playback starts when the take rolls. Completion, F9, menu opening, travel,
shutdown and failed setup stop only CINE's owned animation. Gameplay interruption
ends the camera take with a reason. A normal once-only clip can finish before
the camera take. Failed cleanup retains ownership for retry. Another owner's
token is never stopped. Mock-host tests cover these paths, not live engine state.

Full-body performances currently require stationary presets and no look track
or sequence look steps. Route walking, combat and outfit changes would otherwise
fight the pose owner. These combinations are rejected with a reason, not silently
skipped. Existing takes with Performance Off retain their behavior. CINE's music
and camera clocks are independent of animation speed. Live timing, physics and
restoration acceptance remain required before release.

## Clock and graph

The game-thread clock reads `UGameplayStatics::GetWorldDeltaSeconds` and the
player's `CustomTimeDilation`. It does not advance from the loader's unscaled
engine delta. The 5.6.1 installed `GameplayStatics.h` declares the reflected
world delta result as double.

One clock drives both explicit sequence time and a 0.18-second smoothstep
blend weight. Loop wraps, Once eases out at the endpoint, and Pose holds its
last pose. Pause holds time and entry weight; Stop can still finish while
paused. Exit starts from the current entry weight, including a stop during
fade-in. Rates are bounded to 0.1 through 4; invalid values leave clock state
unchanged.

An earlier draft used a separate Unreal Bool Blend timer. That could leave
the graph blending after the clock released its sequence, especially on an
endpoint crossed by a long frame. The candidate now writes the weight to a
Two Way Blend. Disabled/null sequences select zero alpha, without a second
native blend timer. No evaluated bone arrays or node accumulators are written.

The native fields are re-resolved against the current owning instance, with
type and storage bounds checked. The sequence is a reflected strong reference
and also retained by the request owner. A normal stop retains it through the
blend. Immediate interruption sets zero weight before clearing it. Cleanup
only clears the sequence if it is still the one owned by this take.

Binding loads the adapter only on explicit Start. Both directions of its CSS
property layout are checked before changing the component. CSS inputs and
secondary-motion settings are copied into the new instance. The component's
previous post-process override is retained, including a null override, and
rollback is registered before the reflected setter runs. Native animation-layer
assignments are checked/restored using the existing aiming-repair snapshot;
binding rejects an unexpected replacement of the main animation instance.
The original post-process graph and its captured settings return on release.
Cleanup does not overwrite a later owner's mesh or override.

Weapons hidden by a take were previously visible and are tracked by weak
identity. Cleanup restores only those surviving objects. Playback leaves
the main animation class, shared asset data, materials, morphs and saved choices
untouched. Existing locomotion resumes after the take releases
its transient ownership. The selected idle remains live underneath the
performance blend; starting a take does not clear it and then flash the game's
default idle on exit. While a take owns the pose, ordinary walk updates are
suspended and resume on release or gameplay interruption.

## Remaining integration

1. Accept binding/restoration live, particularly native aiming layers, physics,
   and any idle phase discontinuity when the original graph is recreated.
2. Accept CSS browsing/start-after-menu-close and CINE controls in game, including
   keyboard/controller navigation, unavailable assets and cancellation.
3. Test full motion, cloth, contact, gameplay interruption, repeated sessions,
   cold/warm costs and source-graph restoration in the game.

Engine references: Epic's [SequenceEvaluator API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/AnimGraphRuntime/AnimNodes/FAnimNode_SequenceEvaluatorBase?application_version=5.6)
and [SetOverridePostProcessAnimBP](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USkeletalMeshComponent/SetOverridePostProcessAnimBP?application_version=5.6).
The local 5.6.1 `SkeletalMeshComponent.cpp` shows the setter calling
`InitializeAnimScriptInstance()` with the default force-reinit value; do not
substitute a forced reset of the whole native graph.
