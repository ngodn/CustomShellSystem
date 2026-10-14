# Current coordinated package compatibility

The current Eve surface candidate and matching shared package pass four offline
combinations with current or published legacy Genessa/Commander White packages.
No game files or settings changed. This supersedes the withdrawn Peony-only
rehearsal as evidence for these specific current-container combinations.

| Combination | Packages checked | Import failures | Overlapping packages | Matching facial variants |
| --- | ---: | ---: | ---: | ---: |
| All current | 1,021 | 0 | 0 | 10 |
| Legacy Genessa, current CW | 1,045 | 0 | 0 | 8 |
| Current Genessa, legacy CW | 1,022 | 0 | 0 | 9 |
| Both legacy | 1,045 | 0 | 1 | 7 |

The sole overlap is `/Game/CSS/Shared/SKEL_Base` in the two legacy packages.
Their recorded export-chunk hashes are identical. No combination contains
different export-chunk hashes for the same package. Current packages have no
overlap. This checks exported package content, not every possible resource or
dynamic lookup in the game.

## Inputs and method

Evidence: `build/coordinated-upgrade-20261014-2`. The current Eve/shared input
hashes come from `build/eve-surface-package1/verified.json`. Current Genessa and
CW match that candidate's retained-package installation record. Legacy trios
remain in `Content/CSS-namespace-trial1/backup` and match the retained input
hashes previously checked against their published archives by
`peony-public-upgrade2`. No archive is substituted by version string alone.

The host package-dependency test was rebuilt successfully before this run.
It uses production `Catalog::load` and checks available outfit/library IDs,
revision binding counts, loaded/rejected package counts and exact facial-match
counts. All four cases load four packages and reject none. The mounted package
auditor checks cooked imports and records conservative package-name paths.
All commands, exit codes, input hashes, inventories, overlaps and import/catalog
results are retained. Inputs were rehashed unchanged after the tests.

The first attempt used the older cloth reader from the surface package recipe;
it rejected `--package-name-paths` before producing a report. That attempt is
retained at `build/coordinated-upgrade-20261014`, including its exact recipe and
exit code -6. This is an audit-process failure, not a game crash or package
failure. The passing run uses the hash-matching reader from the earlier
`cw-retirement-audit1` upgrade investigation, which supports that option.

## Reproduction and limits

Python 3.14.7, .NET 10.0.401 and the existing C++23 host target are used. Run:

```sh
cmake --build build/host --target css_package_dependency_tests -j 2
python3 tools/diagnostics/audit_coordinated_upgrade.py \
  --output build/coordinated-upgrade-recheck
```

The output directory must be new. The reader, CUE4Parse library, mappings,
packer, catalog binary, test source and package trios are pinned in the output.
This is still a local candidate with existing metadata versions: Eve 1.4.0,
shared 0.1.0-dev, Genessa 0.2.0 and CW 0.0.6-dev. These labels are not the agreed
stable release targets. No stable ZIP, version tag or public upload was made.

This audit does not test copying files over an old installation, loading saved
profiles, CINE execution, dynamically constructed references, runtime facial
playback, rendering, simulation or performance. It also does not cover leaving
retired standalone locomotion/Astral packs installed. Those migration and full
goal gates remain separate.

## Cleanup

The [removal list](../development/cleanup-2026-10-14-coordinated-upgrade.md)
was written before deletion. Both runs' metadata extractions were verified
identical, then removed. Original packages, test mounts, inventories and all
small failure/success receipts remain. Exact counts are recorded in
`metadata-cleanup-completed.json`; per-file hashes remain in
`metadata-cleanup-plan.json`. Immediately before and after cleanup, `df -h`
reports root 91G available and theark 67G available. These rounded readings
do not establish physical bytes reclaimed.
