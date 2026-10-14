# Genessa form after changing saves

October 15: a failed Stray-to-Faithful request is reproduced on the user's
upgraded save. Its active Alteration upgrade applies `State.ReviveBlock`, which
the native revival ability explicitly rejects. This explains this captured
failure; the full two-save/fresh-launch matrix remains open.

## Upgraded-save diagnosis

Evidence: `work/three-outfit-rig-physics/doubles-secondary-20261015/`.

- `revival-tags.json` records `State.ReviveBlock` and `State.Severed` on the
  current living Stray pawn, with the reflected getter signature.
- `revival-base-package.json` contains `GA_ShellReviveBase.K2_CanActivateAbility`.
  Its bytecode returns false when the avatar has `State.ReviveBlock`.
- `genessa-upgrade-packages.json` shows Alteration applying
  `GE_Genessa_ReviveBlock` and the severed-state effect, then entering Darkform.
  Its end path removes the blocker and requests native revival.
- `revival-abilities.json` records the current pawn's active Alteration instance.
  `alteration-state.json` records both live effect handles as executed. The revival
  instance is inactive, with no received event or active montage in
  `revival-state.json`.
- `native-player-zero.json` verifies that the game's player-zero lookup returns
  the same current pawn CSS resolves. This failure is not a stale-player result.

CSS previously sent the native request once and waited for the 15-second
timeout. A source change now checks the same native tag before sending and
explains that the game blocks revival, mentioning Alteration as a possible
cause. It does not remove effects, alter upgrades or force a forbidden form.
The C++23 development build passes and core `4a4402a210f4d1e8` is installed.
`work/three-outfit-rig-physics/genessa-revival-block-20261015/` records the guarded
stationary activation and a live blocked request. The message appears in 0.2035
seconds. Actual form, blocker tag, health, Resolve, appearance, full CSS settings,
CINE and player/view are unchanged, with the same running process. No native
revival event is sent by the blocked branch. Valid, unblocked transitions still
need regression on this exact core.

The first activation preflight was too restrictive: it required CINE's Take
button to be enabled even outside Cine World. It changed nothing. Source review
shows Record and Capture are both disabled during active takes or recording;
the corrected preflight requires those controls enabled. All stationary,
performance, loader-contract and process-mapping checks remain. Iris Out was
temporarily stopped, then its saved selection was restored exactly.

The user also requests an optional confirmed override. This is queued in the
goal, not implemented. Determine native form/autosave persistence and reversible
upgrade behavior before offering a no-save-impact guarantee. Normal game rules
remain the default, and Cancel must make no changes.

The temporary outfit state was separately restored through existing CSS mirror
and wear controls. `restore-via-mirror.json` verifies full saved-state equality
and unchanged player/view geometry without changing form or editing save data.

## Earlier investigation

The report is specifically Stray on one save, followed by another save where
requesting Faithful appears ineffective. The previous single-player form-event
roundtrip in `work/genessa-doubles/form-control1/live-roundtrip1-result.json`
does not cover that sequence.

## Verified code paths

- `native/src/core.cpp` handles `genessa_form` as an in-memory one-time request.
  It does not persist a form preference or write it into a profile. Default
  cancels the pending request.
- The queued request records the pawn handle. Its update cancels when the
  current pawn differs or the gameplay character is no longer Genessa. It
  waits for the menu to close, a living player, appearance readiness and
  performance cleanup before sending the native event once. A sent request
  times out after 15 seconds unless the current shell tag reaches its target.
- `Appearance::player` resolves the current viewport world and player. The
  bridge returns that pawn and its controller. The UI displays the current
  shell tag, not a stored form setting. These paths do not establish whether
  the actual failing session held stale state.
- Extracted `BPFL_Player.ForceReviveShell` resolves player zero from the given
  world context, builds event data, and sends `Event.Shell.Revive.Force`.
  `Silent=true` selects event magnitude -1. The function has no success
  output. Returning from this call does not prove revival occurred.

The relevant extracted function and hashes of the inspected sources are in
`work/genessa-doubles/form-save-switch1`. No gameplay event, save edit,
ability removal, forced shell activation or core replacement was performed.

## Next capture

`tools/diagnostics/capture_genessa_form.py` uses the existing CSSX bridge to
observe current pawn/controller identity, actual shell, shell/darkform
definitions, menu/death state and CSS status messages. It reacquires the player
each sample and flags a player replacement during a sample. It sends no form
requests and no movement or menu input.

With the game running, start this before the normal save-change sequence:

```sh
python3 tools/diagnostics/capture_genessa_form.py \
  --output work/genessa-doubles/form-save-switch1/live.jsonl \
  --seconds 180 --expect-form faithful
```

Select Stray on the source save, return to the menu, load the destination save,
then request Faithful through MISC and close the menu. The final check fails
unless a stable final sample actually reports Faithful. The capture records
observations, not save contents; annotate which two saves were used separately.
It cannot prove that the native ability activated or explain a rejected event
by itself. If failure reproduces, inspect the destination's native ability
state before considering a change.

At the October 14 checkpoint, only the no-game guard had run:

```text
python3 tools/diagnostics/capture_genessa_form.py --output work/genessa-doubles/form-save-switch1/live.jsonl --seconds 1 --expect-form faithful
error: Need one running game; no request was written
```

That dedicated passive capture remains untested. The October 15 focused probes
above exercise the existing live transport separately. Existing evidence
establishes the controller property names and getter signatures; the capture
checks getter signatures again before calling them. A valid diagnosis still
needs the full reported two-save sequence, a same-save control and a fresh-launch
comparison before closing all cross-save acceptance criteria.
