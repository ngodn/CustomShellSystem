# Genessa form after changing saves

October 14 status: the reported failure is not reproduced or fixed. The game
is closed. This investigation prepares the observation needed to distinguish
a rejected native transition from a stale UI or player reference.

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

Only the no-game guard has been run so far:

```text
python3 tools/diagnostics/capture_genessa_form.py --output work/genessa-doubles/form-save-switch1/live.jsonl --seconds 1 --expect-form faithful
error: Need one running game; no request was written
```

The live transport and getter capture remain untested. Existing evidence
establishes the controller property names and getter signatures; the capture
checks getter signatures again before calling them. A valid diagnosis still
needs the reported two-save sequence, a same-save control and a fresh-launch
comparison. No speculative fix is justified by the current evidence.
