# Commander White facial library checkpoint

October 7, 2026. Private candidate, not installed or accepted in game.

The combined shared trial is `work/cine-animation-port/cw-face-trial-v1`, a
symlink to secondary-drive scratch storage. It retains all 62 body/movement
identities, the four native locomotion alternatives and five outfit animation
adapters. The 95 previous exports remain unchanged. One additional facial mesh
brings the package to 96 exports and two facial rigs.

Commander White adds eight expressions (Afraid, Angry, Flirting, Frown, Shock,
Smile, Open Smile and Surprised) plus Neutral. These use her source controls,
not Eve's facial deltas. The 74 new morphs comprise 44 control samples and 30
compressed corrections. All 16 original body/shoulder morphs remain intact.
Static expressions use inverse storage gains so Unreal's existing morph filter
does not erase their small corrections. Her performance curves remain pending.

## Cooked evidence

`cw-expression-cook-v3` has successful Windows cook, container conversion and
independent CUE4Parse readback receipts. The source comparison checks the
released Commander White package, not only another copy of the new candidate.
It verifies:

- All 2,312,092 rendered vertices, normals, UVs, skin weights and topology.
- Original skeleton, physics, post-process graph, material slots and sockets.
- Render sections, active/required bones and cloth index mappings.
- All 16 original cooked morphs and all 74 added facial morphs.
- 25,405 mapped facial render vertices against their sparse source positions,
  with zero neutral-position difference.
- All eight expressions plus Neutral. The largest decoded deformation
  difference is 0.0030196444 cm (Surprised), within its measured shape bound.

The extended decoder also passes the existing six Eve faces and their source
comparisons (`cw-expression-map-v2/eve-regression-readback.json`, exit 0).
The Commander White rig passes 765 host checks. Combined and pack-only catalog
checks pass 1,127 and 1,115 assertions, respectively. The three focused host
facial/library suites pass. This proves offline data and catalog behavior,
not live facial appearance, gameplay restoration or runtime performance.

## Material repair

The first cooked comparison correctly rejected E3: 37 hair-material references
were empty. The isolated authoring process had mounted CommanderWhite and
CWTest but omitted the original CWMaterial/CXMaterial folders. An editor
comparison against that same incompletely loaded source could not detect the
loss. The released cooked package exposed it.

E4 loads those original folders read-only, verifies all 70 released material
identities and restores them on a new derived mesh. Both fresh creation and
reload pass in `cw-expression-shared-v3`. E4 then passes the complete cooked
comparison against the released package. Original materials and source assets
are unchanged. Neither E3 nor E4 has been installed in the game.

## Remaining work

The full goal remains active. Commander White's facial performance mappings,
the three unresolved Eve facial tracks, seven unknown source-bone mappings,
feasible Unholy Genessa facial coverage, final live checks across the three
requested outfits, native locomotion acceptance and release delivery remain.
No old Seductress, GeneXXa or native dance expansion is authorized.

Build tooling remains .NET 10.0.401, UE 5.6.1 C++/Python and host Python 3.14.
The shared candidate can be reproduced with `package_cw_facial_trial.py` after
the hashed cook/readback inputs pass. Runtime CSS and CINE continue to consume
the same facial catalog. No runtime DLL changed in this checkpoint.
