# Profile save report

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
