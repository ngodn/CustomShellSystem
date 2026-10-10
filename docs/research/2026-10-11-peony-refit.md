# Peony fit correction after the October 11 gameplay review

The user rejected the installed trial's oversized upper garment, lower-front
clipping and distorted world-space hanging panels. The screenshots are linked
in the goal. Peony correction takes priority over continuing Slipstream binding.
No game package, core, save, profile or settings were changed in this work.

## Reused work and identified gap

The earlier Eve tools `fit_nextgen_garments.py`, `sync_garment_morphs.py` and
`fit_morph_clearance.py` separate fabric fitting, morph correspondence and
triangle-interior clearance. `fit_peony_upper.py` only expanded intersecting
fabric. It did not shrink an oversized exterior. Its chest follow-up was also
limited to nearby breast-morph support. Thus those checks did not establish
the close silhouette requested by the user.

The new authoring tool `refit_peony_upper.py` operates on the retained
`new-outfits-flow-uv1.blend`, using the accepted body and its six shape controls.
The jacket/corset main fabric receives inward and outward fitting; separate
trim follows fabric displacement. Small trim components translate together.
The body and skeleton remain unchanged. These are offline Blender 5.2.2 changes.

## Candidates and limitations

All paths below are under `CSS_SeduXtress_eins0fx/work`.

- `peony-closefit1`: the corset's neutral sampled vertex-to-body gap had a
  95th percentile of about 5.70 cm before refitting. The first inward fit reduced
  it to about 1.07 cm, but review renders exposed neckline/chest intersections.
  This candidate is rejected for installation.
- `peony-closefit2`: direct triangle-clearance pushes clear the sampled contacts
  but compress small folds and turn some normals by over 90 degrees. This is
  rejected, not a shipping repair.
- `peony-closefit3`: spatially smooth clearance corrections replace individual
  pushes. All seven recorded shape cases reach zero failures in their selected
  upper-fabric clearance samples. Small compressed folds and normal rotations
  remain, so this is not evidence of complete geometric or gameplay acceptance.
- `peony-closefit4`: adds the requested sweetheart neckline with a 2.5 cm
  authoring depth parameter. Front/side/back review images are retained.
  Lower-front fitting is being investigated separately. Three lower projection
  variants exceed the 8 cm deformation guard and are rejected; their logs and
  recipes remain. Restricting the body material region or fitting fabric before
  its trim does not resolve the failure. Do not import their output or loosen
  the guard.

- `peony-closefit5`: responds to the user's request for more cleavage with a
  4 cm neckline-depth parameter and 6 cm Gaussian width parameter. These are
  authoring parameters, not measurements of the final opening. Neutral review
  shows the deeper sweetheart shape. The breast-size endpoint keeps the center
  opening, but its underbust fabric and trim still need repair. All seven
  recorded shape cases clear the fitter's selected upper samples; this does not
  establish clean topology, trim fit or gameplay acceptance. Scripts, hashes,
  neutral renders and breast-size endpoint renders are retained in its
  `work-receipt.json`. This candidate is not cooked or installed.

## Lower-front cause and recovery

The initial lower diagnostic localized outward drift to vertex 141329. Its
initial body gap was 0.524 cm, yet clearance iterations moved it over 20 cm.
The neighboring triangle interiors really passed through the thigh: full-body
solid-angle winding measured approximately 1 at those interior samples.
The donor was not a hand, and restricting the body region did not fix it.

`work/peony-lower-diagnostic1/topology.log` compares the preserved source and
bound geometry at corresponding vertices. The old `CSS_LowerClearance` radial
fit stretched ordinary 1–2 cm fabric edges into 16–20 cm edges across the leg.
These are visible garment faces, not internal closure caps. The underwear had
258 edges stretched more than threefold relative to joint-aligned geometry,
with a maximum ratio of 23.07. Pushing their corners farther outward was the
wrong repair. This establishes an authoring defect, not a CSS runtime cause.

