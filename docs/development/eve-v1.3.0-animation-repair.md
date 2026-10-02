# Eve v1.3.0 animation repair candidate

The authoring work is in the existing `CSS_SeduXtress_eins0fx` directory:
`authoring/animation-update.md`, `authoring/animation-update-check.json`, and
`tools/{audit,build,check,install}_animation_update.py`.

The installed Eve package before this update matched the accepted v1.2.0-ANIMTEST release.
Its 19 movement animations have the same incorrect virtual-bone mappings fixed
in Genessa. Their original and repaired export hashes match Genessa's verified
repair. The new candidate preserves every other retained export/bulk payload
and all six outfits' customization, and keeps Eve Default Idle plus seven CSS
idles. Seven unselected experimental idle packages are removed.

The v1.3.0 ZIP is verified and installed, with public release pending. SHA-256:
`8ff28249d26e4507f34ab01e5139a231cd8e9499fd9038bc6c40cef1a8be49b2`.
After the user confirmed shutdown, the exact ZIP was installed with matching
hashes and unchanged saved profiles. The authoring project records the backup
in `authoring/animation-update-install-check.json`. The user has been asked to
launch the game and select Eve. CSS beta.8 remains installed.
No new CSS runtime change is needed: missing saved animation choices already
fall back to Default and remain visible as unavailable in the selector.

Retain the eight-idle scope. The earlier Eve work note incorrectly called for
removing Eve Default Idle; that note has now been corrected to the user's final
instruction. Per-outfit gameplay review remains necessary before release.
