# Intermittent original appearance after a gate

October 4, 2026. The author clarified that the outfit sometimes reverts to the
original shell after a teleport gate or launchpad. This is a mesh restoration
report, not a report about colors or selected locomotion.

## Evidence and limits

The installed `df300efb4638e4a6` core logged multiple successful restorations and
no recovery error. A read-only probe captured recreation of the main animation
instance around `A_Shared_Traversal_Shell_Throw_DarkBro_Montage`. The subsequent
55-second mesh recording contained 71 samples, all wearing Unholy Genessa on
Corrupted Genessa. It did not capture the failed original-shell state. The author
had already made another appearance selection. No controls or appearance
settings were changed by the diagnostic tools.

Evidence is retained in `work/gate-intermittent/`. Do not treat a successful
instantaneous apply or this healthy recording as proof that the intermittent
report is resolved.

## Recovery gap and candidate

`repair_mesh_needed()` previously recognized only the original mesh captured
when CSS applied an outfit. The same pawn and component can retain that baseline
across shell/Harbinger changes. A different default mesh arriving after the
one-time transition check therefore did not schedule recovery.

The cooked `BP_PlayerController.SwitchToShellMesh` reads the current pawn's
`ShellClass` default object. `SwitchToDarkFormMesh` uses `DarkformClass`. Both read
`GetDefaultMeshSoftReference`, whose inspected base implementation returns
`DefaultMesh`. These functions are extracted in
`work/sidearm-traversal/{controller,traversal}.json`. The candidate recognizes an
exact match to that current definition as well as the captured original. It
reads the soft path without loading the mesh or invoking a gameplay switch.
An unresolved definition does not authorize an arbitrary replacement.

The normal 250 ms reconciliation loop catches a delayed recognized reset without
depending on the transition edge. Pawn/component ownership, automatic restore,
saved selection, traversal, input and montage guards remain. The normal path
returns before definition lookup when the CSS mesh is already present. When the
new default differs from the captured one, apply takes a fresh restoration
baseline instead of reusing the old mesh's material/control state.

On-demand transition inspection now includes the captured original, intended
mesh, current definition, actual active state, mesh-repair decision and readiness
blocker. Recovery logs also name the mesh being replaced. No continuous verbose
logging or new frame loop was added.

## Validation

The host regression models a late reset to a default different from the captured
mesh, after the completion check already saw a healthy outfit. With the previous
captured-only predicate it failed with `Late gate reset to current shell default
left original appearance visible`. It passes with the candidate, alongside
checks excluding missing meshes, unknown replacements and an unowned component.
This establishes the code-path gap, not the unrecorded live incident's cause.

Five relevant host suites pass: recovery, profiles, animation data, animation
runtime and MISC attachments. The C++23 production Windows build passes with
inventory development and transition mutation commands disabled. Live acceptance
of repeated gates/launchpads is still pending. The public ZIP is unchanged.

Installed with the game closed: `css_core-beta9-gate-default-1f522f0859f2fa26.dll`.
SHA-256: `1f522f0859f2fa267acba3b939c23b9fa88ab43b1dc35bbbc5b49174e8f946c7`.
The selector and copied DLL were verified. The previous core and selector backup
remain available; no package or saved appearance settings were changed.

## Author acceptance

The author relaunched the installed candidate and reported "ok seems fixed" on
October 4, then authorized beta.10. The loader reports the expected candidate.
The new log also captures a late stock reset: after recovery at tick 513673887,
another reconciliation at 513674055 sees `SK_Sester_Genessa_V6` and restores
Unholy Genessa at 513674146. This confirms late mesh recovery in that session.
It does not establish every gate, weapon, revive source or performance case.
