# Finger calibration on the aligned arms, 2026-09-21

The existing source-Eve finger calibration removes the new left-hand skin
crossings in all five tested H2 idle samples on the V44B2 geometry. Front and
rear review of sample 95 shows separated, naturally hanging fingers instead
of the folded fingers on the uncorrected candidate. This is an offline
improvement, not a released V44 or a complete grip repair. V43 stays installed.

## What changed

The [arm-rest candidate](arm-rest-alignment.md) changes the arm/hand bind
orientations and consistently reposes the mesh and morphs. The earlier
[finger calibration](../../../CSS-Mod-Authoring/docs/next-gen-hand-graph-calibration.md)
consumes source-game rotations and produces Eve's measured finger rotations.
Its use must respect both input and output coordinate conventions.

The H2 evaluator's game-reference inputs already have the source-game finger
rotations. All 95 raw comparisons (19 left-hand bones at five samples) agree
with the extracted raw tracks within 0.0000358 degrees. Do not apply the old
V43 inverse compatible-Skeleton mapping to these inputs. An initial comparison
incorrectly compared compressed output against raw input and stopped; the
corrected check separates those domains. The measured compression difference
is at most 0.169611 degrees. The raw equality tolerance remains 0.001 degrees.

Reusing the calibrated output locals is supported by actual deformation
checks, not just matching names. The 19 finger local translations change
only by rounding in the aligned bind. Source-Eve open and closed controls
retain exactly the same four neutral seam triangle pairs on both V43 and
V44B2. In wrist-relative coordinates, the 1,501 fully hand-weighted vertices
differ by at most 0.5383 mm for the open control and 0.6927 mm for the closed
control. The mean differences are 0.0401 and 0.0806 mm. Reposing and skinning
with blended weights are not exactly interchangeable; do not claim identical
deformed surfaces.

## Actual skin checks

Blender's armature modifier evaluates the fitted body with linear skinning.
The two controls include their original corrective-shape values. The H2
comparison changes only the 19 left-finger rotations and uses saved fitted
shape values, with the six public body morphs at zero. It adds no directional
clearance solver and no dynamic corrective-shape driver. The wrist, right
hand, unrelated local transforms, translations and scales remain unchanged.

| Original H2 frame | Uncorrected new triangle pairs | Calibrated new pairs |
| --- | ---: | ---: |
| 0 | 74 | 0 |
| 95 | 142 | 0 |
| 191 | 127 | 0 |
| 287 | 86 | 0 |
| 383 | 74 | 0 |

Every corrected sample retains exactly the four original neutral seam pairs.
The same gate exits 2 for the uncorrected variant and 0 for the corrected one.
It compares exact triangle pairs, including same-finger folds, rather than
counting only contacts between different fingers. Coplanar contacts and fully
enclosed disconnected surfaces remain outside the intersection algorithm.

Matched front/rear renders use the fitted B geometry, independently checked
against its engine bind. B2 differs only in five unweighted attachment leaves;
the body and hand geometry are identical. The numerical skin checks use B2
directly. MoreBeaute's corresponding supporting hand is also open, so an open
idle hand alone does not justify forcing a fist. The render shows better
finger articulation; it does not prove weapon clearance or attachment fit.

## Reproduction and next step

Evidence directory: `work/grip-grounding-v1/arm-rest-hand-transfer-v1/`.
Read `report.json`, `all-h2-samples-v2/report.json`, its
`input-domain-checks.json`, `gate-results.json`, command/result manifests and
`visual-review.json`. The failed compressed/raw comparison is retained in
`all-h2-raw-compressed-rejected.py` and `all-h2.log`.
The reviewed renders are SeduXtress
`work/nextgen-audit/arm-rest-calibrated-fitted-hands-s1-v1/view-a.png` and
`view-b.png`. [Archived probes](../../tools/authoring-probes/hand-transfer/README.md)
reproduce the comparison without saving assets or changing the game.

Next carry the verified input convention into captured attack/overlay and
sidearm cases, checking the aligned fitted geometry and weapon contact.
Earlier V43 clearance results do not automatically transfer to B2. Complete
that animation coverage before integrating a production graph adapter,
corrective driver, full mesh cook and live acceptance. Preserve the tolerable
Axe & Dagger/Axatana behavior and the accepted right-hand improvement.

The full Next-Gen goal remains active: modular physics/motion, native controls,
UI, build/distribution and live lifecycle/performance verification. Original
proportions, hair 200/24, accepted body settings and disabled CSSX are unchanged.

Primary reference: Epic's [retarget manager documentation](https://dev.epicgames.com/documentation/unreal-engine/retarget-manager-in-unreal-engine)
describes retarget base poses. The exact normal/additive remapping equations
come from the installed UE5.6.1 `SkeletonRemapping.cpp:149-163`, as recorded in
the earlier calibration document. The local measurements above establish the
behavior of this candidate; the documentation alone does not prove the repair.
