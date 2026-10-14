# Combo diagnostic limits

Two Peony combo attempts remain failed. Neither proves progression to the
second attack stage. Both restore position/facing, view, CSS state, CINE,
health, released input and desktop focus. This is a fixture limitation to
investigate, not an established CSS or game combo defect.

Evidence: `work/three-outfit-rig-physics/combo-interruption-20261015/`.
Core remains `5f04a31900c8e323`, with Stray, Alteration and Axatana axe equipped.
No production code, asset, upgrade or save changed.

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
The experimental combo branch was removed from the reusable action tool; the
committed attack/dodge implementation remains unchanged. Failed receipts and
raw requests are retained. `restoration-verified.json` independently checks the
two complete before/after state and geometry comparisons. `handoff.json`
rechecks the live original position/view, idle Peony, no performance, no held
keys and naturally reset combo count zero.

The next attempt needs the current gameplay montage/input-queue timing and
observed stage progression, with a displacement bound that covers the sequence.
Do not widen the bound blindly, force the combo counter, or count repeated B1
activation as a combo. The existing single-attack and dodge results remain
valid within their separate scopes.
