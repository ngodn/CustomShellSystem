# CSS Idle Animation correction

The user requires the shared body library in the existing
**Locomotion > Idle Animation** setting. The separate Performance section and
Play workflow were the wrong CSS design and are removed.

All 60 body sequences resolve through the saved idle choice. Walk, Jog and
Sprint retain their own settings. Compatibility filtering still limits default
game human skeletons to Eve Default Idle, Walk, Jog and Sprint. Canonical IDs,
audited bundled fallbacks and CINE's authored playback modes are unchanged.
No outfit or shared asset recook is needed.

Idle Animation includes an inline search field. Results use the existing
choice rows: click a result to select it, or press Enter while searching to
select the first available match. Empty results cannot select anything.
Left/right navigation follows filtered results; clearing the search restores
the full list. Search keystrokes do not move the preview camera or invoke CSS
shortcuts. The search query is temporary, while the selected idle is saved in
the existing per-variant profile field.

The regression first failed because the actual Idle resolver rejected the
shared dance. It now passes, including saved dance selection, exact old-idle
fallbacks, all 60 packaged body choices, and CINE resolution preservation.
Both combined and standalone real catalogs pass. The Release Windows core
build passes. In-game UI, playback and restoration acceptance remain pending.

The idle sequence branch already releases for movement, montages, sidearm aim,
menus, ungrounded movement and player replacement. This change reuses that
branch rather than starting a separate CSS scene. Synchronized facial tracks
currently belong to CINE's explicit playback path; automatic facial-track
evaluation for these newly selectable idle clips still needs integration and
verification. Do not count body selection as completed facial acceptance.

The user rejected the added ESAP author credit in public-facing notes. It has
been removed. Internal source evidence remains in the existing research files.
