# Sidearm re-equip visibility regression

Evidence directory:
`work/three-outfit-rig-physics/sidearm-equip-regression-20261015/`.

The installed development core is
`css_core-development-dc0a609e74750815-boss-permanent.dll`. This check follows
a normal restart and a slot 1/2/1 load sequence. It uses only sidearms already
unlocked on the current save and retains the user's appearance and visibility
settings. No firing input is sent.

## Native equipment path

The shipped `BPFL_Player_Equipment.ChangeSidearm` graph activates the selected
item through the controller, broadcasts the equipment event, and updates the
runtime save object's active sidearm. Its `Save` argument controls the immediate
`SaveGame` call, not the runtime save-object update. The diagnostic uses
`Save=false` and restores the original item through the same function. This is
not a guarantee that no normal game save occurs during testing.

`equipment-decoded.json` retains the source graph. Live signatures and unlocks
are in `equipment-signatures.jsonl`. No unlock function or save-file edit is used.

## First sequence and recovery

Trebuchaxe, Crossbow and Parasite Gun each pass seven states: stowed in-use,
drawn in-use, drawn Hidden, drawn in-use again, restowed, Default, and restored
in-use. Trebuchaxe's `SM_Projectile_Preview` remains hidden and unrendered in
every state, including the freshly spawned weapon. This extends the previous
same-instance check to a new equip after restart and save loading.

The sequence stops at Cursed Child because the fixture incorrectly requires
unchanged Resolve while aiming. The shipped `GA_CursedChildLightHandler` has
`EnergyCost=0.7`, `DmgTickRate=0.2`, and calls `ConsumeResolve` in its light-area
update. Its passive damage light is native weapon behavior. The failed receipt
is retained; it is not evidence of a CSS visibility failure.

The original Naylshotte, CSS state, unlock state, player/view geometry and
released keys restore. Its Bleed asset, tags and active state also match. The
first fixture compares transient object handles and incorrectly reports an
infusion mismatch when the asset receives a different handle after re-equip.
The follow-up compares the effective asset path, tags and active state.

Cursed Child's first aim consumed 5.5999755859375 Resolve. `recovery.json` and
the raw recovery requests verify its restoration to 100, with Health still 72.
The initial `AddResolve` call used multiplier 1 and restored one tenth of the
requested amount. The inspected controller `S_GainResolve` uses multiplier 10
under the active energy system; using that native function restores the exact
remainder. Both attempts are retained. No raw attribute or save-field write was
used. Subsequent Cursed Child checks explicitly account for native aim cost and
restore it through `S_GainResolve`.

The corrected continuation runs only the remaining five sidearms. Its receipt
is separate under `remaining/`; the first three passing cases are not repeated.

## Continuation and input stop

Cursed Child and Ballistazooka pass all seven visibility phases. Cursed Child's
longer aim sequence consumes 58.799869537353516 Resolve through the native light
ability; the diagnostic restores that exact test cost before the next case.

Before Machine Gun's aim input, control rotation changes from yaw
-99.32099914550783/pitch 0 to yaw -164.84097290039062/pitch 4.199999809265137.
The input guard stops the sequence. Naylshotte, its Bleed asset/tags/active state,
CSS settings, unlock state and Health/Resolve restore, with all sampled keys
released and world position unchanged. The newer camera angle is deliberately
left intact. No cause is assigned to that external view change.

`verification.json` independently checks 35 visibility phases across the five
completed weapons and pins the source, recipes and raw receipts. Machine Gun,
Lute and the final Naylshotte sequence were pending at that checkpoint; the
completion below supersedes that pending list. The prior separate
Naylshotte Burn and Bleed draw/stow results remain valid. Neither interrupted
whole-run receipt is relabeled as a complete pass.

## Final three and completed visibility coverage

After renewed idle-game authorization, Machine Gun, Lute and Naylshotte each
pass the same seven phases. All eight sidearms now have passing cases, totaling
56 visibility observations across the retained sequences. No further game input
is needed for this mode regression. No shots were fired in these sequences.

The final receipt confirms restored Naylshotte, Bleed infusion, CSS settings,
unlocks, Health 72 and Resolve 100, with released keys. Position, camera and
view target match exactly. Restoring the actor angle introduces only an
8.526512829121202e-14 degree Euler rounding difference. The fixture's exact
JSON equality incorrectly makes `geometry_restored` and the aggregate `passed`
false. The raw receipt remains unchanged. `final-three/verify.py` independently
checks all 56 phases and restoration, allowing at most 1e-9 degree actor-angle
rounding while requiring exact position, camera and view identity. Its passing
`final-three/verification.json` pins the recipes and raw evidence.

This closes the other-sidearm visibility-mode regression. It does not establish
Tarstone replacement, Barrage or damage behavior, which remain separate checks.
