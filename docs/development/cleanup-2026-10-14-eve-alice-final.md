# Permanent Alice pair cleanup, October 14

After final-path reload, terminal cook, exact independent readback and absent
cook container are verified, remove only these groups:

- `build/cook-scratch/alice-final/out/prefix`: disposable Wine prefix.
- `build/cook-scratch/alice-final/project/Content`: verified duplicate cook inputs whose originals remain in the editor project.
- `build/cook-scratch/alice-final/project/Saved/Cooked`: regenerable cooked tree with exact shipping payloads retained in `alice-cloth-final/pack/stage`.
- `alice-cloth-final/editor/Content/CSS/AnimLab/SK_Face_FinalPathCheck*{create,reload}.uasset`: only the four specifically named comparison clones, if present and unreferenced by the final pair.

The first three paths are on root; the final project remains under
`work/three-outfit-rig-physics/eve-surface-20261014`. Keep its `cook/out` and
`cook/project` links, source assets, DDC, packages, readbacks and receipts.
Check exact scratch destinations and retained hashes, and record root/theark
`df -h` before and after. No other project or unknown directory is included.
