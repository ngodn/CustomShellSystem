# Hidden item actors after form changes

Follow-up: [Burn visibility controls](2026-10-15-sidearm-effect-visibility.md)
now reproduce the lingering orange effect with mesh-only hiding and remove it
through the repaired owner path, preserving infusion. Natural aim/fire/stow
and the original gameplay trigger still require checking. The record below
describes the earlier actor-flag fixture and its narrower scope.

The installed development core repairs actor-only visibility drift for items
that MISC currently wants hidden. Default mode still leaves visibility to the
game. Controlled live fault injection and the original form/profile sequence
pass. The user's visible Tarstone residue has not been reproduced by this test.

## Reproduction and repair

The [profile/form regression](2026-10-15-profile-forms.md#visibility-follow-up-found-during-regression)
found Naylshotte, Infinite Seal and Genessa catalyst actors with `bHidden=0`
while their enumerated meshes had `bHiddenInGame=1`. All three were stowed under
**Only when in use** rules. Resetting and restoring the rules hid their actors
again, which caused the fixture's original/final raw-flag comparison to fail.

`MiscVisibility::evaluate` cached the item as hidden and checked only its mesh
flag before deciding whether to reapply hiding. It could therefore miss the
actor becoming visible independently. The repair checks both flags for the
cached items that the current rule wants hidden, then uses the existing
`set_item_hidden` path. It does not change the rule decision, socket detection,
native form events, gameplay effects, child-propagation policy or Default mode.

The added read uses the owner's existing weak reference and a reflected
`FBoolProperty` with its runtime offset. It adds no object search, callback,
asset load or logging to the frame loop. The additional property check is
bounded by the cached hidden-item list. Invalid owners skip the actor check;
the existing component handling remains.

## Live negative and positive controls

Stage: `work/three-outfit-rig-physics/item-visibility-drift-20261015/`.
Recipe: `tools/diagnostics/check_item_visibility_drift.py`.

The fixture starts with the three stowed actors and meshes already hidden.
It verifies the reflected `SetActorHiddenInGame` signature, clears one actor's
flag while preserving its mesh flag, samples enforcement for up to two seconds,
then restores that actor. No weapon, ammo, item-use or movement input is sent.
It also temporarily selects Default for the sidearm and confirms CSS does not
re-hide its actor. Settings, target flags, player identity and view are checked
before and after.

| Case | Previous core `7f438b1b…` | Fixed core `d7cb0635…` |
| --- | --- | --- |
| Catalyst actor flag cleared | Still visible at final sample | Hidden at first sample |
| Naylshotte actor flag cleared | Still visible at final sample | Hidden at first sample |
| Infinite Seal actor flag cleared | Still visible at final sample | Hidden at first sample |
| Sidearm Default control | Actor and mesh remain visible | Actor and mesh remain visible |
| Original settings/flags/view restored | Pass | Pass |

The positive snapshots finish 0.31-0.32 seconds after the setter response.
This includes RPC/polling time and is not a measurement of repair latency in
frames. The negative-control run exits 1 for the three unrepaired actors; the
same corrected fixture exits 0 with the candidate.

The first fixture attempt also exposed a diagnostic mistake: it expected an
explicit `misc_mode: default` command to erase the saved rule, but production
stores a Default entry. That attempt stopped at its state guard. Only the
test-owned sidearm rule differed; it was restored through the normal command
after checking the complete expected state. `red/restoration.json` records that
recovery. The corrected `red-restored` run retains the real three-actor failure
and passes all teardown checks. Both attempts are retained.

## Natural form sequence and profile regression

After guarded stationary core activation, `check_profile_forms.py` passes:
Faithful to Stray, two mirrored Peony profile loads preserving the separate
Commander White choice, then Faithful and the original mirror preference.
The fixture restores the 4.36 m displacement during the form round trip using
its previously verified bounded transform-restoration path. Full saved CSS
state, actual appearance, CINE model and exact player/view boundaries restore.

The subsequent `check_profile_default_visibility.py` now also exits 0. Both
Default-profile loads restore actual component/actor flags, and the original
settings and every observed world visibility flag restore exactly. This is the
sequence that previously passed profile loads but failed the final actor-flag
comparison. Temporary profiles are deleted; the menu is closed.

The Windows core builds successfully. This runtime-only change is exercised by
the live controls above; portable catalog tests do not execute this Unreal
visibility implementation. No outfit, shared package, loader or UI was changed.

## Timing observations and limits

Three 10-second captures before and three after contain 3,283 records. All stop
for duration, contain no CSS exception and preserve saved settings, appearance,
camera and renderer-setting boundaries. Object handles change across core
reload, so cross-core context comparison uses their recorded names/classes.
All raw rows are finite/nonnegative, and independently recomputed means,
medians, nearest-rank p95/p99 and maxima match the saved summaries.

Median of each build's three capture medians, in milliseconds:

| Measurement | Before | After | Difference |
| --- | ---: | ---: | ---: |
| MISC elapsed time | 0.0534 | 0.0742 | +0.0208 |
| CSS callback elapsed time | 0.1924 | 0.2403 | +0.0479 |
| Callback interval | 16.80595 | 17.4341 | +0.62815 |

Callback-interval p99 ranges from 25.37-31.53 ms before and 37.49-41.40 ms after.
This is a sequential comparison, without randomized or reverse ordering,
isolated background load, matched per-frame animation poses or GPU timing.
Elapsed phase time includes possible thread preemption. It does not establish
an FPS effect caused by the new check, nor prove performance unchanged. Keep
these results in the remaining matched gameplay/performance review instead of
declaring the performance gate passed. `cost-verified.json` retains every
capture's figures and verification scope.

## Installed candidate and remaining scope

Core: `css_core-development-d7cb0635d112f5fa-item-visibility.dll`.
SHA-256: `d7cb0635d112f5fae73f67bc261c81d07df07eb0fef8797d786cc5d55191072f`.
Size: 5,014,016 bytes. `candidate.json`, `candidate.patch`, build log, activation
recipe and raw test receipts preserve provenance. The prior `7f438b1bcaffa5ba`
profile/form core remains available for rollback. Game PID 1944353 is retained.

No visible Tarstone residue, infusion, aiming/firing, projectile or damage test
was performed. Actor visibility may be relevant to effects owned by that actor,
but do not call the user's Tarstone report fixed on this evidence alone. The
cross-save Genessa report, broader gameplay/preview matrix and stable release
goal remain open. Disk space remains about 97 GiB on root and 60 GiB on theark;
only the small core and diagnostic evidence were added.
