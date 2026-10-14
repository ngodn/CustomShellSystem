# Alice candidate cleanup, October 14

This list covers only this session's new Alice candidate cook. Remove these
groups after a successful terminal cook, absent container and verified retained
source and package payloads:

- `build/cook-scratch/alice-ordered/out/prefix`: disposable Wine prefix on the root drive.
- `work/three-outfit-rig-physics/eve-surface-20261014/alice-ordered/cook/project/Content`: duplicate cook inputs, checked against retained editor sources.
- `work/three-outfit-rig-physics/eve-surface-20261014/alice-ordered/cook/project/Saved/Cooked`: regenerable output tree, checked against retained `pack/stage` payloads.

Keep the accepted interchange, reordered candidate, editor assets, DDC, logs,
packages, readbacks and pass/fail receipts. Keep `cook/out` linked to its root
scratch directory. The cleanup recipe checks this exact destination before
removing the prefix. Record `df -h` for root and theark before and after.
Other projects and unknown folders are not touched.
