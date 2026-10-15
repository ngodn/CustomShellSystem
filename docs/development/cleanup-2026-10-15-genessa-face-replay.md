# Genessa facial packet replay cleanup

Removal list recorded before deletion. Only the two new duplicate packets from
this session are eligible:

- `work/three-outfit-rig-physics/genessa-face-source-replay-20261015/replay/SK_EveW3.json`: regenerated packet matches the retained accepted input byte-for-byte; keep its recipe, output digest and verification.
- `work/three-outfit-rig-physics/genessa-face-source-replay-20261015/replay/SK_GenessaW3.json`: same verified duplicate status for the second variant.

The completed replay and cooked comparison exited zero. Keep all original W3
meshes, patch inputs, module sources/binary, editor captures, current installed
packages, archived decoded meshes, and the new small reports/recipes/logs.
No other directory or another session's file is eligible.

Before: `df -h` reports 96G available on `/` and 49G on theark. Actual removal
and after readings are recorded below only after deletion.

Completed: exactly those two files were removed, 36,022,334 logical bytes.
`genessa-face-source-replay-20261015/cleanup.json` records their hashes, retained
originals and before/after `df -h`. After: 96G available on `/`, 49G on theark.
Rounded free-space readings do not establish physical bytes reclaimed.
