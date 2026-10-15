# Genessa Blender replay cleanup

Removal list recorded before deletion. Both new replay jobs are terminal and
verified. The chain's twelve corruption controls pass. No installed or original
authoring file is eligible.

- `work/three-outfit-rig-physics/genessa-blender-patch-replay-20261015/export/head-patch.json`: exact duplicate of retained `face-patch1/head-patch.json`; keep the command, recipe, report and verified hash.
- Under `work/three-outfit-rig-physics/genessa-face-chain-replay-20261015/`, `surface/surfaces.json`, `transfer/morphs.json` and `articulation/morphs.json`: reproduced byte-for-byte from the retained source/recipe chain; original accepted payloads remain in the authoring project.
- In that chain directory, `backing/backing.json` and `export/head-patch.json`: verified identical geometry/morph data with only the explicitly checked generated-preview provenance changed. Keep provenance, output hashes, comparison-control results and the generated articulation preview used by that provenance.
- In that chain directory, `integration/SK_EveW3.json` and `integration/SK_GenessaW3.json`: exact duplicates of the accepted W3 packets, which remain in `face-integration-input3`.
- PNG files directly in the chain's `surface`, `transfer`, `articulation` and `backing` directories: scratch renders emitted by the unchanged recipes; no new visual acceptance relies on them.
- `transfer/preview.blend` in that chain: unused intermediate preview. The following articulation stage consumes the reproduced morph data; keep `articulation/preview.blend`, which the backing/export stages actually use.

Keep all logs, reports, commands, original and resumed recipes, the failed first
comparison receipt, generated mapping, corruption-control recipe, current accepted
inputs and installed packages. No recursive directory deletion is authorized.

Before: `/` has 96G and theark has 49G available in `df -h`. Record actual deleted
paths, hashes, bytes and after readings in the replay's `cleanup.json`.

Completed: 50 listed files removed, totaling 102,725,111 logical bytes. The exact
pre-deletion list is `genessa-face-chain-replay-20261015/cleanup-plan.json`;
`cleanup.json` records the completed deletions. The articulation preview remains.
After: `/` has 96G and theark has 49G available in rounded `df -h` output. These
readings do not establish physical bytes reclaimed or account for other activity.
