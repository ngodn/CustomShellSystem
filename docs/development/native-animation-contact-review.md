# Native animation surface review, October 9

The complete 59-clip converted batch has been evaluated against four cooked
native meshes: Genessa, Harros, Gragu and `SK_ThornBoi` (the evidence directory
labels that fixture `sariel`). All 236 clip/mesh evaluations complete with finite
transforms and verified input checksums. Each mesh covers 20,059 sampled poses,
80,236 total. This is neutral skinning, not gameplay or contact acceptance.

Evidence: `work/three-outfit-rig-physics/native-contact1`.

## Method and limits

The extractor reads the original cooked render vertices and all eight skin
influences without weight normalization. Meshes contain respectively 87,730,
41,050, 57,979 and 148,738 vertices. Each mesh's actual bone subset, hierarchy
and translation retarget modes are applied to the independently decoded cooked
animation. Every referenced lower-leg vertex selected by the existing contact
probe is measured against Z=0, including all of its influences.

The selected vertices have at least half their weight on calf/foot/ball bones
and a bind height at or below 30 cm. This can include clothing, so the minimum
is not necessarily an anatomical foot. Bind minima range from -0.34 to -0.98 cm.
No game IK, animation graph, physics, cloth, material visibility, body morph or
component ground-placement offset is evaluated here.

Twelve neutral surface renders cover Seated Recline, Black Hole and Iris Out
on the four meshes. `pilot-surfaces.png` was inspected; the poses retain their
recognizable body silhouette. Native Genessa's unsupported cloth strands are
visible in the static skinning render, which does not simulate cloth. The image
does not establish continuous deformation or live cloth quality.

## Ground reference findings

Minimum selected-surface height in centimetres, before game corrections:

| Native mesh | Black Hole | Iris Out | Existing 20 gait sequences, worst sample |
| --- | ---: | ---: | ---: |
| Genessa | -4.34 | -8.49 | -7.79 |
| Harros | -3.65 | -9.04 | -9.68 |
| Gragu | -8.89 | -14.33 | -15.95 |
| ThornBoi | -4.94 | -10.50 | -10.79 |

The gait baseline uses the previously verified native locomotion cook. Its
twenty sequences cover the existing four gait choices and directional samples.
The new examples' offsets are comparable to that baseline. Do not raise roots,
alter body motion or claim a newly introduced gameplay defect from these
numbers alone. Larger offsets in kneeling and floor-level clips remain review
items. Existing user-accepted floor intersections and the separate After Brute
Battle crash deferral remain unchanged.

Next: compare these exact clips and frames in the native game animation graph,
including start/stop, action interruption and floor contact. The combined
217-package shared candidate remains uninstalled. Custom-rig animations and
installed outfit assets were not modified by this review.

## Diagnostic corrections

The first contact attempt rejected six signed-scale accessory tracks present in
the game's shared skeleton but absent from all four mesh bone sets. The probe
now checks scale validity after selecting the mesh's complete bone subset. It
still rejects unsupported signed/zero scale on a bone the mesh uses, non-finite
values, invalid quaternions, incomplete coverage and incompatible hierarchy.
A cooked-capture regression test fails before the fix and passes afterward;
its selected-accessory case still rejects negative scale.

The long Cinderella capture requires more than the old arbitrary transform-count
limit. The probe now bounds the actual binary payload to 1 GiB before mapping it,
matching the decoder's byte-based bound. It verifies dimensions, sample clock,
file length and checksum. All four complete 59-clip scans include Cinderella.

Eight retarget/parser tests and the visible-material contact test pass. The
first retry had an incorrect command argument index and did not run the probe;
its log is retained. The successful pilot, full-batch and gait-baseline commands
and exit records are separate. No game/runtime code changed for these checks.
