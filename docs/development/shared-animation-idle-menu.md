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

The installed menu correction uses the existing idle branch, which releases
for movement, montages, sidearm aim, menus, ungrounded movement and player
replacement. Body selection alone does not prove facial playback acceptance.

## Automatic facial playback candidate

The follow-up runtime selects the verified facial mesh when the saved idle
has a facial track. The user's expression remains the baseline. Body-only
clips and unsupported faces retain their existing behavior. Changing the idle
back to a body-only clip restores the original mesh unless an expression still
needs the facial adapter.

Once the existing idle gate settles, compatible facial clips use the existing
explicit evaluator and morph sampler with one game-time clock. CSS repeats
the selected clip as an idle; CINE keeps the catalog's Once/Pose/Loop mode.
The evaluator is before the preserved secondary-motion nodes. There is no new
menu, Play button, authored asset change or outfit recook.

Automatic playback owns its session token and selection context. Movement,
aiming, attacks, menus and traversal use the existing playback interruption
checks. Outfit, pawn, catalog and idle-choice changes cancel automatic
ownership. Starting a CINE scene releases the automatic session first. A failed
facial binding leaves body idle available and cannot retry every update against
the same player, mesh, catalog and selection.

Host coverage includes all three authored playback modes as repeating idles,
unchanged CINE one-shot behavior, interruption and expression-baseline sampling,
frame-step equivalence at 30/60/144 FPS, token ownership and failure retry gates.
Nine related test suites pass and the Windows Release core builds. Live facial
sync, interruptions, physics continuity and performance remain unverified.
The candidate is separate from the currently installed menu-fix build.

The user rejected the added ESAP author credit in public-facing notes. It has
been removed. Internal source evidence remains in the existing research files.
