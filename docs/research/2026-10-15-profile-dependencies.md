# Profile loading when an outfit is unavailable

The installed development core now rejects a profile whose current-shell outfit
is missing from the catalog, whose variant has been removed, or whose outfit is
incompatible with that shell. Rejection happens before scheduling an appearance
change or restoring global profile settings. The saved profile remains intact.

## Defect and change

Previously `Core::command` scheduled the profile's selection and immediately
restored MISC visibility, animations and doubles settings. The deferred appearance
apply checked availability later. A missing selection could therefore change
global settings and report success without applying the saved outfit.

`profile_selection` in `native/src/data.cpp` validates the current-shell choice
against the loaded catalog before the command writes any pending selection or
global state. An unavailable outfit reports that the outfit and required shared
assets should be installed. A missing variant or incompatible shell has its own
error. A profile without a current-shell selection retains the existing default
appearance behavior. Unavailable selections for other shells do not prevent the
current shell's valid selection from loading.

This is catalog preflight, not a transaction around every engine asset load. A
package accepted into the catalog can still fail during runtime asset loading.
That case and a shell transition between scheduling and application are outside
this change's verified scope.

## Verification

Evidence directory:
`work/three-outfit-rig-physics/profile-dependency-20261015/`.

- Before validation was added, the new regression failed with
  `Unavailable profile was not rejected before settings changed` (exit -6;
  core dumps disabled). The original red result is retained.
- All 26 host CTest suites pass after the fix. The profile test checks the three
  rejection cases, unchanged serialized state, recovery after the catalog entry
  returns without resaving, and unaffected default/other-shell behavior.
- Three archived user snapshots and their 12 profiles retain settings through
  parsing, serialization and repeated global restoration.
- Four actual current/mixed-legacy catalog combinations pass using the rebuilt
  production catalog library.
- The C++23 Windows development core builds successfully. No outfit, rig,
  material, mounted package or loader changes were made.
- A guarded stationary reload keeps game PID 1944353, confirms the new DLL is
  mapped and the previous DLL is unmapped, and restores the exact saved state,
  idle choice, appearance and player/view geometry.
- The existing live Default-profile test passes on the new core: save a temporary
  profile at Default visibility, hide the equipped sidearm/seal, load twice and
  compare their actual component/actor flags. Original visibility, full saved
  state, CINE model and player/view geometry restore. The temporary profile is
  deleted. No gameplay or camera input was sent.

The live test accepts an explicit expected core SHA-256, preserving its mapping
and hash guards instead of requiring a new hardcoded diagnostic for each build.
Its default remains the previously verified core for reproducibility.

Missing-package rejection and reinstall recovery were tested offline with the
production selection helper. No live package was removed or saved state injected.
This turn's live regression covers normal profile loads in the world, not a new
wardrobe-preview or installation-lifecycle pass.

## Installed candidate and rollback

Installed core:
`css_core-development-acf3aabd4e95e3e8-profile-dependency.dll`

SHA-256:
`acf3aabd4e95e3e83fdba3018d5ffb831d6dc3d6aa7d5263bb1857a0e8fefd03`

Size: 5,011,968 bytes. `candidate.json` records the build base, source hashes and
cache hash; `candidate.patch` records the implementation and diagnostic changes.
The stage retains build/test logs, activation recipe and live test receipts.

Previous installed core remains available for rollback:
`css_core-development-7004a5a913f6c8ce-profile-default.dll`, SHA-256
`7004a5a913f6c8ce31fb26b64e6b055134b684269074faa7eb42079b286b3dc1`.

The existing guarded stationary reload procedure was reused. This does not settle
the historical animation-worker reload crash or authorize arbitrary active-motion
reloads. Root has 97 GiB and theark has 60 GiB available, rounded by `df -h`.
Only the small core candidate and evidence were added; no full asset copy or cook
was produced, and no unrelated files were removed.

## Remaining profile investigation

Follow-up: [the mirror-profile correction](2026-10-15-profile-forms.md) now
reproduces and repairs the selection mismatch below, with a single-save live
Faithful/Stray regression. The separate cross-save form report remains open.

The profile command selects by literal current-shell ID. Normal appearance lookup
can instead resolve a living shell's saved slot while Harbinger mirroring is on.
Review profile lookup and deferred-apply slot ownership together before changing
either: merely redirecting lookup could write a mirrored choice into the wrong
slot. This is a source-level investigation target, not a reproduced defect or an
explanation of the user's separate Stray/Faithful cross-save report.

The full release goal remains active. Broader profile/save migration, actual
missing-provider/reinstall lifecycle, form transitions and the other release
gates are not closed by these results.


## Later mirrored-provider review

The [stable runtime review](2026-10-15-stable-runtime-review.md) found that the
mirrored selection resolver could skip a missing living-shell provider before
this preflight. Commit `ac62655` fixes it, with remembered/inferred Stray source,
fallback, disabled-mirror and reinstall host regressions. The development
candidate is now installed through guarded activation; repeated normal live
profile loads and exact restoration pass. Actual missing-provider uninstall/
reinstall remains untested. Direct-shell acceptance above remains historical.
