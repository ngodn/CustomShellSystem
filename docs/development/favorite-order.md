# Shell ordering and selection

2026-09-21: The user requested equipped outfits first, then favorites, then
non-favorites. `Catalog::display_order` groups the existing catalog in that
order without changing its order within each group. An equipped favorite is
included only once. Missing saved IDs introduce no rows. The two existing
Harbinger/original-appearance controls remain separate from the outfit entries.

The SHELL page now uses the currently worn outfit on each rebuild. The earlier
entry-time pin could leave a previously equipped outfit above the actual one.
Before clearing the old widgets, the page retains the selected outfit ID.
After sorting, it restores focus to that outfit and reveals its row if it moved.
Favoriting, unfavoriting and equipping therefore update the order without
silently changing which outfit the next action targets.

`work/favorites1/host-exit.json`, `tests-exit.json` and `windows-exit.json` report
successful builds and catalog tests, including equipped favorites, multiple
stable groups, removed favorites, stale saved IDs and empty catalogs. Live
row movement, selection retention and controller prompts await installation.
