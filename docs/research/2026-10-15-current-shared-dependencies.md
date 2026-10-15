# Current shared-package dependency checks

Eight production catalog cases pass against the actual current outfit/shared
files and retained pre-migration shared packs. No game files or settings changed.
This extends the four mixed-outfit checks with current missing, outdated and
duplicate-provider coverage.

Evidence: `build/current-shared-dependencies-20261015/`.
Recipe: `tools/diagnostics/audit_current_shared_dependencies.py`, Python 3.14.7.
The existing C++23 `css_package_dependency_tests` target was built successfully
before the run. It calls production `Catalog::load`, verifies exact available
outfit/library IDs, revision binding counts, facial matches and loaded/rejected
counts, and checks the specified diagnostic text.

| Providers alongside the three current outfits | Loaded | Rejected | Facial matches | Verified response |
| --- | ---: | ---: | ---: | --- |
| Current shared | 4 | 0 | 10 | All outfits available |
| No shared | 0 | 3 | 0 | Install CSS Shared Assets |
| Old standalone locomotion | 1 | 3 | 0 | Update CSS Shared Assets |
| Current shared plus old locomotion | 0 | 5 | 0 | Remove duplicate copies of CSS Shared Assets |
| Two copies of current shared | 0 | 5 | 0 | Remove duplicate copies of CSS Shared Assets |
| Old Astral only | 0 | 3 | 0 | Install CSS Shared Assets |
| Current shared plus old Astral | 4 | 0 | 10 | All outfits available |
| Current shared plus both retired packs | 0 | 5 | 0 | Remove duplicate copies of CSS Shared Assets |

The old Astral pak has no CSS manifest. It is not counted as a loaded catalog
package and cannot satisfy the current shared dependency. Current packages and
both retained old packs have no overlapping exported package paths. Two copies
of current shared naturally overlap in 283 packages, with identical export-chunk
hashes, and the catalog rejects the duplicate library rather than selecting one
according to discovery order. No divergent export-chunk hashes were found.

The retired trios come from `Content/CSS-namespace-trial1/backup`, not from the
old dependency fixture's now-broken links to former installed locations. The
current trios match the accepted Eve/shared package and its retained UG/CW
installation record. Input hashes, exact commands, exit codes, inventories,
case links, expectations and overlap details are retained. Inputs rehash
unchanged at completion. The fixtures use symlinks and generate no full copies
or extracted assets.

These results support the current dependency handling and removal guidance.
They do not demonstrate mounting the retired assets in the game, physical
install/upgrade steps, saved-profile migration, CINE execution, rendering or
simulation. The installation instructions should still remove both retired
standalone packs. No new runtime fix was needed for these eight cases.