`refit_peony_lower_surface.py` now recovers the underwear and its trim from
the retained `CSS_JointAlignment` shape before that radial fitting step. It
builds fresh body-morph correspondences, fits the fabric with a smooth
displacement field and triangle-clearance checks, and transfers body skin
weights. Original source, accepted body, other garment parts and cloth points
remain unchanged. Unresolved selected contact samples or fabric triangles
turning over 90 degrees reject the candidate.

`peony-lower-recovery1` exposed two small front holes in its review render.
The full-body comparison identified an omitted body section, `Eve Genitals`.
`peony-lower-recovery2` includes that section in its body surface:

- All seven individual cases finish with zero selected fabric-contact failures
  and zero over-90-degree fabric normal rotations. Minimum triangle area ratio
  is 0.239 relative to the recovered aligned fabric.
- Maximum neutral fabric edge length is 3.51 cm, down from 20.17 cm. Maximum
  edge stretch relative to aligned source is 1.52, with no edges over threefold.
- `preservation.json` verifies unchanged body, other parts, skeleton, topology,
  UVs and unrelated morph deltas. Transferred weights are normalized; at most
  floating-point noise was discarded by the eight-influence export check.
- Neutral, combined lower-body sliders at 1, and recorded walk frame 36 have
  retained front/back/side/quarter review renders. The repaired front fabric
  stays intact in these views. The first walking render used nonexistent index
  145 of a 145-frame capture and stopped; frame 36 was then rendered successfully.

These are offline checks, not continuous gameplay acceptance. The review uses
neutral opaque materials and does not reproduce the garment's alpha masks.
Visible overlap in other sections must be checked with their actual materials.
Separate trim has internal contacts not covered by the primary-fabric pass;
do not describe every garment triangle as collision-free.

The two main hanging-panel components do not exhibit the same extreme edge
stretch. Their geometry was not changed by the underwear recovery. The world
report still needs comparison with the new package before attributing all of
the reported ragged appearance to cloth simulation.

## Upper trim follow-up

`peony-closefit6` carries the deeper neckline and recovered lower garment. Upper
trim now uses fixed neutral fabric donors across body shapes and transports its
surface frame; small hardware rotates with the fabric rather than only moving
its center. Neutral and maximum-breast renders are retained. The large-bust
underbust result is still not accepted: a targeted body-contact probe finds
small contacts on components 70/71, and trim-to-fabric appearance needs work.

`cloth-sprint78` replays the retained two-panel geometry composite against the
repaired assembly. The source composite uses matching body poses from separate
full-component runs, not a simultaneous game render. It preserves a useful
cloth/garment comparison without proving current gameplay behavior.

`assemble_peony_refit.py` applies only jacket/corset coordinates and their six
morph endpoints to the retained `peony-hipfit11` interchange. The collar stays
unchanged. It removes 11,430 faces belonging to both hip-accessory and arm-ornament
groups. Body, hair, heels, gloves, skin weights and cloth point identities remain
unchanged by that assembly step. The lower-fit tool has a separate, unaccepted
weight-transfer experiment; do not confuse the two scopes.

## Customization controls

`build_peony_customize.py` recognizes either the original complete wardrobe or
the complete removal of the four pocket/arm-ornament sections. Partial removals
are rejected. Removed parts no longer produce visibility controls or parameter
bindings. The original recipe regenerates exactly unchanged; the candidate has
20 wardrobe controls instead of 22, retaining five palettes and identical
glove, collar, boot and leg-panel visibility definitions. The existing body/hair
controls are merged separately as before.

Verification: `peony-closefit3/control-check.json` records the original-recipe
comparison, removed control IDs, preserved controls and partial-removal negative
case. This verifies the builder only, not a rebuilt catalog or runtime package.

## Remaining work

Finish lower-front fitting, inspect the upper folds and verify intermediate and
combined body shapes, animation weights and moving poses. Repair the separate
world-space cloth-panel defect while retaining simulation. Rebuild normal and
facial meshes, cloth bindings and morph buffers after the final geometry change;
then cook, check dependencies/catalogs and perform gameplay comparison. Keep all
previously working Eve variants, Genessa, Commander White, action interruption,
facial playback, doubles and saved selections in the regression scope.

None of these private candidates is installed or accepted as a completed fix.
