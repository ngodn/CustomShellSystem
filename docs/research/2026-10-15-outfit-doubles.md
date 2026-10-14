# Doubles across the release outfits

Six older Eve outfits pass a normal Faithful summon with Use CSS on the
installed `d7cb0635d112f5fa` core. Each has three active observations, the
expected source mesh, 24 finite visual bone transforms and complete CSS group
cleanup. These are binding and lifecycle checks, not visual cloth/contact
acceptance. Peony's separate both-form checks remain in
[its evidence record](2026-10-15-peony-doubles.md).

| Outfit | Evidence directory | Result |
| --- | --- | --- |
| Black Pearl | `outfit-doubles-20261015/eins0fx.seduxtress-black_pearl` | Pass |
| Planet Diving Suit | `outfit-doubles-20261015/eins0fx.seduxtress-prototype` | Pass |
| Skin Suit | `outfit-doubles-20261015/eins0fx.seduxtress-skin_suit` | Pass |
| Bikini | `outfit-doubles-20261015/eins0fx.seduxtress-bikini` | Pass |
| Casual Knitwear | `outfit-doubles-20261015-2/eins0fx.seduxtress-casual_sweater` | Pass |
| Midsummer Alice | `outfit-doubles-20261015-2/eins0fx.seduxtress-midsummer_alice` | Pass |

Paths are under `work/three-outfit-rig-physics/`. Each passing child preserves
saved settings, CINE, player identity and view. Its created/removed group counts
increase by one, with zero active/cached groups and no fallback at completion.
All six cases were independently checked against their raw samples by
`verify_appearance`; the second directory holds `verification.json`.

## Interrupted parent checks

Both parent runs remain failures. The first stops on camera movement during
Casual Knitwear; the second reuses only the four independently verified cases,
adds the remaining two Eve passes, then stops on camera movement during Unholy
Genessa's Eve variant. Neither run proves the complete nine-case matrix.
The two Genessa variants and Commander White still need a completed Faithful
check. The failed Genessa capture records one created and removed CSS group,
but its view guard fails, so it is not counted as a passing case.

The first separate recovery restores Peony and its idle without overwriting
the newer camera view. The second interruption crosses the user's save change;
its temporary appearance/idle and delayed persistence required explicit recovery.
The native customization service delays saves for 30 minutes on `persist:false`.
Consequently, a later doubles-mode command can change the live mode while its
disk acknowledgement remains stale. This is the cause of the subsequent
Default-mode fixture timeout, not evidence of a failed native summon.

The diagnostic now separates appearance ownership from camera restoration:
a newer camera view stops gameplay and prevents repositioning, while an unchanged
owned appearance can still be restored. This new cleanup branch is compiled but
has not yet been exercised live. Preserve the failed receipts rather than
rewriting them as successes.

## Upgraded-save secondary clone

The user's upgraded save resolves a new pawn and controller. Its owned
`GA_AstralClones_Action.HasSecondaryClone` returns true in actual Stray form.
Evidence is under `doubles-secondary-20261015/`.

`ug-css/stray-css` observes both `AstralClonePrimary` and
`AstralCloneSecondary`, six active observations each. Both contain the captured
Unholy Genessa source mesh. Backward input (`S`) and camera movement interrupt
the fixture, so its parent and child remain failed. No position restoration is
attempted over the user's new position. A separately recorded passive follow-up
finds zero active/cached groups, prepared/removed totals both 16, no fallback or
error, and unchanged saved settings. The prior totals were 14, consistent with
two cleaned-up groups. This does not replace the interrupted full check.

The verifier can now explicitly require both Stray actor kinds and two CSS
groups. Existing primary-only captures fail that requirement in both Default
and Use CSS, while the original four-case primary-only verification still passes.
No progression, save data or game assets were changed by these captures.

## Restoration and CW secondary-clone follow-up

The follow-up `cw-css-revival-core/stray-css` passes on installed core
`4a4402a210f4d1e8`. Both primary and secondary receive the captured CW mesh:
54 active actor observations and 432 finite visual bone transforms pass the
independent verifier. Two CSS groups are prepared and removed, with zero
remaining active/cached groups and no fallback/error. Child and parent pass;
full saved state, CINE, appearance, original Iris Out idle selection and
player/view geometry restore. This closes the interrupted CSS-copy cleanup
case below, not visual cloth/contact or other outfit/form coverage.

`restore-via-mirror.json` verifies full saved-state restoration and unchanged
player/view geometry. Existing CSS mirroring and normal wear controls restore
both saved form choices and remembered appearances without a native form switch.
The attempted Faithful restoration exposed Alteration's native revival blocker;
[that diagnosis and separate core change](../development/genessa-form-save-switch.md)
do not count as a doubles pass.

`cw-modes-focused/stray-default` independently passes with both Stray clones:
48 active actor observations, 384 finite bone transforms, native mesh selection,
zero CSS groups and clean expiration. CW is the player's selected appearance;
Default clones intentionally use the native mesh. This capture uses the older
`d7cb0635d112f5fa` core.

The following Use CSS child stops before input because the game loses focus.
The camera also changes, so the parent declines to reposition the character.
`idle-restoration.json` separately verifies full saved-state restoration and
preservation of that newer position/view. The parent remains failed and CSS
secondary-copy acceptance remains open.

Two earlier attempts are retained: `cw-modes` stops before summon because the
saved Iris Out idle is playing; `cw-modes-idle-original` pauses that idle but
stops before input because the game lacks focus. Both restore the full state.
The wrapper now temporarily pauses automatic idles and separately restores its
owned idle choice even when geometry restoration stops. The latter interrupted
cleanup path is compiled but not yet exercised as an integrated live run.
