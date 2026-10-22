# Release installation and live acceptance, build 26073152

The 21 coordinated releases are installed. All 86 payload files match their
release archives after the game checks. CSS remains unpublished 1.0.2, CSSX is
1.3.1 and Traverse is 1.0.2. No public upload was performed.

Evidence: `work/release-acceptance-26073152/`. `install-receipt.json` records
archive verification and installation; `final-verification.json` records the
post-test hashes and restoration. The optional UUEPBS profile was not installed
by this operation. The existing UUEPBS installation and profile were preserved.

## Live coverage

Tests ran through Steam on game build 26073152 with the actual release DLLs.
Game-only captures used the native screenshot command. Brief foreground input
was used for menu and movement checks after the user's authorization.

- All 75 installed outfit variants and 116 native/non-CSS appearances selected
  and loaded without reported runtime errors. This is load coverage, not complete
  animation acceptance. Two outfit cases belong to the unreleased Naughty Nun
  experiment and are excluded from the coordinated release verdict.
- The locked Sapphire Herald shade displays the fairness reminder and Wear
  anyway / Cancel dialog. Cancel preserves the look; acceptance applies the
  shade and survives menu reentry. All 24 native unlock flags remain unchanged
  (8 unlocked, 16 locked). Actual mission completion while wearing the locked
  shade was not exercised.
- Tarsus, Halberd Bro, Twin Sword Bro, Laughing Flayer, William and Ball Boy passed
  short walking checks with valid animation poses and measured displacement.
  William's cyan ghost and Ball Boy's second red ghost materials were reviewed.
- CSSX opens all four installed extension pages without an extension error.
  Traverse cancel preserves position. Meteor travel raises the pawn about
  2,200 cm and returns it visible and walking. Plain travel returns the pawn
  from an offset position to the Marrow Keep beacon. Cross-zone travel and
  every destination were not tested. The original meteor setting was restored.
- Commander White, Eve Peony and Unholy Genessa each expose 24 shape controls.
  Existing UUEPBS resolves both chest helper bones and all 24 profile morphs.
  Four motion configurations per outfit reach the active AnimBP and ControlRig,
  and solver steps advance. This validates settings transfer, not motion feel.

Screenshots, raw requests and case results are retained alongside the scripts.
The first movement attempt exceeded the short flat-lane displacement bounds;
its evidence is retained as `movement-too-long.*`. A bounded 0.4-second key hold
then passed all six cases. The first performance attempt used an invalid
expression identifier and was rejected; the corrected run is separate.

## Findings still open

The current performance run does not establish an FPS improvement. The sampler
responded correctly to a 20 FPS cap, but repeated appearance windows varied too
much for a reliable comparison. These measurements are engine frame intervals,
not GPU presentation FPS. See `performance-assessment.json`.

Motion limit contacts remain frequent on Commander White and Genessa. During
approximately one-second windows, aggregate contacts were:

| Outfit | Firm | Natural | Bouncy | Earthquake |
| --- | ---: | ---: | ---: | ---: |
| Commander White | 85 | 104 | 164 | 546 |
| Eve Peony | 0 | 0 | 0 | 10 |
| Unholy Genessa | 100 | 125 | 234 | 646 |

These are aggregate counters across helpers, not contact percentages or a
matched animation comparison. After the game was reopened for their playtest,
the user reported that the installed motion "seems smooth" and directed us to
continue. This accepts the feel of their test; the exact outfits and presets
they exercised were not specified. Do not retune solely because of these counters.

Additional Genessa skirt morph buffers and the matching deformation graph remain
under validation. The installed releases do not yet contain that private fix.

Naughty Nun Modular Liturgy 2.0.0 visibly deforms incorrectly. The user confirmed
it is an unfinished local experiment; it is outside this release set. The
Phoenix and Slayer downloads remain uninstalled because their dependency audits
found missing materials/textures. Their separate reports retain the details.

## Final state

The game exited normally. CSS state, CSSX state and UUEPBS profiles match the
pre-install backup. One Traverse backup settings file changed during testing
and was restored after retaining its test copy. Normal gameplay autosaves
changed eight save/backup files; they were preserved, not rolled back. No native
shade unlock was granted by the tests. The original Commander White appearance
was restored before exit.

The compatibility checks above pass within their stated scope. This is not an
unqualified release approval while the requested performance and cloth
follow-ups remain open.
