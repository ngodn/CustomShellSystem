# Eve tangent bake cleanup, October 14

Scope: only files produced by this session under
`work/three-outfit-rig-physics/eve-surface-20261014/tangent-bake`.
The work directory resolves through the existing theark symlink.

Removal groups, listed before deletion:

- Attempt 1's four `SK_*TangentBaked.uasset` outputs: rejected because unit tangent lengths did not establish correct UV direction or handedness. Remove only after attempt 2 saves and reloads successfully. Retain attempt 1's receipts, hashes, logs and archived commandlet source/module.
- Attempt 2's two regular `SK_*NormalTangentBaked2.uasset` outputs: superseded because rebuilding reset their imported sphere radii. Remove after attempt 3 passes full cooked validation, retaining the failed decoded bounds, receipts and archived source/module. Keep the two accepted attempt 2 facial sources.
- The completed cook's disposable Wine prefix: regenerable runtime setup, after a terminal cook result and confirmation that its container has stopped.
- The completed cook's copied project Content: byte-identical duplicates of retained editor sources, checked against input hashes before deletion.
- The completed cook's Saved/Cooked tree: regenerable intermediates, only after successful packaging/readback and byte comparison of all four shipping meshes against retained package staging.
- `cook/out/ddc`: cache belonging to the superseded bounds trial; the original baseline cache and corrected `cook3/out/ddc` remain.
- `editor/Intermediate`: completed native build intermediates, including its PCH; retain source, build receipts and the verified compiled module.

The cook cleanup groups apply to both `cook` and `cook3`. For the first cook,
retain its failed assembly result and packaged payloads; remove its duplicate
cooked tree only after the corrected `pack3` checks pass and its own four
shipping payloads match the retained first-attempt package staging.

Keep accepted installed assets, candidate editor sources, useful DDC, packaged
payloads, independent readbacks, commands, logs, failed checks and reproduction
recipes. Do not remove any other session's files or unknown folders.

Before this trial, `df -h` reported root 94G and theark 11G available.
Record actual removals, byte counts and immediate before/after disk readings in
`tangent-bake/cleanup-completed.json` and append the result here.

Completed after the corrected four-mesh readback passed:

- Rejected attempt 1 sources: 162,187,446 logical bytes.
- First cook's stopped Wine prefix: 1,084,763,593 logical bytes.
- Verified duplicate cook inputs/outputs, the second stopped prefix and two superseded regular sources: 3,804,118,222 logical bytes.
- Superseded cook cache and native build intermediates: 705,582,639 logical bytes.

Total removed: 5,756,651,900 logical file bytes. Receipts are
`rejected-cleanup-completed.json`, `cook-prefix-cleanup-completed.json`,
`cleanup-completed.json` and `cache-cleanup-completed.json` under the trial.
The main cleanup's immediate `df -h` readings were root/theark 94G/7.4G before
and 94G/8.5G after. Later readings before/after the cache cleanup both read
94G/9.4G. The entire trial began at 94G/11G and ends at 94G/9.4G with new source,
package and verification artifacts retained. Logical byte totals do not imply
the same physical-space change. No unrelated or unknown folders were removed.
