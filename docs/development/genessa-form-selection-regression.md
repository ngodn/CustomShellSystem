# Genessa form transition regressions, October 9

The user reports two defects in the installed cloth trial:

- The first Faithful-to-Stray switch plays the transformation but cannot summon
  Stray doubles. Returning to Faithful and then Stray makes the ability work.
- Customize shows only Ground Height, with People as the outfit label, until
  another appearance is selected and Unholy Genessa is selected again.

These are open functional regressions. The accepted menu-only cloth clipping
limitation does not defer either defect.

Evidence is in `work/three-outfit-rig-physics/genessa-form-first-stray1`.
The captured saved state has Unholy Genessa/Eve in the living Genessa slot and
an older `css.npc.people/sk_hilga` selection in the CorruptedGenessa slot.
Harbinger mirroring is enabled. The active mesh and applied appearance are
Unholy Genessa. The renderer/reconcile path resolves the mirrored living slot,
but menu, customization and facial playback consumers read the raw darkform
slot. That discrepancy explains the People label and missing controls. A later
snapshot after reselection contains Unholy Genessa in both slots.

The candidate uses the existing `appearance_selection_key` resolver for those
consumers and status reporting. It preserves the separate saved Stray selection;
no settings cleanup or forced appearance reselection is required. Editing a
mirrored appearance addresses the same living selection used by reconciliation.
Tests cover a stale NPC darkform slot, no darkform slot, edits to the mirrored
selection and preservation of the separate selection.

The first Stray ability failure is not yet diagnosed. The log records the first
switch and the successful round trip, but does not capture the failed ability's
activation/input state. Do not claim the selection repair fixes that failure.
The native form event is sent once and currently considered complete when the
shell tag changes; ability and animation completion need further inspection.
