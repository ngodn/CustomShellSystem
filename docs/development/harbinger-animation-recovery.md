# Selected animations after Harbinger revival

On October 4 the author reported losing Eve Default Idle after death and revival
into Harbinger form, with possible walk/jog/sprint failures too. Story, Tarstone,
item and Genessa revival were reported as triggers. These individual triggers
have not all been reproduced locally.

## Confirmed selection defect

Appearance reconciliation already mirrored the last living shell's selection
when `harbinger_mirror` was enabled. It deliberately did not copy that selection
into the Harbinger save slot, preserving the user's separate Harbinger look.

`Core::sync_walk_safely` instead looked up `state.selections[appearance.shell]`
directly. After severing, that is the Darkform slot. An empty slot supplied no
custom clips, and a different saved variant failed the `applied_id` check.
The mirrored mesh could therefore look correct while its selected animations
were absent. The same lookup supplied idle, walk, jog and sprint.

The existing playback layer already reacquires a changed main anim instance,
checks the current post-process instance and idle fields, and heals overwritten
blendspace fields. It cannot restore a clip that the selection layer stopped
requesting. No new graph setters or animation hooks are needed for this defect.

## Repair

`appearance_selection_key` now supplies the selection for both appearance
reconciliation and animation playback. Built-in feminine idle/walk and animation
choice validation use that source too. The previous mirroring rules remain:

- Prefer the saved last living shell if its outfit supports the current form.
- On a load while severed, infer the shell from the Darkform name, including
  CorruptedGenessa.
- With mirroring disabled, keep the Harbinger's own selection.
- Missing or incompatible outfits do not become mirror sources.
- Resolve each animation from that outfit and variant's existing choices.
  Default and missing clip definitions still fall back to the game.

The resolver reads saved data only. It does not duplicate selections, change
gameplay shell identity, resurrect the character or modify animation assets.

## Evidence and checks

The host regression failed before the repair with
`Revival lost the mirrored animation selection`. It now passes repeated
living/Harbinger/return transitions for all four slots, saved-state reload,
CorruptedGenessa inference, mirrored animation edits, Default, feminine walk,
independent Harbinger variants, and incompatible/uninstalled outfits.

The cooked `GA_ShellRevive_Genessa` and `GA_ShellRevive_SavedByTheShell` use the
common `GA_ShellReviveBase` path, whose `ReviveShell` calls `ActivateShell`.
Extracted bytecode and package list are in `work/sidearm-traversal/revive.json`
and `revive-packages.txt`. This supports checking the common transition rather
than adding item-specific fixes. It does not prove every reported trigger is
now correct in the running game.

Production C++23 core builds. Animation, animation-runtime, recovery,
skeleton-compatibility and socket-fit suites pass (5/5). The installed candidate
also includes the pending sidearm traversal repair; its hash and previous
selector are recorded in `work/sidearm-traversal/install-revive.json`.
Live acceptance is pending. Public release ZIPs remain unchanged.
