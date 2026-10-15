# CW source replay cleanup plan

Prepared before deletion on October 15, 2026.

- `work/three-outfit-rig-physics/cw-baseline-sources-20261015-pass5/texture-replay/`: remove generated texture payloads only after their complete path/hash table matches the retained accepted texture stage. These are duplicate relocation outputs, not authoring sources.
- `work/three-outfit-rig-physics/cw-baseline-sources-20261015-pass5/graph-replay/`: remove generated B2/B3 graph payloads only after their complete path/hash table matches the retained accepted compiled graph stage.

Keep the replay requests, logs, exit records, verification, recipes and a file-by-file cleanup receipt. Keep every original input, accepted namespace stage, cooked baseline, installed package and rollback. Earlier attempts contain only small audit scripts and failure records and stay available.

Record `df -h / /run/media/eins0fx/theark` immediately before and after removal. Logical bytes removed are separate from measured free-space recovery.
