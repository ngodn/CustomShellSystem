# Coordinated upgrade audit cleanup, October 14

Removal list, written before deletion:

- `build/coordinated-upgrade-20261014/metadata`: superseded metadata extraction from the failed reader invocation. The original packages remain hash-pinned, and the second run extracted identical files.
- `build/coordinated-upgrade-20261014-2/metadata`: reproducible metadata extraction after the four import/catalog cases passed. Retain per-file hashes, package identities/versions and source container hashes; the test-case links still point to the original retained packages.

Keep all package trios, live installation, rollback, case mount links, package
inventories, import results, catalog results, requests, commands, failure logs
and the first recipe. Delete only the two listed metadata trees after their
relative filenames and file hashes match exactly. Reject symlinks in either
tree and recheck source container hashes before removing files.
