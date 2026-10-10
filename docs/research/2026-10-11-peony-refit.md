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

The lower diagnostic localizes an outward drift to vertex 141329. Its initial
body gap is 0.524 cm; the first smooth fit moves it only slightly, but clearance
iterations drive it more than 20 cm away. Its nearest body triangle has normal
thigh/pelvis weights, not hand weights. The selected body-region query matches
the full-body query there. Next identify which neighboring triangle constraints
drive that drift and distinguish internal closure faces from visible garment
surfaces. Do not assume that every negative nearest-normal sample on an open
regional surface is a real exterior intersection, or simply suppress deeper
samples. This is a fitter diagnostic, not proof of the runtime cloth cause.

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
