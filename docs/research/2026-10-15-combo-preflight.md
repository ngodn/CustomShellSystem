# Combo diagnostic timing and initial limits

The first two Peony combo attempts remain failed. Neither proves progression to the
second attack stage. Both restore position/facing, view, CSS state, CINE,
health, released input and desktop focus. This is a fixture limitation to
investigate, not an established CSS or game combo defect.

Evidence: `work/three-outfit-rig-physics/combo-interruption-20261015/`.
Core remains `5f04a31900c8e323`, with Stray, Alteration and Axatana axe equipped.
No production code, asset, upgrade or save changed.

The later `peony-queued-two-hit/` check passes actual B1 to B2 progression after
waiting for the native input block to clear. The original failed receipts below
remain unchanged. See the follow-up section for the corrected fixture.

## Native observations

Independent cooked readback of the heavy axe selectors lists B1, B2 and B3,
with separate held and normal classes. The live combo-counter component begins
at count zero. Its debug history records native stage names; a repeated mouse
press alone is therefore insufficient evidence of combo progression.

The first active ability in the earlier attack checks is B1_Hold, followed by
normal B1 on release. The player's mesh montage can use a different animation
path from the ability's authored Axatana montage. Do not infer the current
input window from the authored montage's notifications alone.

`packages.json`, `parents.json`, `hold.json` and `counter.json` retain the
decoded selectors, abilities, montage notifications and combo-counter graph.
Their package lists, commands and extraction logs are alongside them.
`counter-readonly.json` records live metadata and values without mutating them.
`counter-compact.json` retains simplified counter increment/reset bytecode.

## Attempts and restoration

The fixture temporarily faces the player into the previously checked uphill
corridor, with the camera unchanged. It restores the original facing afterward.

- `peony-irisout/` sends two presses, then stops before a third because the
  displacement exceeds the 550 cm next-input limit. The final location remains
  within its separate 800 cm restoration bound. Native history adds B1_Hold
  and B1 only. No stage-two pass is claimed.
- `peony-two-hit/` waits to observe normal B1 after release before its second
  press. The recorded history still lacks B2, so its explicit stage assertion
  fails. No combo acceptance is claimed.

Both retained `recipe.py` files contain exactly the diagnostic versions used.
At that point the experimental combo branch was removed from the reusable
action tool. Failed receipts and
raw requests are retained. `restoration-verified.json` independently checks the
two complete before/after state and geometry comparisons. `handoff.json`
rechecks the live original position/view, idle Peony, no performance, no held
keys and naturally reset combo count zero.

## Follow-up: native input block and a passing two-hit chain

`input-readonly.json` identifies the live Sparta input component and its
`ActiveBlocks` and `InputQueue` fields. Unsupported reflected properties are
recorded as errors rather than inferred from old SDK offsets.

`timeline/` sends one attack and records the input component, combo count,
active abilities and position for five seconds. The native input block appears
at about 0.807 seconds and clears at about 1.576 seconds after the press. The
earlier second presses fell inside that block. Observing normal B1 alone was
insufficient to time the next input. The counter resets naturally around
3.133 seconds. These are observations from this capture, not fixed game timings.

The diagnostic response poll uses 5 ms instead of 100 ms for the combo child
process, preserving the existing request lock and timeout. This permits several
readbacks within the input window; it changes no installed core behavior.

The one-hit timeline finishes at a previously occupied point about 665.5 cm
up the checked staircase. Two attacks continuing uphill would exceed the
prepared corridor. `forward-corridor.json` also rejects an extended downhill
route from the original location: five misses and thirteen penetrating starts.
The corrected fixture instead starts at the recorded uphill endpoint, faces
back toward the original location and stays inside the existing corridor.
Player, geometry, core and health evidence must match before that temporary
relocation. Each case restores the original position and facing afterward.

`peony-queued-two-hit/` observes a native block, waits for it to clear while the
combo count is still one, then sends the second normal mouse press. Native
history adds B1_Hold, B1, B2_Hold and B2. Independent verification finds 45 raw
active-ability observations. Iris Out stops early, facial ownership releases,
the combo counter resets to zero and all state/geometry/focus checks restore.
No input block, combo counter, save or upgrade is forcibly changed.

The proven two-hit branch is now in the reusable diagnostic, with an explicit
`--start-evidence` requirement. The subsequent
[twenty-case matrix](2026-10-15-outfit-combo-interruption.md) also passes all ten
release variants. Neither check establishes a three-hit finisher, moving combos,
other weapons or other forms.
