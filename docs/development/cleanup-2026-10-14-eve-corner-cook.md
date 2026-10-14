# Eve corner-order cook cleanup, October 14

Removal list recorded before deletion:

- `work/three-outfit-rig-physics/eve-surface-20261014/blackpearl-ordered/cook/out/prefix`:
  disposable Wine prefix copied from the validated cook image. Remove only after
  the cook command and wineserver exit, its named container is absent, and the
  private package/readback checks finish. Keep the image identity, recipe, logs,
  source assets, useful DDC, cooked outputs and verification evidence.

No source assets, accepted installations, rollback packages or other sessions'
files are included. Record the exact removed path, logical size and free space
before and after in the private candidate's cleanup receipt.

Completed at 08:05 UTC after cook exit zero, independent package/readback checks
passed, and the named Docker container was confirmed absent. Removed
1,084,763,594 logical file bytes. The immediate `df -h` readings were root 95G /
theark 13G both before and after. No equivalent physical-space recovery is
claimed from those rounded readings. Receipt:
`work/three-outfit-rig-physics/eve-surface-20261014/blackpearl-ordered/cleanup-completed.json`.
