# Outfit dodge interruptions

Twenty stationary Stray dodge cases pass: Iris Out and Come Closer on all seven
Eve variants, both Unholy Genessa variants and Commander White. Actual native
dash activation, early facial-performance interruption and restoration are
verified. No production code or cooked assets changed.

Core: `5f04a31900c8e323a27f6ed5650649595015b6dea6600f6682db582dc8fcd619`.
Evidence: `work/three-outfit-rig-physics/dodge-interruption-20261015/`.

## Native behavior and terrain

Cooked readback identifies `GA_Dash_CorruptedGenessa` and `GA_DashBase` as the
active path. With no movement input, `GetMoveInput` returns `(0, -1)`.
Corrupted Genessa always uses directional dash selection. The backward case
uses the camera-relative direction. The configured distances are 425 cm forward
and 465 cm strafe, with a 0.35-second travel setting. These settings are not a
guarantee of final displacement on terrain.

The [initial preflight](2026-10-15-dodge-preflight.md) sampled a broad grid with
misses. The focused downhill corridor then passed 66 visibility traces, but the
first actual dodge travelled 715.25 cm, beyond the fixture's 500 cm restoration
limit. It interrupted Iris Out and preserved health, but the fixture correctly
remains failed. `peony-irisout/recovery.json` separately verifies restoration to
the recorded position and facing, with unchanged state, health and camera and
released input. The cause of the longer displacement was not isolated.

The extended downhill corridor includes misses and penetrating starts. It was
rejected for repeated testing. The uphill corridor instead passes all 93 traces
through 750 cm, at lateral offsets of -50, 0 and 50 cm. The normal dash input
uses a temporary camera yaw matching the original actor heading. Each case
restores that view, actor position and facing afterward. Traces are visibility
samples, not a general walkability or hazard guarantee.

## Completed checks

`peony-uphill/` passes the focused trial. `matrix/result.json` and
`matrix/verified.json` pass the complete 20-case sequence. Each case:

- Checks the actual SpaceBar mapping, zero movement input, native dash
  parameters, player/core identity and corridor coordinates.
- Starts the requested clip and observes full-weight facial playback.
- Presses and releases SpaceBar before collecting the detailed ability record.
- Observes an active `GA_Dash_CorruptedGenessa` spec and early performance stop,
  followed by released facial ownership and completed gameplay animation.
- Restores CSS state, CINE, health, equipped actor identities, position/facing,
  camera and desktop focus, with no held keys.

The matrix restores Peony, `eve_idle_702`, no idle override and all outfit
memories. Three negative controls reject continuing playback, natural clip end
and missing native activation. Verification reads the raw ability-spec replies;
an input event alone is not a pass.

Observed displacement ranges from 753.39 to 754.50 cm in three dimensions, within
the separately checked uphill area and the 800 cm restoration guard. Input
hold duration ranges from 0.153 to 0.163 seconds. Stopped clip playheads range
from 2.182 to 2.418 seconds; these are not input-latency measurements.

The existing `check_attack_interruption.py`,
`check_outfit_attack_interruptions.py` and
`verify_outfit_attack_interruptions.py` now also accept the dodge action. Their
historical filenames are retained. The verifier still accepts the 20 original
secondary-attack receipts, without rewriting those captures.

## Remaining scope

This closes the recorded stationary Stray dodge-interruption matrix. It does
not establish directional moving dodges, Faithful dodge behavior, damage,
invulnerability timing, visual cloth/contact, or the cause of actual displacement
exceeding the configured dash distance. Combos, interaction, broader lifecycle
and performance gates remain open.
