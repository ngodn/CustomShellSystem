# Skin Suit and Bikini final-path cleanup, October 14

Scope: this session's `work/three-outfit-rig-physics/eve-surface-20261014/eve-tangent-final`.
The work directory already resolves to theark. Do not move it again.

Prelisted removals, after the four permanent meshes pass independent cooked
comparison with the verified private meshes:

- `cook/out/prefix`: disposable Wine environment, after terminal cook success and confirmation that its container is absent.
- `cook/project/Content`: staged duplicates, after every file matches its retained editor source and recorded input hash.
- `cook/project/Saved/Cooked`: regenerable cook outputs, after all shipping mesh payloads match retained `pack/stage` files.
- `editor/Content/CSS/AnimLab/SK_Face_FinalPathCheck*create.uasset` and `*reload.uasset`: diagnostic clones created only for the preservation verifier. Keep their comparison results and require that none is a dependency of a permanent mesh.

Keep all four final editor sources, private input symlinks, useful DDC, compiled
module, recipes, logs, packages, dependency fixture, roundtrip and readback
evidence. No unrelated or unknown folders are included.

Before staging, root/theark free space was 94G/9.4G. Record immediate `df -h`
readings and actual removals in `eve-tangent-final/cleanup-completed.json`.

Completed after exact four-mesh cooked comparison passed: the stopped Wine
prefix, duplicate cook Content and regenerable Saved/Cooked tree were removed,
totaling 2,131,005,002 logical bytes. The diagnostic clones were not saved as
files, so none required deletion. All retained final sources and packaged
payloads still match their hashes. Immediate `df -h` readings were root/theark
94G/7.9G before and 94G/8.8G after. Sources, useful caches and evidence remain;
no unknown or unrelated folders were touched.
