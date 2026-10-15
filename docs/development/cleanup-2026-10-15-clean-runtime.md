# Fresh-runtime test cleanup

Removal list recorded before deletion:

- `/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2/Binaries/Win64/CSS-clean-runtime-20261015/clean-trial`: disposable clean installation copied from the retained runtime ZIP;
  original runtime, settings and binary hashes have been restored and verified.
  Keep its input manifest, test request logs, clean state and playback result in
  `work/three-outfit-rig-physics/clean-runtime-install-20261015`.
- `work/three-outfit-rig-physics/clean-runtime-install-20261015/original-desktop.png`:
  temporary desktop overview used to inspect the startup wait. It contains no
  release evidence. Retain the game-only startup image and native wait log.

The test directory has 362 files, 746358394 bytes.
`postcheck.json` preserves their hashes. No source archive, recovery save,
accepted asset, live installation or original settings will be removed.

Removal completed after the per-file hash check. Root remained 93G available
and theark remained 49G available in rounded `df -h` readings. The runtime cache
was on the game drive, not either of those filesystems. The deletion receipt is
`work/three-outfit-rig-physics/clean-runtime-install-20261015/cleanup.json`.
