# Profile Default visibility restoration

**Installed and verified live on October 14.** Actual profile loads restore
Default visibility in the world and wardrobe preview. Saved state, appearance,
CINE model, player position and camera restore; the menu is closed and controls
are unblocked. The remaining migration and full-release gates are separate.

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
| Live world profile action | Hidden sidearm/seal return to recorded Default flags on load and repeated load |
| Live wardrobe profile action | Preview flags return to recorded Default on load and repeated load |
| End of menu test | Full world visibility flags, saved state, view and controls restore |

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

The initial build receipt's `installed: false` records the earlier offline
stage. The subsequent activation and live evidence below supersede it.

## Guarded installation and live profile checks

The game was confirmed stationary, with no montage or facial performance.
The retained activation recipe checks the loader ABI/hash and source contract,
verifies the candidate hash, requires CINE between takes, temporarily selects
the native idle, and checks stable appearance, position, camera and zero speed
five times before reload. It verifies the new core mapped, the previous one
unmapped, and restores the saved idle. PID 1944353 remains unchanged. Full CSS
JSON and player/view geometry match before and after. The prior natural-end core
remains available for rollback. This is another guarded stationary reload pass,
not proof of safe reload during arbitrary animation-worker activity.

`tools/diagnostics/check_profile_default_visibility.py` uses the real core
commands, not direct state-file edits. It records the original state, clears
MISC rules, saves a uniquely named temporary Default profile, hides the equipped
sidearm and seal, then loads the profile twice. Both actual components change
from visible-at-Default to hidden and back. The active MISC rule count returns
to zero. It then restores every original rule and deletes its temporary profile.
The world run passes with exact saved-state, CINE, geometry and visibility
restoration.

The preview run uses CSSX's existing menu open/close requests and reflected
`GetDisplayMenuCharacter`. Its separate preview props pass the same checks.
No desktop focus change, movement, attack or camera input was sent. The game's
menu camera is restored on close.

The first menu wrapper stopped before opening anything because it sent
`menu.status` to the outer developer request endpoint. This read belongs to the
engine service. `menu-check2.py` corrects that request; the failure log remains.

The preview fixture's strict final comparison exited 1 because two inactive
world Axatana actors changed `bHidden` from 0 to 1 while the preview was open.
Their components stayed hidden (`bHiddenInGame=1`, `bVisible=0`), and the preview
flags restored exactly. No visible-equivalence or cause claim is inferred solely
from the actor flag. A separate post-close inspection confirms every recorded
world actor/component visibility flag matches the original world baseline.
Movement and look input are not ignored, the menu is closed, and full saved
state matches. `verify-live.py` independently checks these distinct phases and
records the two intermediate differences explicitly. The original failed menu
fixture result is retained. The reusable fixture now reports exact preview flags
and effective world visibility separately when run inside a menu; a caller must
still check world restoration after closing it.

Evidence added under the same directory:

- `activation/`: executed recipe, loader acknowledgment, mapped-core verification,
  class inventories, original settings and exact restoration receipt.
- `live/` and `live-menu/`: actual commands, temporary profile snapshots,
  before/hidden/loaded/reloaded/restored component records and raw results.
- `menu-result.json`, `post-menu-inspection.json`, `live-verified.json`: menu
  closure, final world/control restoration and independent combined verification.
- `verify-live.py` and recursive `SHA256SUMS`: evidence checks and hashes.

The host regression fixture's sidearm key was also corrected from `sidearms`
to the production category `sidearm`; its rebuilt test passes. This test-only
change does not alter the installed core. Build-time source hashes remain tied
to commit `d0ab9df`, before that fixture correction.

This verifies the reported profile defect with the current Peony appearance,
loadout and saved settings. It does not establish every profile/outfit lifecycle,
missing-mod reinstall behavior, CINE migration or the deferred Tarstone effect
report. Stable release gates remain open.
