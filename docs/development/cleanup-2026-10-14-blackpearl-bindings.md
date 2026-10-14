# Black Pearl binding cook cleanup, October 14

Removal list recorded before deletion. All paths below are relative to
`work/three-outfit-rig-physics/eve-surface-20261014/blackpearl-ordered`.

- `binding-cook/out/prefix`: disposable Wine prefix. Cook and wineserver must
  have exited and the named `css-eve-blackpearl-binding-cook` container must be
  absent before removal.
- `binding-cook/project/Content`: duplicate cook inputs. Verify every file
  against the retained `editor/Content` source and recorded input hash first.
  Keep the bound normal/facial editor assets and all editor dependencies needed
  for final-path integration.
- `binding-cook/project/Saved/Cooked`: regenerable dependency cook and duplicate
  mesh payloads. The two shipping-eligible meshes are retained byte-identically
  under `binding-pack/stage`, with verified IoStore and independent readbacks.
  All other cooked dependencies are editor stand-ins and explicitly excluded
  from shipping. Their pinned sources and cook recipe remain.

Require `binding-pack/assembly-verified.json` to pass before removal. Keep input
and output hashes, commands, logs, DDC, project settings, source assets, two-mesh
package, readbacks and original accepted installation. Do not remove any other
work folders or backups. Record actual paths, byte counts and `df -h` readings
in `binding-cleanup-completed.json` beside the private candidate.

Completed at 08:25 UTC. The container was absent, all 46 staged source files
matched retained editor files and recorded hashes, and all four cooked mesh
payloads matched the retained package staging copies. Removed 2,040,966,091
logical file bytes. `df -h` before: root 95G, theark 12G. After: root 95G,
theark 13G. The detailed receipt records each removed group separately.
