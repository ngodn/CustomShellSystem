# Current normal and facial mesh preservation

The October 14 installed inventory now has a paired comparison for all ten
appearances. Every facial mesh preserves its normal mesh's bind reference and
all existing morph deltas. Eve's seven pairs and Commander White also have
identical decoded render digests: positions, normals, UVs, skin influences,
indices, colors, section metadata, cloth mappings and recorded attribute buffers.

All ten pairs preserve their Skeleton and Physics Asset references. Eve and
Genessa preserve the post-process graph reference. CW uses the separately
validated `ABP_NormalBelt20` / `ABP_FacialBelt20` pair. Eve adds 115 morphs per
appearance, Genessa adds 115 per variant, and CW adds 190.

The comparison includes decoded clothing exports and solver configurations:
one clothing asset each for Planet Diving, Knitwear, Alice and CW, plus six for
each Genessa variant. The other four appearances have none. All sixteen
clothing assets and their associated decoded configurations match their facial
counterparts after resolving local object references by class, name and outer.
Numeric export indices cannot be compared directly because the facial morphs
shift the mesh's index. Invalid local indices and outer cycles are rejected.
This covers serialized fields visible to the reader, not omitted native fields
or simulation behavior.

Material comparisons preserve every Eve slot. Genessa preserves its original
25 slots and appends `UG_Mouth`. CW changes only the accepted optical materials
in slots 8, 10 and 45; the other slot data is identical.

## Genessa's authored additions

Genessa intentionally differs in neutral render topology: seven lip-seam
duplicates and 2,138 oral vertices bring each facial mesh from 160,794 to 162,939
vertices. Both current installed facial meshes were independently decoded and
checked against the retained `face-integration6` editor captures using the
existing `verify_face_cook.py` recipe. Positions, UV0, section order and indices
match exactly across the whole mesh. All 137 morph index sets match. Their
position and tangent deltas remain within the stored compression precision;
largest component differences are 0.002000094 cm and 0.000244141 respectively.
All pinned installed files and editor captures remained unchanged.

## Receipts and limits

- [Paired comparison](../../work/three-outfit-rig-physics/current-inventory-20261014/facial-preservation.json),
  produced by [check_current_facial_preservation.py](../../tools/diagnostics/check_current_facial_preservation.py).
- [Negative checks](../../work/three-outfit-rig-physics/current-inventory-20261014/facial-preservation-negative.json):
  in-memory changes to an original morph, cloth reference bone, body material
  and local export reference each fail for the expected reason.
- [Current Genessa cook comparison](../../work/three-outfit-rig-physics/genessa-current-facial-cook1/comparison.json)
  and [input-preservation receipt](../../work/three-outfit-rig-physics/genessa-current-facial-cook1/verified.json).
  Commands, logs, reader/mapping hashes and raw readbacks are retained beside
  them. The historical comparison tool writes `installed: false` because it
  performs no installation; the wrapper receipt identifies these inputs as
  reads of the current installed package.

These results extend the six original Eve import comparisons to their facial
meshes for the fields shared by both audits. They do not establish new facial
visual acceptance, bounds/Nanite equivalence, full Blender provenance, graph
execution, cloth motion, lifecycle, doubles, CINE or performance coverage.
Nothing was cooked or installed during these checks.
