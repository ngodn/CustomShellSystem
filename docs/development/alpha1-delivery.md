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
production DLL passed its normal-restart boot check, with installed files
matching every release manifest hash. Saved state is identical before and
after restart/menu entry. Runtime and loader agree on the new Windows PID.
CSSX remains disabled. Publication details follow once the upload is verified.

Custom Eve idle, weapon hiding and beacon playback remain unfinished, as do
broad weapon/combat/transition coverage and the modder kit. Full CSS v1.0.0
work remains active. Preserve accepted rig, proportions, physics and ground
contact while continuing. See eve-movement-delivery.md for S1 evidence.


## Exact production build in game

The user reopened CSS after the normal restart. production-menu.mp4 and its
reviewed frame show Eve (Stellar Blade), Black Pearl equipped. Saved choices
remain walk/jog/sprint=eve, idle=css.feminine and beacon=original.

The first 30-second capture includes a beacon travel cinematic, not a clean
three-gait comparison. All 59 status samples are error-free. At the destination,
Eve is restored on the new player pawn, recovery is clear and feminine idle
resumes. The following 20-second destination recording contains gameplay and
attacks; 39 samples show custom jog, custom sprint, idle and released override,
without animation or maintenance errors. Sampled frames were inspected.
Walk acceptance remains the preceding user-reviewed S1 recording; it was not
observed again in these short production captures. Do not describe these as
exhaustive combat or three-gait production validation.

Evidence: work/alpha1/live/deployment.json, loader.json, menu-status.json,
state-in-menu.json, production-menu.mp4, production-world.mp4,
world-observation.json, after-world-status.json, destination.mp4 and
destination-observation.json. Game PID remained live after recording.
