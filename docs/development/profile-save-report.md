# Profile save report

## Confirmed input conflict

The author reproduced a concrete cause after the first candidate: the UI
advertises Space to save, but Space inserts a character while the profile name
field has keyboard focus. The typing guard then suppresses the save binding.
The earlier validation-message change did not address this interaction.

The new-profile Save prompt now shows Enter on keyboard. A foreground-only,
edge-triggered shortcut reads Enter while the field has focus and also after
Slate commits it. Holding Enter or returning to the page with Enter held does
not repeatedly save. The handler is restricted to New profile with no modal or
picker open. Mouse saving stays on the existing button path, and mapped
controller confirmation is admitted while the name field is focused. Other
keyboard bindings remain suppressed while typing.

Epic documents Enter as a text commit action:
[OnTextCommitted](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/WidgetEvent/OnTextCommitted_EditableText).
The game dump confirms `UEditableText.OnTextCommitted`. This repair uses the
existing native polling model rather than installing a global delegate hook.

Host tests cover the Enter prompt key, single press, held-key suppression,
background/page gating, re-entry and a second press. Production compilation and
live focused-field keyboard/controller acceptance are recorded separately under
`work/profile-input-fix/`. The Genessa gameplay clipping report is queued after
this input repair in `docs/work-queue.md`.

The candidate `css_core-beta9-profile-input-20992cfe8c3c16d3.dll` was installed
and hash-verified. The author confirmed "ok it works, thanks" after the
focused-field Enter test. This confirms keyboard saving; it does not separately
verify controller input.

## Initial report and validation-only candidate

October 4, 2026. Markuzkiller reports an "Invalid preset slot" message and only
one visible row. The author believes the installed version is beta.7 or beta.8;
the exact version, entered name and reporter logs are not yet available.

Both release tags use the same `valid_id` check for saved profile names: 1 to 96
ASCII letters, digits, periods, underscores or hyphens, excluding `.` and `..`.
Spaces are rejected. They use "Invalid profile slot" for both invalid names and
the 64-profile capacity limit. The quoted "preset" wording is not an exact match
to those CSS versions, so it does not establish which build produced it.

The UI shows one **New profile** row before any profile has been saved. It
suggests `profile.1`, then lists saved profiles below that row. One visible row
alone is not evidence of a one-slot capacity bug.

## Candidate change

The real save operation now uses the portable `save_profile_snapshot` helper.
Its validation reports empty names, excessive length, invalid characters and
full capacity separately. The input hint explicitly says no spaces and gives
`Eve_BlackPearl` as an example. Accepted names, storage schema, capacity and
replacement behavior are unchanged. No automatic renaming or silent replacement
was added.

The regression failed against the old generic error. The new tests verify
unchanged state on rejection, first and second saves, serialized snapshots,
maximum-length names, full capacity, replacement at capacity and slot reuse.
`css_profiles`, `css_data` and `css_animations` pass.

This improves diagnosis but does not establish the reporter's root cause. Ask
them to leave `profile.1` unchanged or enter `Eve_BlackPearl` and save. If that
also fails, request the exact name, CSS version, `CSS.log` and
`runtime/status.json` from `Binaries/Win64/ue4ss/Mods/CustomShellSystem/`.
Do not delete their settings or profiles as a workaround.

Build and installation evidence stays in `work/profile-save-report/`. No public
ZIP is replaced by this change. Live UI and reporter acceptance remain pending.
