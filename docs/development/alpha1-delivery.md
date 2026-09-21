# CSS 1.0.0-alpha.1 local delivery

The user accepted corrected Eve Walk/Jog/Sprint on 2026-09-21: "yeah its good
now". The alpha includes that correction. The earlier proposal to list sideways
instability as an accepted known issue is rejected and superseded.

Source tag: v1.0.0-alpha.1, annotated, commit
84cb7ee11868ed8eb2a23b8875d6ce7ad25fc550. The Windows Release loader and core
were built from the clean tagged worktree with developer probes and transition
tests disabled. No CSSX, user state, catalog, game saves or source assets ship.

Local downloads in dist/v1.0.0-alpha.1:

- MSII-CSS-v1.0.0-alpha.1.zip
- CSS_EveStellarBlade_eins0fx_P-v1.0.0-alpha.1.zip
- Eve-README.txt, release-notes.md, verification.json and SHA-256 sidecars

Runtime SHA-256:
21ef866b9f755b9ccb56c4f4a3997c11c4d5c9bb5bdc6e02acf1e79f119d82d0

Eve SHA-256:
bbe4bce4ba2bc9ba7d9ab263c454180a591740da4e7a46eccff527759989f8c7

Verification: Windows production build exited zero; eight release tests passed.
A fresh host build of the tagged source passed all 15 tests. The extracted
runtime with the exact Eve package passed 53 startup/discovery/cache/recovery
checks. Both ZIPs pass CRC, allowlist, checksums and complete byte readback; all
member paths are below 140 characters. The old primary host build was missing
a test executable, so its incomplete CTest result was replaced by the fresh
tagged build, not treated as success.

The Eve trio is exactly the installed and accepted S1 package. Gameplay review
used movement2, which contains the same movement, idle and UI fixes. The exact
production DLL has not had its final in-game boot check. Do that with a normal
restart before public publication; do not use DLL hot reload. These artifacts
are local downloads, not a published GitHub release.

Custom Eve idle, weapon hiding and beacon playback remain unfinished, as do
broad weapon/combat/transition coverage and the modder kit. Full CSS v1.0.0
work remains active. Preserve accepted rig, proportions, physics and ground
contact while continuing. See eve-movement-delivery.md for S1 evidence.
