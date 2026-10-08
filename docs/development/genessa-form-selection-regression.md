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

The user clarified that the failed Stray summon plays its animation but produces
no doubles. The first Stray ability failure is not yet diagnosed. The log records
the first switch and the successful round trip, but does not capture the failed
ability's notify or spawn state. Do not claim the selection repair fixes that failure.
The native form event is sent once and currently considered complete when the
shell tag changes; ability and animation completion need further inspection.

Selection repair `b1e1cbe` passed 153 animation checks, all 24 host test targets
and the Windows core build. The staged DLL is
`genessa-form-first-stray1/css_core.dll`, SHA-256
`48690559fd0755162174aace640204f686f199aa1a86d523c9848670ec39646d`.
It was installed after the user closed the game, with no game process present
at the final selector change. The selected core is
`css_core-form-selection-48690559fd075516.dll`. Installed readback matches the
staged hash. Protected settings/loader hashes and outfit package file metadata
remain unchanged. `installed.json` and `preinstall/core.json` retain the receipt
and previous selector. In-game confirmation is pending.

The cooked `GA_AstralClones_Action` routes the `Activate` montage notify to
`SpawnPrimaryClone`. The animation task uses
`AbilityTask_PlayMontageAndWaitWithNotifies`. A visible montage alone therefore
does not establish that the spawn call ran. The extracted bytecode is retained
in `native-stray-ability.json`; inspect the game-specific path instead of assuming
the standard engine task has identical behavior.

`capture_summon.py` in the evidence directory reads the current ability spec,
montage/animation instance, primary and secondary clone references and CSS
Astral counters without invoking gameplay actions. It resolves ability instances
again after form changes. Its initial three-second run completed against the
recovered Stray state with no reflection errors. This is an observation tool,
not proof of a reproduced failure or a passing regression check; polling can
miss short-lived events.
