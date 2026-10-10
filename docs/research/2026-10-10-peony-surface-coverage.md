# Peony surface coverage and fixed attachment contact

October 10, 2026. Private `peony-panelcoverage2` study in the Eve authoring
root, continuing the [joint-coverage study](2026-10-10-peony-joint-coverage.md).
No installed package, saved selection or CSS runtime change.

## Right leg

`fit_right_surface.py` takes the retained walking and sprinting penetration
witnesses, finds the nearest posed body boundary, then increases the closest
own-leg capsule's endpoint radii to cover that boundary with a 0.15 cm margin.
It rejects corrections above 3 cm and degenerate tapered capsules. It leaves
all centers, bones, directions, opposite-leg geometry and cloth settings
unchanged. This is a local fit, not proof of whole-body containment.

Six endpoint radii change: right-thigh endpoints 15/16 by 2.112 cm, 17/18 by
2.182 cm, and right-calf endpoints 19/25 by 2.172 cm. The fresh
`PA_RightSurface` asset passes native readback against the recipe.

All three selected-right-panel captures pass the unchanged strain screen:

| Gait | Failed frames / 145 | Maximum edge extension, cm |
| --- | ---: | ---: |
| Walk | 0 | 1.513 |
| Jog | 0 | 1.405 |
| Sprint | 0 | 1.983 |

These use four iterations, two substeps, body collision, self-collision and
full cloth simulation. No threshold is relaxed. Independent anchor checks
pass. The unselected left panel still uses its previous saved collider.

Mapped right-panel vertices more than 0.02 cm inside the posed neutral body
fall from 40 to zero at walk frame 0, and from 419 to zero at sprint frame 78.
This is the same whole-body winding query as before, not a visible-pixel,
triangle-crossing, body-morph or full-clip acceptance test.

Front/quarter walking and side/back sprint renders show the measured right
panel without the earlier large pulls. Some hem bunching remains. The gold
surface is the measured cloth; the other panel is skinned and the neutral
review does not represent final materials or footwear masking. Continuous
motion and the combined saved outfit still require review.

## Left leg and the attachment boundary

The same fitter uses all 48 retained leg-region witnesses, excluding the arm
and torso classifications. It increases left-thigh endpoints 8/9 by 2.065 cm,
10/11 by 1.489 cm and proximal endpoint 22 by 1.538 cm. Native readback passes.

This improves leg contact at the sampled poses but fails strain: 32 walking,
15 jogging and 8 sprinting frames. Maximum extensions are 2.250, 2.660 and
2.665 cm respectively. An eight-iteration control retains exactly the same
failure counts, with maximum extensions 2.017, 2.399 and 2.491 cm. Increasing
iterations is rejected as a repair for this candidate.

The first failure in each gait is edge 260/261. Its rest length is 1.430 cm;
it reaches 2.915 cm at walk frame 51, 2.986 cm at jog frame 33 and 3.047 cm at
sprint frame 26. Vertex 260 belongs to the fixed upper transition. This
motivates a separate control restoring only proximal endpoint 22 to its
preceding radius, while retaining the four lower endpoint corrections.

That control passes all three 145-frame strain screens at four iterations.
Maximum extensions are 1.664 cm walking, 1.766 cm jogging and 1.644 cm
sprinting. All independent anchors pass. At the same sampled poses there
are 67 inside positions in walking (58 nearest arm, nine nearest torso),
and zero in sprinting. The remaining walking contact is at the upper
attachment/arm area; no sampled nearest-leg witness remains in either pose.
Front walking and quarter sprint renders retain some hem folds without the
large fixed-boundary pull. This control is retained with the right surface
correction for the combined save.

At four iterations the mapped left walk frame has 69 inside positions:
65 classify to the nearest arm face and four to the nearest torso face.
Sprint frame 78 has one inside position, 0.520 cm deep, nearest an arm face.
No sampled position in those two frames has a nearest leg face. Nearest-face
classification alone does not identify which volume encloses the point.

## Upper attachment diagnosis

`inspect_attachment_fit.py` checks 32 render vertices at 19 unique positions
previously identified as fully skinned. All are outside the body in the bind
pose, but inside its combined surface at walking frame 0. Recomputing their
weights from the nearest bind-body triangle produces essentially the same
weights and posed positions. A repeated surface-weight transfer would not
repair these witnesses.

`inspect_attachment_regions.py` partitions body triangle solid angles into
arm, leg and torso contributions. At all 32 witnesses, the arm contribution
is between -0.998507 and -0.998150; the leg and torso contributions largely
cancel. These regional surfaces are open and are not independent containment
tests. Together with the full-body winding and matching skinning results,
they identify arm overlap, including witnesses whose nearest boundary face
was classified as torso. The earlier nearest-face label must not be treated
as proof of a torso weight defect.

The arm/attachment intersection remains unresolved. Do not inflate torso or
leg colliders or reweight working attachments to hide it. A fit/contact change
must preserve the intended outfit shape and be checked through motion.

## Remaining work

The right collision asset and left proximal-endpoint control are retained
for the combined candidate. The left enlarged-endpoint candidate and its
eight-iteration version are rejected. The extra-iteration timing ran alongside
offline contact analysis, so it cannot support a clean cost comparison.
It provided no strain-screen benefit and is not adopted.

`peony-panelcoverage2/study-receipt.json` freezes 285 files and verifies all
118 staged files against the initial manifest. It removes 20,761,956 logical
bytes of regenerable
caches only after native/Blender processes finish and the project lock is
acquired. Source assets, reports, failed candidates and recipes remain.

The new `peony-panelcombined1` project rebuilds only `SK_Peony` with
`PA_LeftSurfaceHipControl` and `PA_RightSurface`. Fresh native inspection
matches the prior saved mesh exactly except the two cloth physics references
and new clothing GUIDs: render mapping, weights, cloth maps, recorded solver
settings, morph deltas, mesh physics and post-process reference are retained.
Its six gait captures use the saved bindings without collision or iteration
overrides. All pass the same strain predicate and independent anchor checks:

| Saved panel / gait | Failed frames / 145 | Maximum edge extension, cm |
| --- | ---: | ---: |
| Left walk | 0 | 1.666 |
| Left jog | 0 | 1.766 |
| Left sprint | 0 | 1.646 |
| Right walk | 0 | 1.511 |
| Right jog | 0 | 1.403 |
| Right sprint | 0 | 2.008 |

The right sprint maximum alone does not trigger the failure predicate, which
requires both a ratio above two and an extension above 1 cm on the same edge.
Whole-component editor cloth-step means range from 1.630 to 1.905 ms. These
short editor captures are not matched game frame-time measurements or a
performance guarantee.

The saved combined mesh reproduces the mapped-contact result: right walk 0,
right sprint 78 and left sprint 78 have zero inside samples beyond 0.02 cm.
Left walk 0 retains 67 inside samples, maximum depth 2.212 cm, around the
upper attachment. Composite front-walk and quarter-sprint renders show both
panels from their separate captures at exactly matching body poses. These
are offline composites, not a simultaneous render-thread capture. Hem folds
and arm contact remain visible review items.

The combined receipt freezes 197 files, verifies all 121 staged inputs with
only the intended `SK_Peony` change, and removes 90,383,296 logical bytes of
regenerable caches after every job finishes. Both private projects retain
their assets, exact recipes, native reports and review images.

Upper attachment fit, body controls, materials, Windows cook, facial
integration and gameplay/performance remain pending.
No launch is requested until a complete game trial is installed and verified.
