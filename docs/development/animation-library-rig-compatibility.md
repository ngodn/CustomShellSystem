# Shared animation rig checks

2026-10-06. Readback of installed packages, each mounted separately with the
base game. The package verifier checked the actual containers before readback.
No installed asset was changed.

| Package | Version | Body meshes | Skeleton | Post-process |
| --- | --- | --- | --- | --- |
| Eve | 1.4.0 | BlackPearl2, PFit14, SFit16 | Shared/SKEL_Base | ABP_Secondary |
| Eve | 1.4.0 | BFit1 | Shared/SKEL_Base | ABP_BikiniFeet2 |
| Eve | 1.4.0 | KCloth4, ACloth3 | Shared/SKEL_Base | ABP_KnitFeet1 |
| Unholy Genessa | 0.2.0 | EveW3, GenessaW3 | UnholyGenessa/SKEL_Wings2 | ABP_Wings2 |
| Commander White | 0.0.6-dev | CommanderC4 | Shared/SKEL_Base | ABP_BodyHairB4 |

The installed Genessa meshes are W3, not the W2 meshes named in earlier notes.
They retain the Wings2 skeleton and post-process blueprint.

All nine mesh bindings pass the static check. Their skeletons share an exactly
equal 386-bone prefix, including parent indices and every serialized reference
transform. All nine virtual-bone definitions also match. Genessa appends four
wing bones to make 390; each has its own additive rotation node in ABP_Wings2.
Commander White and Eve have 386 bones in these skeleton readbacks.

Every bound post-process class targets its mesh's skeleton and declares both
`CSSIdleSequence` (object) and `CSSIdleEnabled` (boolean). Each retains its own
body and hair control rigs. Eve's footwear-specific graphs and Genessa's wing
graph must remain active when shared animations play. A shared body clip does
not authorize replacing any of these graphs with the Black Pearl graph.

This supports the existing directed Base-to-Wings2 compatibility rule. It does
not establish skinning quality or live compatibility for every new clip.
Cooked playback, contacts, footwear, hands, hair, wings and cloth still need
the goal's outfit-by-outfit checks. Old bundled specialized locomotion must
remain available unless its shared replacement preserves that behavior.

Reproduce from isolated AssetReadback exports:

```sh
python3 tools/authoring-probes/animations/audit_outfit_rigs.py \
  work/cine-animation-port/outfit-compatibility \
  work/cine-animation-port/outfit-compatibility/report.json
```

The output is created exclusively, so use a fresh filename for another run.
Its scope deliberately excludes mesh skinning and live physics. The retained
manifest, meshes.json and dependencies.json files identify the exact evidence
for each package. The report records reference digests, graph bindings,
physics assets and control-rig dependencies.
