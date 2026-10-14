# Profiles across mirrored Genessa forms

The profile load path now uses the same living-shell mirror resolver as normal
appearance and customization. The installed core passes repeated profile loads
in Stray while preserving a different saved Stray outfit, then returns to Faithful.

## Cause and behavior

Profiles save a table of shell selections. The old load command read only the
literal current shell tag and queued that choice as an explicit outfit selection.
With Harbinger mirroring enabled, this could load a separate Darkform choice in
place of the profile's living choice and then overwrite the living slot.

The [profile convention](../control-convention.md#profiles-global-character-snapshots)
defines a global character snapshot. Loading now restores its saved selection
table, together with the existing profile settings, after validating the effective
current-shell choice. This also restores saved choices for other shells and
removes choices absent from the snapshot. Stored profiles themselves are unchanged.
Unavailable choices for other shells remain saved and do not block the current
shell. The existing fallback handles them if those shells are later selected.

The restored table goes through ordinary appearance reconciliation. It is no
longer treated as an explicit outfit pick. Copying a newly selected Darkform
outfit back into the living slot is now limited to explicit outfit picks; an
automatic fallback or inferred mirror does not create that extra saved choice.
Loading a Default snapshot clears its old selections, so recovery cannot simply
reapply the outfit that the profile was meant to remove.

The current mirror preference and last observed living shell still determine
which saved slot is applicable. They are not new fields in the profile format.
There is no save-schema or public extension API change. Missing outfit/variant
and incompatibility validation still runs before profile state changes.

## Offline evidence

Stage: `work/three-outfit-rig-physics/profile-mirror-20261015/`.

- The regression fails before the resolver change with
  `Mirrored profile loaded the separate Darkform choice instead of the saved living choice`.
  Its exit -6 and stderr are retained; core dumps were disabled.
- New tests cover mirror on/off, inference after a severed restart, absence of a
  separate Darkform slot, Default restoration, independent/unavailable other-shell
  choices, repeated load and serialization/restart. They call the production
  snapshot restoration helper. The earlier missing-dependency rejection tests now
  call that whole helper, verifying that rejection leaves saved state unchanged.
- All 26 host suites pass. Three archived snapshots and their 12 stored profile
  instances pass the existing serialization and global-settings checks. These are
  not 12 distinct user profiles or actual two-save gameplay transitions.
- The C++23 Windows development core builds. Loader and mounted assets are unchanged.

## Live evidence

The guarded stationary activation completes in game PID 1944353. The candidate
is mapped, the previous core is unmapped, and the original settings, idle choice,
appearance and player/view geometry restore.

`tools/diagnostics/check_profile_forms.py` uses the existing CSS command path to
save one temporary profile, enable mirroring, request Stray, load the profile
twice, request Faithful, restore the original mirror preference and delete the
temporary profile. It reads the actual mesh, shell, customization and saved slots.
No movement keys, camera input, save-file edits or direct gameplay-event probes
are sent. The form requests use the same path as MISC.

The tested state is deliberately distinct:

| Saved slot | Choice |
| --- | --- |
| Living Genessa | Eve / Peony |
| Corrupted Genessa | Commander White / original |

Both loads in Stray retain the actual `SK_PeonyV2` mesh and the complete Peony
customization. The independent Commander White selection remains unchanged.
This would exercise the wrong choice with the old literal-tag load path.

The first run passes the profile and settings assertions but exits 1 because
the form round trip changes the player's position by about 4.44 m and rotates the
actor toward the unchanged camera yaw. The original failed result is retained.
The position was restored using the reflected transform call already used by
CINE, after checking that the same player was stationary and the view/settings
had not changed. Readback matches the original position exactly; actor yaw differs
by approximately 5.7e-14 degrees after the rotation round trip. The separate
restoration receipt keeps its strict-equality failure rather than hiding it.

The fixture now restores the starting transform after the native form round trip,
only for a stationary player displaced less than 5 m with an unchanged view.
The second run (`live-forms-restored`) exits 0: both repeated loads, full saved
state, actual appearance, CINE model and exact boundary geometry pass. The form
round trip displaces the character by 4.15 m before the fixture restores it.
No core change was made to native form movement. The two recorded runs contain
four passing profile-load observations; only the second fixture run passes all
its teardown assertions without a separate recovery step.

This verifies a single-save Faithful/Stray round trip with Peony mirroring. It does
not diagnose the reported cross-save Stray-to-Faithful failure, verify doubles
summoning, establish every outfit's transition behavior, or close the full
gameplay/lifecycle release matrix. Default appearance removal is tested offline;
this live form check does not remove the user's outfits.

## Visibility follow-up found during regression

The subsequent `live-default` check passes both actual Default-profile loads and
restores the full saved state/CINE/view, but exits 1 on its final raw visibility
comparison. Before it starts, Naylshotte, the Genessa catalyst and the Infinite
Seal have actor `bHidden=0`, component `bHiddenInGame=1` and `bVisible=1` under
their original **Only when in use** rules. After restoring those same rules,
their actor flags are 1 while both component flags remain unchanged. Mesh
effective visibility is hidden at both boundaries; this does not establish the
visibility of attached non-mesh effects.

`MiscVisibility::evaluate` in `native/src/misc_visibility.inl` re-hides a cached
item only when its enumerated component becomes visible or it newly enters the
hidden set. It does not check whether the game has cleared the owner actor's
hidden flag. That omission explains how actor-only visibility drift can survive
until a settings reset, even though `set_item_hidden` owns both flags. The
observed mismatch and failed receipt are retained. Do not describe the complete
visibility teardown as passing or force flags back to their drifted values just
to satisfy the fixture.

Next: repair and verify enforcement after actor-only visibility changes, including
the form round trip. This is a candidate mechanism for the user's lingering
Tarstone effects, not a reproduced visible effect or a proven shared cause.

## Candidate and rollback

Installed core: `css_core-development-7f438b1bcaffa5ba-profile-mirror.dll`.
SHA-256: `7f438b1bcaffa5ba43fc488d3f19e9a53bdb83ba481de20bf65038caf6057fc4`.
`candidate.json` and `candidate.patch` retain the build base, source/cache hashes
and implementation. Logs, copied recipes and raw live/restoration results remain
in the evidence stage. The prior `acf3aabd4e95e3e8-profile-dependency` core is
retained for rollback.

Root has 97 GiB and theark has 60 GiB free, rounded by `df -h`. The stage occupies
about 7 MiB with the visibility regression. No large copies or cooks
were produced; no unrelated work was removed. The stable release goal stays active.
