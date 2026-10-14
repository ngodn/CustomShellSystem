# Outfit combo interruptions

Twenty stationary Stray cases pass: a native two-hit Axatana axe chain
interrupts Iris Out and Come Closer on all seven Eve variants, both Unholy
Genessa variants and Commander White. Both attack stages, early facial-playback
release and complete restoration are verified. No production code or assets
changed.

Core: `5f04a31900c8e323a27f6ed5650649595015b6dea6600f6682db582dc8fcd619`.
Evidence: `work/three-outfit-rig-physics/combo-interruption-20261015/matrix/`.

## Corrected input timing

The [initial diagnostics](2026-10-15-combo-preflight.md) remain failed, with
their executed recipes and verified restoration retained. A later one-hit
timeline explains why observing B1 alone did not work: the second press fell
inside the game's native input block.

The corrected fixture observes that block, waits for it to clear while the
combo count is one, then presses and releases the actual secondary-attack key.
It never forces an input block, ability, combo count, upgrade or resource.
Native history appends B1_Hold, B1, B2_Hold and B2, and the counter resets to
zero naturally. The response poll is shortened only in the diagnostic child
process so it can read the current input window in time.

Each case temporarily starts at the previously occupied uphill endpoint of the
one-hit timeline, facing toward the original position. That keeps the two-hit
chain within the existing checked staircase corridor. The start evidence must
match the player, original geometry, core and unchanged health. Maximum sampled
distance from the original position is 663.46 cm, below the 750 cm observation
guard. This is a bounded local fixture, not a general terrain or hazard test.

## Results and restoration

`result.json` passes all twenty cases. `verified.json` independently reads
the raw ability-spec replies and combo-history replies, finding 863 active
attack observations across the sequence. These are repeated observations, not
863 distinct attacks. Five negative controls reject continuing playback,
natural clip end, missing native activation, missing B2 and an unobserved
second input.

The second press occurs 1.680 to 1.864 seconds after the first. Stopped clip
playheads range from 0.950 to 1.220 seconds. Those playheads are not input-latency
measurements. Both clips release facial ownership before their natural end.

Every case verifies unchanged health/equipment and restores position/facing,
camera, CSS state, CINE and desktop focus, with no held keys. The parent restores
Peony, `eve_idle_702`, no idle override and all outfit memories. The final
read-only `handoff.json` independently confirms original appearance, exact
saved state/CINE, restored geometry, inactive performance, released input,
counter zero and the same installed core. `metrics.json` records the ranges.

The existing action diagnostics now accept `secondary_combo` with explicit
`--ground-proof` and `--start-evidence` inputs. The verifier also supports
`--check-only`; all twenty archived attack and twenty archived dodge cases
still pass without replacing their historical verification receipts.

## Scope

This closes the recorded two-hit Stray combo-interruption matrix. It does not
claim a three-hit finisher, moving combos, other weapons/forms, damage or visual
cloth/contact acceptance. Interaction, broader lifecycle, performance and the
other stable-release gates remain open. Accepted outfit assets are unchanged.
