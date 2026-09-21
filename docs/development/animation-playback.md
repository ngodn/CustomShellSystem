# Animation playback candidate

2026-09-21. Source implementation, not deployed or accepted in the game.
Continues [animation options](animation-options.md). Evidence: `work/anim8/`.

## Ownership repair

The old walk override wrote `ActiveBlendSpace` and `UseActiveBlendspace`, but
release only cleared the flag. It lost an earlier active override. The new
lease captures both fields, retains that baseline across CSS gait changes,
and restores it only while the live pair still matches CSS's last write.
External pointer or flag changes are left alone on release. If CSS reclaims
the override, the latest external pair becomes its new baseline.

Object identity includes the weak handle's serial number. The baseline asset
is rooted while displaced, with CSS clearing only a root flag it added. A
pre-existing root is retained. This is needed because a raw or weak pointer
alone does not keep the displaced asset alive. See Epic's
[object handling documentation](https://dev.epicgames.com/documentation/unreal-engine/unreal-object-handling?application_version=4.27)
for the general ownership rule; the adapter uses the pinned UE4SS SDK's root
and weak-reference APIs already used elsewhere in CSS.

An expired original cannot be reinstalled: release restores a disabled null
override in that case. An original that was valid but disabled retains its
pointer and disabled flag. Readback confirms both writes. Failed restoration
retains cleanup state for retry, and failed hook removal no longer discards
the callback handle or permits the core to unload.

The built-in walk-speed hook now checks the current player's movement
component and game thread before translating a request near 184 cm/s to
85 cm/s. Other actors, threads and requested speeds pass through. No speed
change is made for mod-provided movement. The unused experimental Aristocrat
run override and its shared `AxisToScaleAnimation` mutation were removed.

## Custom movement path

The core resolves Walk, Jog and Sprint options only when the enabled,
currently applied outfit/variant matches the selection, with no appearance
apply/restore pending. Its custom paths go through the same owner as the
built-in idle/walk, so two CSS controllers cannot fight over the pair.
Changing definitions clears the old lease and cached references.

Before installation a custom asset must be an exact 2D `BlendSpace` class,
reference the current mesh's Skeleton object, and expose direction on X
(-180 to 180) and nonnegative speed on Y. Reflected arrays/struct fields are
type- and size-checked. There must be 1-256 samples; each must be a same-
skeleton, non-additive `AnimSequence`, with positive finite playback rate
and root motion disabled. This deliberately does not accept a BlendSpace1D
or an additive aim-offset class as base locomotion. The checks establish
structural compatibility, not valid directional coverage, cadence, contact,
notify behavior or visual quality. Those still need authored asset checks.

Sampled property names follow the pinned UE 5.6.1 `BlendSpace.h` and
`AnimSequence.h`. The native reflected adapter is compiled but has not yet
been exercised on a new cooked package in the installed game.

Current gait intent comes from this player's linked
`ABPL_Locomotion_MotionMatching` instance, resolved each update. Previously
the code found the first global `GA_Walk_C`, which was not tied to the player.
The linked-layer method signature and `IsWalking`/`IsSprinting` fields were
enumerated in `work/grip-grounding-v1/post-process-hand-isolation/`.
Custom movement declines to activate with missing or contradictory flags.
Stationary velocity selects idle even with Walk toggled; jog/sprint during
acceleration follow the flags rather than nominal full-speed thresholds.
The legacy option retains a speed fallback if the linked layer is absent.

Each active update re-resolves the mesh's main animation instance. Pawn,
animation-instance or mesh changes release the old override, and asset loads
are followed by identity checks before any write. Missing player ownership,
airborne movement, ignored move/look input, an open game menu or an active
montage releases CSS's override. These are conservative current guards, not
proof that all combat and travel transitions are covered.

`runtime/status.json` includes `animation.engaged`, `gait` and `error` for
the native controller. Asset errors also reach the normal CSS message area.
The new runtime does not replace game montages, alter event timing or hide
weapons yet.

## Validation and remaining work

Portable tests cover original enabled/disabled/null pairs, repeated claim,
CSS gait changes, external ownership changes, flag-only changes, recycled
object generations, independent instances, player/thread speed filtering,
invalid speeds, missing gait flags and acceleration classification. These
tests exercise the policy used by the native adapter, not a simulated claim
of in-game success. The focused suites pass (100 data, 119 animation-choice and 57 runtime-policy
checks), and the final Windows core build exits 0. Command results are
recorded alongside each log in `work/anim8`.

Required before deployment/acceptance:

- Confirm linked-layer gait flags keep updating while the custom override is
  active. If the graph stops evaluating that layer, move intent collection
  to a verified player-owned source. Static field existence does not prove
  freshness during override playback.
- Author and cook complete directional/speed BlendSpaces, then exercise the
  reflected validator and test Default, outfit/profile changes and recovery.
- Measure update latency and frame cost. Current maintenance remains 250 ms;
  fast gait changes and combat release need a measured, appropriate update
  point. Do not claim that this establishes pre-combat weapon restoration.
- Verify attack/parry/aiming, damage, airborne transitions and beacon travel.
  A montage guard alone does not cover every aiming or ability state.
- Finish custom idle carriers, per-weapon idle selection, reversible weapon
  hiding and the beacon pair while preserving game events.
- Visually review skin/weapon/ground contacts, in-world movement and menu
  controls. Preserve V44 binding, proportions, hair 200/24/0, accepted body
  physics, the -3 cm grounding offset and disabled CSSX.

No new asset or core was deployed at this checkpoint. DLL hot reload remains
avoided because of the earlier animation-worker crash; this ownership repair
does not establish that hot reload is now safe.
