# Release installation and live acceptance, build 26073152

The 22 coordinated releases are installed. All 95 payload files match their
release archives after the final game checks. CSS remains unpublished 1.0.2, CSSX is
1.3.1 and Traverse is 1.0.2. No public upload was performed.

Initial evidence: `work/release-acceptance-26073152/`. Final cloth, Cheat Menu
and timing evidence: `work/cloth-expanded-26073152/`. Its
`final-verification.json` checks 23 archives, installs 22 (the UUEPBS profile
is optional), compares 95 installed payloads and verifies restored settings. The optional UUEPBS profile was not installed
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

## Performance and known limits

The initial timing sampler responded correctly to a 20 FPS cap, but the
appearance sweep was too variable to establish an upgrade improvement. A final
Commander White ABBA run held camera, position, outfit and render settings
constant. Four plain-idle windows measured 49.8 to 59.9 engine Hz; four Iris Out
windows measured 41.9 to 51.4. Both groups drifted over time. These are engine
frame intervals, not GPU presentation FPS. The first follow-up used overlapping
rolling windows and was excluded; the corrected run waits fifteen seconds
before reading each separate eight-second window.

CSS 1.0.2 already includes the reduction from three reflected calls per changed
facial morph to one, with readback sampling every sixty updates. This is a code
optimization, not proof the FPS complaint is fully resolved. The final test
still shows extra cost during dance playback. No additional runtime change was
made from these measurements. See `work/cloth-expanded-26073152/performance/`.

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

The Genessa skirt correction is complete and included in the refreshed full
Genessa 0.3.1 and Shared Assets 1.0.2 ZIPs. Four normal/facial meshes pass the
five added controls, combined maximum shapes, menu reentry and reset. Iris Out
facial playback also passes on both variants. Original geometry, skinning,
morphs and cloth data remain intact. See
[expanded cloth evidence](2026-10-22-expanded-cloth.md).

Naughty Nun Modular Liturgy 2.0.0 visibly deforms incorrectly. The user confirmed
it is an unfinished local experiment; it is outside this release set. The
Phoenix and Slayer downloads remain uninstalled because their dependency audits
found missing materials/textures. Their separate reports retain the details.

## Final state

The game exited normally. CSS state, CSSX state and UUEPBS profiles match the
latest pre-install backup taken after the user's playtest. The two test-written
Cheat Menu preference files were restored after retaining their test copies.
Seven normal gameplay save/backup files changed during the final pass and were
retained. No native shade unlock was granted. The original Commander White
appearance and idle override were restored before exit. The temporary window
rule and the workspace Docker daemon started for cooking are stopped.

The compatibility and cloth updates are ready for publication within the
coverage above. Do not advertise an FPS fix or universal third-party
compatibility. Actual shade mission completion and missing third-party assets
remain limitations, rather than silently passed tests. No upload was performed.

## Published Cheat Menu 1.1.2

The user confirms 1.1.2 was already published. Its exact existing ZIP replaces
the outdated local 1.1.1 install. No code change or new release is required by
the checks performed here. Any later code fix must use 1.1.3.

God draft/discard/apply/restore, movement 1.5x, game speed 0.5x and cleanup pass.
Six numeric action parameters retain exact values; 79 pickups and 89 Tarstones
load from the updated catalogs. All 32 referenced controller functions are
available by reflection. Permanent grants/unlocks were not executed. The
Character page and numeric input field render without a menu error; this page
capture does not establish every keyboard entry path. The existing host suite
also passes. Evidence: `work/cloth-expanded-26073152/cheat/`.
