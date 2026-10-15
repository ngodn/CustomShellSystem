# Existing shared-pack upgrade evidence

The physical upgrade from the separate shared packs happened on October 8.
Do not repeat it merely because the installation receipt predates later
gameplay acceptance. The reconciliation audit passes at
`work/three-outfit-rig-physics/upgrade-evidence-20261016/result.json`.

The transaction at `MortalShell2/Content/CSS-namespace-trial1` replaced six
containers with four: three outfits and consolidated Shared Assets. Its plan
hash matches the independent installation validation. All eighteen retained
original files still match their recorded hashes. The old Astral and Locomotion
folders are absent from the mounted mods directory. The original installation
verified 59 protected files unchanged, including CSS settings and CINE files.

The pre-upgrade status records Prototype and the old Iris Out and gait paths.
That is historical status, not a newly observed process. Subsequent native
library, outfit and CINE results exercise the migrated paths. Both Genessa CINE
takes complete and restore CSS/CINE state. Fresh shipping startup separately
proves default-state creation and current-provider discovery.

Existing compatibility evidence includes four current/legacy outfit catalog
and import cases, eight shared-provider cases, and actual missing-provider
removal/reinstall. The first two sets are offline tests; they do not establish
live rendering of every old package combination. See
[package compatibility](2026-10-14-coordinated-upgrade-audit.md),
[dependency cases](2026-10-15-current-shared-dependencies.md),
[provider lifecycle](2026-10-15-shared-provider-lifecycle.md), and
[fresh shipping startup](2026-10-15-clean-shipping-runtime.md).

The transaction retains the pre-migration state file's hash, but that exact
JSON was not found among 928 retained state files. Later archived-profile tests
and live profile loads must not be described as replaying that snapshot. An
older September snapshot contains excluded Seductress variants and was not
loaded as an Eve/Genessa/CW migration test. This missing historical file is a
provenance limitation, not a new requirement to recreate old user settings.

The physical replacement is accounted for. Saved-profile coverage remains
limited to its recorded tests. No game changes, new asset copies or repeated
gameplay checks were needed for this reconciliation.
