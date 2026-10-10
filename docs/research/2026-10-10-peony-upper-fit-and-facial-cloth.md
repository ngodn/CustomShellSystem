# Peony upper-panel fit and cloth-enabled facial mesh

October 10, 2026. Continues the [surface-coverage study](2026-10-10-peony-surface-coverage.md).
All work is private under Eve's authoring root. No game files changed.

## Upper fit: two rejected candidates

`work/peony-panelattachment1` probes the existing fully skinned attachment
witnesses against the captured body. Moving them closer to the resting body
does not reliably clear the walking arm. At 0.15 cm stand-off, 11 of the 32
render-vertex witnesses remain inside the posed body; at 0.35 cm, 20 remain.
These counts include duplicated render vertices. A tangential slide followed
by surface projection and weight transfer also leaves intersections.

Five of the 32 corresponding body-surface points have an enclosing arm-region
winding contribution. That regional surface is open, so this is diagnostic
evidence of overlapping regions, not proof of a skeleton defect. The body and
animations were not changed.

`tools/fit_peony_upper_panel.py` constructs fresh candidates from
`peony-panelcombined1`. It fits only upper-panel points above 105 cm, blending
to the full correction at 114 cm. The same displacement field transforms the
render mesh and cloth proxy. Body geometry, lower drape, skin weights, morph
deltas, topology, UVs, selectors and skeleton data remain unchanged.

- `peony-panelattachmentfit1`: pointwise projection toward a 0.35 cm body
  stand-off collapses some folded triangles to 1.88% of their previous area.
  Reject this shape. The initial import correctly refused occupied package
  paths; only private staged copies were moved aside before the successful
  fresh import and binding. Both copies and exit receipts remain available.
- `peony-panelattachmentfit2`: a 2 cm smoothing radius retains at least 74.04%
  of triangle area, with minimum old/new normal dot product 0.9241. It changes
  260 render points and 45/44 proxy points. The saved cloth settings and all
  115 staged dependencies stay unchanged. It still fails motion and contact
  comparisons, so reject it too.

The second candidate's independent anchors pass, but the unchanged strain
screen fails on 9 of 145 left-jog frames and 24 of 145 right-walk frames.
Other sampled gaits pass. The first failing edges connect a nearly fixed
upper particle to its movable neighbor: left 260/261 and right 283/303.
The fit shortened their rest length from about 1.430 cm to 1.252 cm.
No simulation setting or acceptance threshold was loosened.

Mapped render-vertex contact uses the same whole-body winding query as the
baseline, counting unique positions more than 0.02 cm inside the body:

| Pose | Combined baseline count / maximum depth | Smoothed fit count / maximum depth |
| --- | --- | --- |
| Left walk, frame 0 | 67 / 2.212 cm | 61 / 2.244 cm |
| Left sprint, frame 78 | 0 / 0 cm | 4 / 0.294 cm |
| Right walk, frame 0 | 0 / 0 cm | 4 / 0.293 cm |
| Right sprint, frame 78 | 0 / 0 cm | 4 / 0.295 cm |

Fewer walking witnesses do not establish an improvement: the deepest
intersection worsens and three previously clear samples acquire penetration.
Retain the prior combined geometry and colliders. The upper attachment still
needs correction; these trials do not justify another simple body projection,
global collider enlargement or higher solver iteration count.

## Facial expressions on the retained cloth candidate

`work/peony-panelfacial1` copies the unchanged combined cloth candidate and its
117 dependencies. Body-point correspondence is checked against the accepted
earlier Peony facial assembly before reusing the expression source.

The existing native facial builder adds 115 expressions to `SK_Peony_Face`,
retaining the 22 body/hand morphs. A fresh editor process reloads all 137.
The native verifier compares 406,473 built expression deltas, with maximum
retained error 0.000095176 cm. The largest filtered delta is 0.015001682 cm,
within the existing build threshold and comparison tolerance.

Preservation verification compares the non-expression mesh description,
built topology, bone maps and all 37 built sections, including skin data and
cloth mappings. Both persistent clothing assets also compare equal. A separate
native cloth inspection of the saved facial mesh matches the combined source
exactly in every reported field except its asset name:

- Render positions, weights, indices and mappings.
- Both panel physics assets and all inspected cloth settings.
- Existing cloth-section morph deltas.
- Shared skeleton, body physics and post-process references.

All 117 staged asset hashes remain unchanged. This integrates facial data with
the current cloth candidate; it is not a new facial feature claim, Windows-cook
validation, applied-expression playback test or in-game acceptance.

## Retained evidence and next work

The rejected fit directories each retain `fit-recipe.py`, geometry checks,
native logs, import/binding receipts and generated assets. The second also
retains six gait captures, independent anchor/strain checks and four mapped
contact samples. Their `study-receipt.json` files record hashes and cache
cleanup after confirmed editor termination and acquisition of the project lock.
Only regenerable caches were removed, 111,017,314 logical bytes across the two
fit trials. Original inputs, rollback packages and installed files remain.

The facial candidate retains `face-import.json`, `face-reloaded.json`,
`face-inspection.json` and `face-cloth-verified.json`, with the exact source
recipes and saved assets. Continue from this facial-enabled combined candidate
when making the next upper-attachment repair. Preserve its existing lower-panel
collision coverage and verify facial/cloth preservation after any rebuild.

Upper contact, body-shape fit, final material/normal review, wardrobe/catalog
integration, Windows cooking and gameplay remain. Slipstream and the remaining
three-outfit, shared-pack, runtime and release requirements remain active.
Keep the game closed until a complete trial has been installed and verified.
