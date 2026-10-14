# Skin Suit and Bikini surface repair cleanup, October 14

Removal list recorded before deletion. All paths are under
`work/three-outfit-rig-physics/eve-surface-20261014`.

- `skinsuit-ordered/cook/out/prefix` and `bikini-ordered/cook/out/prefix`:
  disposable Wine prefixes after their respective cooks exit successfully,
  the owning Docker container is absent, and package/readback checks pass.
- The corresponding `cook/project/Content` trees: duplicate candidate inputs,
  only after exact comparison with retained editor files and cook input hashes.
- The corresponding `cook/project/Saved/Cooked` trees: regenerable dependency
  cooks and duplicate candidate payloads, only after each shipping payload
  matches the retained package staging tree.
- The same three groups under each `binding-cook`: apply the same rules after
  paired readback and binding/base-render preservation pass. The overall
  assembly check may still fail its tangent-length gate, provided that failed
  receipt and all diagnostic payloads remain. Retain the normal/facial cooked
  payloads under `binding-pack/stage`; these are still needed for the unresolved
  zero-tangent investigation.
- `bikini-ordered/editor/Content/CSSAuthoring/EveSurface20261014/SK_Content`:
  the wrongly named normal mesh from the failed first binding attempt. Remove
  after the correctly named pair passes save/reload and cooked preservation.
  Keep its path/hash inventory and the failed plan, command, exit and log.

Keep authored editor meshes, original sources, ordered interchange, useful DDC,
all recipes, logs, successful and failed receipts, independent readbacks and
verified containers. Dependency staging uses absolute links to retained sources.
Do not remove unrelated work. Record exact removed paths, byte totals and
`df -h` before/after in a cleanup receipt for each completed group.

The candidate and binding cook groups are complete. Removed 7,245,198,013
logical bytes in total, with exact retained payload and source comparisons.
Candidate cleanup changed root/theark free space from 94G/9.1G to 94G/12G.
After paired cooking/readback, binding cleanup changed it from 94G/7.7G to
94G/11G. Both overall assembly receipts remain failed on tangent length.
Each outfit retains `candidate-cleanup-completed.json` and
`binding-cleanup-completed.json` with the individual readings and file counts.
