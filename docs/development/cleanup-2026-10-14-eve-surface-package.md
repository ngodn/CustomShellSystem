# Combined Eve package cleanup, October 14

Only the new `build/eve-surface-package1` staging tree is included. Its work
link is `work/three-outfit-rig-physics/eve-surface-package1`.

After package, catalog, import, roundtrip and all twelve decoded mesh checks
pass, remove these verified duplicates:

- `stage`: 1,111 staged payload files, after checking every retained source against the preparation plan and retaining the new package trio.
- `roundtrip`: regenerable container readback, after checking every export/bulk hash and package identity against the plan and retaining decoded mesh comparisons.
- `metadata-readback`: duplicate extracted metadata, after exact comparison with retained `metadata` and the packed files.

Keep original/repaired editor assets, original and edited metadata, the new
package trios, full source plan, commands, logs, readback digests, failed-step
evidence and recipes. Verify no assembly/conversion/readback job is active.
Record retained hashes and `df -h` for root/theark before and after removal.
No other project, installed file, backup or unknown folder is included.
