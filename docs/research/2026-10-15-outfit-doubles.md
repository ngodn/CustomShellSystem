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
preservation of that newer position/view. That parent remains failed; the later
`cw-css-revival-core` run above closes its CSS secondary-copy check.

Two earlier attempts are retained: `cw-modes` stops before summon because the
saved Iris Out idle is playing; `cw-modes-idle-original` pauses that idle but
stops before input because the game lacks focus. Both restore the full state.
The wrapper now temporarily pauses automatic idles and separately restores its
owned idle choice even when geometry restoration stops. The latter interrupted
cleanup path is compiled but not yet exercised as an integrated live run.

## Peony and both Genessa variants: completed secondary-clone checks

After the user restored game focus, three sequential Use CSS checks passed on
core `4a4402a210f4d1e8`. Evidence directories below are relative to
`doubles-secondary-20261015/`:

| Appearance | Evidence | Active actor observations | Finite visual bone transforms |
| --- | --- | --- | --- |
| Peony V2 | `peony/summon/stray-css` | 48 | 384 |
| Unholy Genessa, Eve | `ug-eve/summon/stray-css` | 46 | 368 |
| Unholy Genessa, Genessa | `ug-genessa/summon/stray-css` | 46 | 368 |

Every case observes both `AstralClonePrimary` and `AstralCloneSecondary`, with
one copied component matching the captured source mesh on each actor. The
normal summon input is observed and released. Each case creates and removes
two CSS groups; active/cached groups return to zero without fallback or error.
Health is preserved. These counts aggregate observations over each clone's
lifetime, not distinct actors or rendered frames.

The new `tools/diagnostics/check_stray_outfit_doubles.py` wrapper uses each
variant's existing saved customization. It restores outfit-level memories,
Commander White, the original idle choice, full saved CSS state, CINE and exact
player/view geometry after each case. An independent raw-sample verification
also compares all three starting and ending state files and geometry values;
they match across the entire sequence. Its receipt is
`three-outfit-secondary-verification.json`. Each parent and child passes, with
the recipe, requests and before/after state retained alongside its result.

The earlier unfocused launch stopped before creating evidence or changing an
appearance. Python 3.14.7 compilation passes. No core or cooked assets changed
for these three checks. The wrapper's normal restoration is now verified live;
interrupted restoration remains a separate case.

This closes secondary mesh-copy and cleanup coverage for Peony and both UG
variants, alongside the earlier CW pass. Visual cloth/contact, secondary ghost
material parameter coverage, other customization combinations and the missing Faithful UG/CW cases remain
open. The six older Eve secondary cases are closed by the follow-up below. No native
form override or save-file edit was used.


## Six older Eve secondary-clone checks

All six older Eve variants now pass both Stray clones through normal ability
input on core SHA-256 `4a4402a210f4d1e8c706726348237908be80472ef7df129b886ee657e48a5857`.
Evidence is under `doubles-secondary-20261015/older-eve/`:

| Variant | Case folder | Actor observations | Finite visual bone transforms |
| --- | --- | --- | --- |
| Black Pearl | `eve-black_pearl` | 48 | 384 |
| Planet Diving Suit | `eve-prototype` | 46 | 368 |
| Skin Suit | `eve-skin_suit` | 46 | 368 |
| Bikini | `serialized/eve-bikini` | 48 | 384 |
| Casual Knitwear | `serialized/eve-casual_sweater` | 46 | 368 |
| Midsummer Alice | `serialized/eve-midsummer_alice` | 46 | 368 |

`verify.py` independently rechecks the raw samples, parent and child receipts,
full state equality and geometry restoration. `six-outfit-verification.json`
passes all six. Each summon observes primary and secondary with the expected
copied mesh, finite sampled transforms, unchanged health, normal input release,
and two created/removed CSS groups returning to zero without error or fallback.
Each parent restores the starting Peony selection, all saved settings and idle
preferences, CINE and player/view geometry. The batch also restores the previous
focused window. These are binding and lifecycle checks, not visual cloth/contact
or secondary ghost-parameter acceptance.

Two fixture failures remain recorded:

