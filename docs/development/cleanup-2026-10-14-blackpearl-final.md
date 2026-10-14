# Black Pearl final-path cleanup, October 14

Removal list recorded before deletion. Paths are under
`work/three-outfit-rig-physics/eve-surface-20261014`.

- `blackpearl-final/cook/out/prefix`: disposable Wine prefix, after terminal
  cook/readback success and absence of `css-eve-blackpearl-final-cook`.
- `blackpearl-final/cook/project/Content`: duplicate cook sources. Require exact
  equality with retained final editor inputs and recorded cook input hashes.
- `blackpearl-final/cook/project/Saved/Cooked`: regenerable dependency cook and
  duplicate final mesh payloads. Retain the exact two mesh packages under
  `blackpearl-final/pack/stage`, their verified container and independent readback.
- Staged editor dependency copies listed in `blackpearl-ordered/binding-plan.json`
  and `blackpearl-final/plan.json`: replace each byte-identical task-owned copy
  with an absolute symlink to its retained, hash-pinned source. This keeps the
  existing recipe paths working without keeping another 89 copies. Do not
  replace newly authored final meshes or the retained private bound mesh sources.

Only proceed after final `pack/verified.json` passes. Preserve original authoring
files, all created mesh sources, useful DDC, settings, scripts, commands, logs,
failed-run evidence, packages and readbacks. Leave unrelated folders untouched.
Record exact paths, logical bytes and `df -h` before/after in
`blackpearl-final/cleanup-completed.json`.

Completed at 08:43 UTC. Removed 1,879,657,382 logical bytes from the three listed
cook groups after confirming terminal success and an absent container. All 45
cook input files and four retained cooked payloads matched their references.
Replaced 89 duplicate editor copies (731,992,857 logical bytes) with verified
absolute symlinks. Immediate `df -h` before: root 95G, theark 11G. After: root
95G, theark 13G. The receipt lists every path and link target.
