# Identical Eve cook-cache cleanup, October 14

Keep the canonical cache at
`work/three-outfit-rig-physics/eve-surface-20261014/knit-ordered/cook/out/ddc`.
The completed-cook audit found every file in these six older caches identical
at the same relative path in that retained cache:

- `skinsuit-ordered/cook/out/ddc`: duplicate first Skin Suit cook cache.
- `skinsuit-ordered/binding-cook/out/ddc`: duplicate Skin Suit binding cache.
- `tangent-bake/cook3/out/ddc`: duplicate accepted tangent cook cache.
- `eve-tangent-final/cook/out/ddc`: duplicate final Skin Suit/Bikini cache.
- `cloth-transfer/planet-cook/out/ddc`: duplicate private Planet pair cache.
- `planet-cloth-final/cook/out/ddc`: duplicate permanent Planet pair cache.

All paths above are beneath `eve-surface-20261014`. Before deleting each old
directory, require no active Eve cook containers, recheck every file against
the canonical cache, rename the old directory aside, install an absolute
symlink at its original path, and verify that all original files still resolve
with the same hashes. Only then remove the renamed duplicate. Record every
retained file hash, removal group and `df -h` before/after. Keep the canonical
cache while any of these links remain. Existing cook recipes can still copy
from their original cache paths.

The audited Black Pearl caches, Bikini caches and first Planet cache are not
identical subsets and will not be touched. All other folders are outside this
cleanup, unassessed and untouched. Sources, editor modules, packages, readbacks,
release artifacts and pass/fail receipts are not removal candidates.
