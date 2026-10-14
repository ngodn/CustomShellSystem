# Profile Default visibility restoration

The save/profile review found a CSS core defect: loading a profile with an
explicit empty `misc_rules` object kept visibility overrides enabled after that
profile was saved. An empty map is a valid snapshot of Default visibility.
The old `load_profile`/`load_look` branch skipped it because it only applied
nonempty maps.

The failing test saves Default visibility, enables hidden seal and in-use
sidearms, then restores the profile. Before the correction it fails with
`Loading an explicit Default visibility profile did not report a change`.
This is an offline reproduction of the production settings path, not an
in-game observation.

## Correction

`Preset::has_misc_rules` distinguishes an explicit map from older profiles that
never recorded visibility. New snapshots always record the map, even when
empty. Parsing retains valid map presence; serialization does not invent it
for an older profile. Null, array and string values preserve the old behavior
of not applying a visibility snapshot.

The production request branch now calls `restore_profile_settings`, shared with
the host tests. It restores animation choices, walk mode, stored visibility and
optional doubles settings. When visibility changes, the request still calls
`Appearance::set_misc_rules`; an empty map clears its active rules and marks them
for restoration on the next frame. Outfit application remains in the existing
request path. The helper does not change selections, remembered customization
or stored profiles.

No schema version or public profile ID changes. An old profile already rewritten
by an earlier core with an explicit empty map cannot be distinguished from a
deliberate Default snapshot; that explicit map now means restore Default.

## Verification

| Check | Result |
| --- | --- |
| Original empty-map restore condition | Reproduced failure, exit -6; core dump disabled |
| Explicit empty map, repeated load | Restores Default, second load reports no change |
| Structured and bare legacy profiles without MISC | Preserve current visibility, including after serialization/reload |
| Malformed non-object MISC data | Preserves current visibility |
| Nonempty profile settings | Restores visibility, animation choices, walk mode and doubles |
| Retained user state | Three snapshots, four profiles each, 12 restore cases pass |
| Full host tests | 26/26 pass |
| Current/mixed legacy package catalog cases | 4/4 pass with rebuilt host code |
| Windows development core | Build succeeds, `CSS_INVENTORY_DEV=ON` |

The retained snapshots are from `cw-rescan-crash1/recovery/css-state-before`,
`peony-v2-body-live2/hot-reload1/before/state`, and
`peony-stationary-cost-20261014`. Tests verify unchanged serialized selections,
animation choices, profiles, remembered customization, favorites, MISC and
doubles fields. All source snapshots remain hash-identical. The four profiles
overlap between these snapshots; they are not 12 distinct user profiles.

The archived-state test exercises parsing, serialization and global profile
settings restoration. It does not execute outfit application or resolve every
saved identifier against the game. The package test separately checks the
four retained catalog combinations. Neither closes live profile migration,
missing-mod/reinstall behavior, CINE migration or the full release matrix.

## Candidate and reproduction

Evidence directory:
`work/three-outfit-rig-physics/profile-default-restore-20261014`.
It contains the failing result, initial and final build logs, final test results,
catalog result, pinned archived inputs, source hashes, patch and candidate core.

```sh
cmake --build build/host -j 2
ctest --test-dir build/host --output-on-failure
cmake --build build/windows --target css_core -j 2
build/host/css_profile_tests \
  work/three-outfit-rig-physics/cw-rescan-crash1/recovery/css-state-before/state.json \
  work/three-outfit-rig-physics/peony-v2-body-live2/hot-reload1/before/state/state.json \
  work/three-outfit-rig-physics/peony-stationary-cost-20261014/state-before.json
```

Uses the existing C++23 host and clang-cl Windows builds. Candidate:
`css_core-development-7004a5a913f6c8ce-profile-default.dll`, 5,010,432 bytes,
SHA-256 `7004a5a913f6c8ce31fb26b64e6b055134b684269074faa7eb42079b286b3dc1`.
This changes the CSS core only. No outfit or shared assets were rebuilt.

**Not installed.** The existing game process remains running, and this turn
stayed offline after the user was released from the stationary test. The active
core remains the accepted natural-completion trial. Install this candidate in
a suitable closed-game or guarded stationary window, then check the actual
profile action and world/menu visibility restoration. Stable release gates
remain open.
