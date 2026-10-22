# Expanded Genessa cloth channels

The full Genessa 0.3.1 and Shared Assets 1.0.2 updates are installed and packaged.
Evidence lives in
`work/cloth-expanded-26073152/`.

The current Genessa source meshes contain the new garment morphs, but the shared
cloth graph and meshes only carry the five original correction channels. The
baseline probe reproduces the missing buffers on all four step-6 meshes.

The candidate adds GlutesWidth, GlutesUpDown, HipBoneSize, ThighsSize and CalvesSize
as separate XYZ scalar attributes. It retains the existing fifteen buffers and
adds fifteen more. A separate ten-channel graph preserves the original five-channel graph for
older Genessa revisions. CSS already binds matching
`CSS_Morph_` variables, so no runtime core change is needed for these channels.

Creation and fresh reload pass. All four meshes preserve render geometry,
existing morphs, skin weights, cloth mappings and cloth asset data. Both regular
meshes retain 40 morphs and both facial meshes retain 155. All thirty render
buffers reload identically. Source asset hashes remain unchanged.

Two probe corrections were required. Genessa has shared source vertices across
the skirt boundary; their attributes are duplicated into ordinary render
sections. The graph's cloth-only shader condition prevents double deformation.
The checker now compares nonzero boundary values with the actual morph instead
of requiring zero. Separately, UE 5.6.1 does not serialize the imported-model
attribute cache. All eight original/candidate meshes show that cache empty on
fresh load, while their serialized render buffers remain valid. The reload gate
therefore compares all serialized buffer values/digests, with imported-cache
validation at build time. Failed attempts and storage evidence are retained.

GPU comparison passes: 64 captures form 32 stock/candidate pairs across both
facial meshes. All five added channels produce a nonzero correction. Non-cloth
positions are unchanged, and the worst residual against the independently
exported render morphs is 0.0283602 cm (limit 0.05 cm). Four negative checks reject
the uncorrected output for original glute size and new glute lift. Evidence:
`gpu-verified.json`, `gpu-result.json` and `captures/` in the private stage.
This is held reference-pose correction, not animated contact or gameplay proof.

Windows cook passes for all six packages, compiling both SM5 and SM6 kernels.
Independent readback preserves all existing geometry, morphs, skinning, cloth
and the original fifteen attribute buffers. The only semantic changes are the
additional buffers and two graph references. The comparison normalizes the
reader's `LODData`/`LodData` spelling and six qualified reduction enum names;
their actual values remain identical. Dependency and shader checks pass for
the assembled full packages. All other package payloads remain unchanged.

Live checks pass for all four normal/facial meshes: the ten-variable graph and
thirty attribute providers load, each added body morph reaches weight one,
combined changes survive menu reentry, and resetting clears all five weights.
World and menu captures show textured clothing without broken geometry. Large
combined changes and extreme poses can still clip. These checks do not read
back the private Optimus value map or prove every animation/contact case.

The initial compatibility probe incorrectly expected current Eve meshes to use
the original Genessa graph. Their mesh defaults and live instances both have
no deformer; those outfit payloads remain byte-identical. Corrected checks of
Peony, Prototype, Casual Knitwear and Midsummer Alice pass. The failed probe
and corrected results are retained separately.

The authoring extension is C++20, matching the existing CSSRigAudit module.
Orchestration uses the installed Python 3.14.7 and Unreal's embedded Python.
The original shared graph remains untouched. The proposed new shared paths are
`DG_ClothMorphBodyV2` and `MDC_ClothMorphBodyV2`; only the four updated Genessa
meshes will reference them. The existing CSS variable-binding contract applies.

The baseline command was `python3 work/cloth-expanded-26073152/run.py baseline`.
It exited successfully only after all four meshes failed the expanded-buffer
check as expected. Creation and reload use the same runner with `create` and
`reload`. Raw receipts retain source hashes, buffer digests and preservation
checks. The GPU runner uses the existing held-simulation method, comparing the
engine cloth graph with the candidate for each shape, combined shapes, neutral
weights and disabled cloth. Its comparison must also reject an uncorrected
output for both the existing glute shape and the new glute-lift shape.

The two refreshed asset archives use annotated `unholygenessa-v0.3.1` and
`shared-assets-v1.0.2` tags, following this repository's existing product-scoped
CSSX tags. Their pinned trios and changelogs come from those tagged trees. The
CSS core `v1.0.2` tag and its archive remain unchanged.

Iris Out playback also passes on both Genessa and Eve variants with all five
added shapes set to one: the facial mesh and its new graph stay active, pose
snapshots advance, and no maintenance/overlay error is reported. Captures show
the skirt following the animated body without an exploded mesh. Appearance,
idle override and saved CSS state restore after these checks.
