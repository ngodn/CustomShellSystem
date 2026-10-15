# Genessa material replay cleanup, October 15

Only the new `work/three-outfit-rig-physics/genessa-material-rig-sources-20261015/stage/`
is eligible for removal after its replay passes. It contains duplicate migrated
material-instance payloads reproduced from retained cooked originals. Verify
every file against the accepted package, record its hash and size, then remove
this exact stage. Keep the request, writer report, commands, exit status, source
audit and cleanup receipt. Original sources, cooks, accepted stages, installed
files and rollback are excluded. Record `df -h` before and after; logical bytes
removed do not by themselves prove physical space reclaimed.
