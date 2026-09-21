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
stable groups, removed favorites, stale saved IDs and empty catalogs. The shipping build also passes. The change is installed in `css_core-ui1.dll`
through the recorded normal restart (`work/ui-live1/deployment.json`). The user
reports the controls work. Live row movement, selection retention and rendered
ordering are still being checked.

## Live verification

`work/ui-live1/shell-order.jpg` was reviewed and shows equipped Black Pearl,
then starred BeauteGenessa and Seductress v2.0.2, then non-favorites. The existing
Harbinger/original controls remain above the outfit entries.

`tools/check_favorite_focus.py` selected BeauteGenessa and temporarily removed
its favorite flag. The live selected-row index moved from 3 to 6, with the row's
actual favorite action still targeting `beaute.genessa`. Restoring the flag
returned it to row 3 without changing that target. The complete saved state
matched before/after. `favorites/verification.json` passes; both Steam stills
were reviewed, including the unchanged right-hand details while the row moved.
No outfit was equipped or customization changed during this check.
