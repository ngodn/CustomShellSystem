# Stable release changelogs

These four BBCode files accompany the coordinated stable release approved by
the user on October 16:

- [CSS v1.0.0](css-v1.0.0.bbcode), including Shared Assets information in the same post.
- [Eve v1.5.0](eve-v1.5.0.bbcode).
- [Unholy Genessa v0.3.0](unholy-genessa-v0.3.0.bbcode).
- [Commander White v0.1.0](commander-white-v0.1.0.bbcode).

The delivery copies are colocated with their packages under `dist`, following
the existing layout. These tracked files remain the editable sources:

- `dist/v1.0.0/css-v1.0.0-changelog.bbcode` (CSS and Shared Assets).
- `dist/eve-v1.5.0/eve-v1.5.0-changelog.bbcode`.
- `dist/unholygenessa-v0.3.0/unholygenessa-v0.3.0-changelog.bbcode`.
- `dist/commanderwhite-v0.1.0/commanderwhite-v0.1.0-changelog.bbcode`.

The withdrawn October 12 Eve/beta.12 rehearsal has moved to
`dist/withdrawn/eve-v1.5.0-beta12-rehearsal`. It is not the current Eve delivery.
Future delivery updates must refresh the matching `dist` BBCode too.

New features appear first with actual game screenshots. Fixes and improvements
have no screenshots. Commander White adds belt physics, facial controls and
52 additional idles. Images are uploaded separately to the `media/css-v1.0.0` Git branch; code,
release tags and Nexus publication are not part of that upload.

All five stable release archives are under `dist`. The user accepted release on
October 16 and ended further validation. The release index is `dist/DELIVERY.md`;
remaining research is recorded in `docs/development/v1.0.1-follow-up.md`.

Media receipts live under
`work/three-outfit-rig-physics/changelog-media-20261015/`.
The corrected expression/shade screenshots and installed UI evidence live under
`work/three-outfit-rig-physics/shade-picker-20261015/`.

Latest media commit: `a62224b36ed5c33fcc55d0907a52ab6cb0cd6400`.
The existing image URLs remain pinned to their earlier immutable commit. The
sunlight image uses the new commit; its downloaded raw bytes match the capture.
Receipts are `changelog-media-20261015/upload-shade-ui.json` and
`changelog-media-20261015/upload-sunlight.json`. The sunlight capture preserves
the current CW appearance and exact saved settings, and closes the menu. It
does not toggle lighting, change outfits, reload code or send gameplay input.

## Current Eve delivery

`dist/eve-v1.5.0/CSS_EveStellarBlade_eins0fx_P-v1.5.0.zip` replaces the old
rehearsal bundle (631,596,096 bytes). SHA256:
`e810b764360f505ba4263ae714c68062bb93c161f8c9a415efd16660b5e09212`.
The current seven-outfit cooked containers match the accepted installation.
Only the manifest and catalog version fields change from 1.4.0 to 1.5.0.
Package validation, ZIP integrity and extracted-payload comparisons pass.
`verification.json`, the checksum, README, credits and current BBCode are
colocated in that folder. Game files were not changed.

The exact recipe, relocation record and retained versioned manifest are under
`work/three-outfit-rig-physics/stable-dist-20261015`. Disposable staging copies
were removed after verification. The initial cross-device hard-link failure
is retained in the log; the completed recipe uses a cross-device copy fallback.
The current Genessa, CW and combined Shared Assets candidates are also packaged
under dist. Their cooked data matches the accepted installation. The combined
catalog check passes 1,545 checks. The CSS runtime archive is now also staged at
`dist/v1.0.0/MSII-CSS-v1.0.0.zip`. Its shipping core matches the reviewed build,
its shared dependency hashes match the bundled Shared Assets trio, and all five
archive checksums pass. `dist/coordinated-v1.0.0.json` lists their exact paths.
This is a local candidate, not a published or final tagged release. The overall
goal remains open.

## Public-release comparison

Classify changes against CSS beta.11, Eve 1.4.0, Unholy Genessa 0.2.0 and
Commander White 0.0.6-dev. Unreleased trial corrections are not public bug fixes.
Facial Expression selection and synchronized facial playback are additions.

The current shared library has 60 idle choices. The released Eve 1.4.0 manifest
has eight: Eve Default and seven numbered Eve idles (702, 703, 704, 705, 706,
708, 713). Those eight remain in the current library, so the addition is exactly
52 idle animations. Public notes use the count, without naming example clips.

For the NPC addition, the user requests Merrick as the feature image. Show
Use NPC / Enemy selected, Merrick selected, and his face clearly in frame.
The first Merrick capture is rejected because it crops off his head.

Merrick now replaces Droeg in the CSS post, with Use NPC / Enemy and Merrick
selected. The final capture preserves the current Merrick appearance and saved
settings and closes CSS afterward. The first two framing retries stopped before
opening CSS after detecting world movement. No gameplay inputs were sent.

Merrick, CW expressions and the idle-animation list were uploaded in media commit
`1f777697ad6fb93f9cfbcf4867fa677cd7ea0eca`. All three raw URLs were downloaded and
matched to the captured bytes. Receipt: `upload-new-features.json`.
