# Outfit attack interruptions

This checks the Phase 4 attack requirement using the actual mapped secondary
attack, with Axatana in axe form. It complements the earlier Faithful aim
interruptions. The current save is Stray with Alteration active; no form,
upgrade, save or resource overrides are used.

Core under test:
`5f04a31900c8e323a27f6ed5650649595015b6dea6600f6682db582dc8fcd619`.
Evidence root: `work/three-outfit-rig-physics/attack-interruption-20261015/`.

## Fixture and restoration

`tools/diagnostics/check_attack_interruption.py` verifies the live input mapping,
starts the requested performance and observes it active at full weight. It
presses and releases the mapped secondary attack, then records active native
Axatana attack specs, montage, performance interruption and released facial
ownership. The test waits for gameplay animation to finish and restores the
recorded player position/facing after the bounded attack displacement. It
preserves camera target/rotation, equipped actor identities, health, CSS state,
CINE and desktop focus. It sends no movement or camera input.

`check_outfit_attack_interruptions.py` runs Iris Out and Come Closer across the
ten release variants. It restores outfit-level and per-variant memories and the
original appearance/idle override after the matrix. The independent verifier
checks actual active attack counts in the raw RPC log, complete case coverage,
early interruption and restoration. It rejects active-performance, natural-end
and absent-native-attack controls.

`peony-irisout-secondary/` passes the initial focused check, including independent
verification and all three negative controls.

## Completed matrix

`matrix/result.json` and `matrix/verified.json` pass all 20 cases: Iris Out and
Come Closer on seven Eve variants, both Unholy Genessa variants and Commander
White. Every case starts the requested performance, observes actual native
Axatana attack activation and releases the performance before its natural end.
The raw ability-spec records contain 40 active-attack observations in total.
The three negative controls are rejected by the independent verifier.

The stopped clip positions range from 2.086 to 2.291 seconds. These are clip
playhead values, not input latency measurements. Secondary-attack displacement
ranges from 181.78 to 183.79 cm before the fixture restores the starting point.
All cases restore position/view, CSS state, CINE, health, released input and
desktop focus. Equipped actor identities remain unchanged during this matrix.

The parent restores the complete saved CSS state and all outfit memories,
including stale saved choices that it never changes. Final live readback
confirms Peony, `eve_idle_702`, no idle override, no held keys, and the same
player/process. No runtime or cooked-asset change was needed.

## Retained first attempt

`peony-irisout-first/` is a failed diagnostic, not a passing regression test.
Primary attack interrupted Iris Out, but changed Axatana from axe to dual blades.
The fixture incorrectly required identical equipment and a montage/last-ability
name containing Axatana. The Stray body montage used a Sicario animation path,
and an input-release ability had already replaced the last-ability getter.

The live mappings confirm primary on left mouse and secondary on right mouse.
Shipped `GA_Player_Attack_Light_Axatana` contains the transformation path, and
the Katana A1 default requests a weapon transformation. The active ability specs
provide a better observation than a montage filename or the last input event.

`position-recovery.json` restores the original player/view after a 191.99 cm
primary-attack displacement, with unchanged health/settings and no held keys.
`restore-axe/result.json` then uses the normal mapped secondary attack to restore
axe form. It observes the native Axe B1 hold/attack abilities, preserves all
non-primary equipment, and restores the original position/view after 186.04 cm.
The final fixture uses secondary attacks so the equipped form remains constant.

## Remaining scope

These checks do not establish attack-combo sequencing, hit damage, parry, dodge,
interaction, visual cloth/contact, Faithful attack coverage or other weapons.
No production code or cooked assets changed for this diagnostic work.