- The first Bikini child summon passes, but its parent cannot restore the stale
  saved idle `eve_idle_712`, which the current catalog no longer advertises.
  `recovery/result.json` verifies exact restoration using a stationary guarded
  reload of the identical core bytes. The mapped filename ends in
  `state-recovery.dll`; its content hash is unchanged. No game save is edited.
  The fixture now preserves unavailable saved choices and still rejects any
  actually playing montage before input.
- The continuation's Bikini summon and state/geometry/CINE restoration pass,
  but its final appearance query times out while a separate inspection runs.
  The CSSX client used one request/response file without coordinating callers.
  `continuation/restoration-followup.json` independently confirms full state
  and appearance restoration. The original parent stays failed. A bounded
  advisory lock now serializes the entire client exchange, including temporary
  file creation, across processes. Four isolated tests cover two concurrent
  clients, lock timeout without a write, and release after response/write failure.
  Fifteen existing tooling tests also pass. The final serialized Bikini parent
  and both remaining outfits pass end to end.

No game core code or cooked assets changed. This completes Stray mesh-copy and
cleanup coverage for all ten release appearances. Faithful UG/CW, visual
cloth/contact, secondary ghost parameters and additional customization remain
separate open checks.


## Peony secondary ghost materials

`doubles-secondary-20261015/peony-secondary-materials/` passes a focused normal
Stray summon targeting `AstralCloneSecondary` explicitly. The recorded component
is still owned by that active actor after the material reads. Six Peony ghost
families preserve all 58 captured source parameters, two nonzero morphs and
seven hidden sections; private material instances carry the correct Stray form
controls. Both actors are observed across 36 samples with 288 finite visual
bone transforms, and both CSS groups expire cleanly. Full saved state, CINE,
appearance, exact player/view geometry and the previous focused window restore.

`independent-verification.json` rechecks the recorded data and rejects three
negative controls: a primary actor label, an unrelated component and a missing
live actor. The capture tool now accepts an explicit customization actor; the
form wrapper rejects incompatible form/mode combinations before any game work.
Python compilation and whitespace checks pass. No core or asset changes.

This closes current Peony secondary ghost-parameter, morph and hidden-section
copying. Other outfits' material combinations, visual cloth/contact and the
remaining Faithful UG/CW cases are still open.


## Remaining Faithful cases completed

`faithful-remaining-20261015/` completes the three previously interrupted cases
on core `dc0a609e74750815084130278354de76f754643326c8ec3829393d7439cead67`.
Normal summon input produces the expected Faithful `AstralCopy` in every case:

| Appearance | Active observations | Finite sampled bone transforms |
| --- | ---: | ---: |
| Unholy Genessa, Eve | 9 | 72 |
| Unholy Genessa, Genessa | 13 | 104 |
| Commander White | 12 | 96 |

Each copy matches its captured source mesh. Each temporary CSS group is removed,
with no remaining active/cached group, error or fallback. The three-case child
restores the Faithful appearance, settings, CINE and view. The outer fixture
returns to the original Stray form and Obsidian Eredrim appearance. Purchased
upgrade ability records and Alteration's revival-blocker handle remain unchanged.
This uses the previously accepted one-time revival override; it does not edit
save files or remove the upgrade.

The first outer result remains failed because the summons spent 18.75 Resolve
each and the fixture did not refund that cost. All its other restoration fields
pass. A guarded follow-up issues exactly one native `S_GainResolve(56.25)` call.
That call succeeds, but the generic helper then raises `KeyError` because the
function has no return value. It is not called again. Separate read-only
verification checks the successful void-call acknowledgement and confirms
Resolve 100, health 72, exact saved settings, original form/appearance and restored
geometry. `resolve-recovery.json` and `verification.json` retain that result;
the failed parent receipt and request logs remain unchanged.

`verify-recovery.py` independently verifies the three raw summon captures and
records the evidence hashes. The diagnostic now accepts a selected outfit list,
so already accepted Eve cases were not repeated. No production code or assets
changed. Faithful copy binding and cleanup now cover all ten release appearances;
visual cloth/contact and broader material/customization combinations remain
separate requirements.
