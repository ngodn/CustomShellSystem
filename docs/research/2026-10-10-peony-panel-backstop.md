# Peony split-panel backstop and saved candidate

October 10, 2026. Private authoring work, not installed or release accepted.
Paths below are relative to the Eve authoring root's `work/` directory.

## Contact evidence and controlled changes

`peony-paneltrace1` adds selected-cloth native contact queries. Solver group
identity must match the selected asset, and particles are checked against the
component export. Position agreement is within 0.000004 cm. This avoids
interpreting another cloth group's collision data as the selected panel.

Left walk frame 0 edge 401/403 grows from 1.849 to 5.235 cm. Its endpoints
contact the left calf with normals 66.568 degrees apart. Their initial targets
are inside the collider by 2.106 and 2.839 cm. Other failures involve thigh
contact. Later frame-specific failures vary between native runs; a previous
right sprint frame-78 witness did not recur in the contact capture.

Both panels also fail the sprint strain screen at frame 2 with the closest
body colliders approximately 20..24 cm away. An independent reconstruction
puts those endpoints within 0.009 cm of their 3 cm backstop surfaces.
This is a query against saved maps and captured poses, not an intermediate
constraint trace. The controlled comparisons establish the next change:

- Two substeps alone leave left walk and right sprint failures.
- Removing selected-panel self-collision alone leaves early sprint failures.
- A 0.1 cm self-collision margin with two substeps passes five of six gait
  screens; left walking still fails three frames.
- A 60-frame lead-in does not repair left walking.
- Removing the selected loose panel's backstop, while retaining body and
  self-collision, passes all six gait screens. Both early sprint controls also
  pass with one substep and the original self-collision margin.

`peony-panelsteps1` retains these reports and independent anchor/strain checks.
The controls modify only the selected panel. They do not prove a complete
two-panel candidate, so the chosen settings were saved and retested together.
No diagnostic that disables body collision or self-collision is adopted.

The nonlegacy backstop calculation is checked against local UE 5.6.1
`Chaos/PBDSphericalConstraint.h`; group ownership uses the engine's actual
`FClothingSimulationCloth::GetGroupId()`. The contact probe is editor tooling,
not per-frame CSS runtime work. See also Epic's
[Chaos clothing simulation API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ChaosCloth/FClothingSimulation?application_version=5.6).

## Saved candidate

`peony-panelcloth1` saves zero backstop radii for both panels, self-collision
thickness 0.1 cm, two substeps and four iterations. Body collision, directed
per-panel physics assets, self-collision, animation drive and full simulation
remain enabled. Geometry, weights and morphs are unchanged by this step.
Fresh native inspection confirms two cloth assets with 531/529 particles,
16 pins each, saved maps and retained body physics/post-process bindings.

Six fresh captures use the saved asset without diagnostic overrides. Every
capture has 145 frames at 60 Hz. The strain screen rejects an edge only when
its length exceeds twice its rest length AND its extension exceeds 1 cm.
Ratio and extension maxima can belong to different edges.

| Panel / gait | Failed frames | Maximum ratio | Maximum extension, cm |
| --- | ---: | ---: | ---: |
| Left walk | 0 | 2.040 | 0.915 |
| Left jog | 0 | 1.732 | 1.056 |
| Left sprint | 0 | 2.526 | 1.608 |
| Right walk | 0 | 1.532 | 1.017 |
| Right jog | 0 | 1.617 | 1.398 |
| Right sprint | 0 | 2.129 | 1.702 |

All anchors pass, maximum reconstruction error 0.000025 cm. Recorded actual
solver settings are two substeps and four iterations. Whole-component editor
cloth-step means span 1.647..1.853 ms, p95 2.256..2.639 ms and maxima
3.111..5.611 ms. These are not per-panel costs, matched game-performance
measurements or a claim of no FPS impact.

## Trim and material correction

The earlier status note claiming unimplemented trim ownership was incorrect.
`peony-cloth2/panel-sections.json` and `merge_peony_panel_sections.py` show that
68 Arm-trim triangles and 232 Lower-1-trim triangles already belong to the
5,330-triangle fabric section. The split preserves their face corners and
UV2 family selectors exactly. Each side retains 34 Arm, 116 Lower-1 and
2,515 Lower-2 triangles. The empty former trim slots are not missing geometry.

The partition binding script used the older `peony-cloth1/binding-plan.json`.
It assigned `MI_peony_monochrome_Lower_2` to both split slots instead of the
combined `MI_peony_monochrome_Panels`. The private candidate now restores that
combined material. All other slots retain their prior material interfaces.
Fresh native inspection after the material save is identical to the previous
inspection, including render geometry, mappings, morphs and cloth settings.
The incorrect binding was never installed.

`material-restored.json`, `panel-family-verified.json` and
`inspection-after-material.json` retain the evidence. Only the candidate's
`SK_Peony.uasset` differs from its staged inputs. All staged inputs in the
contact and step-control projects remain byte-identical.

## Remaining acceptance

The first mapped full-outfit review identifies remaining contact failure.
Both panels are reconstructed from separately captured full-component runs
with identical body poses, not a simultaneous render-thread capture. The
walk frame-0 and sprint frame-2/frame-78 views no longer have the earlier
extreme pulls, but hems bunch and the body-contact screen fails:

| Pose | Left inside vertices / max depth, cm | Right inside vertices / max depth, cm |
| --- | ---: | ---: |
| Walk 0 | 195 / 3.838 | 42 / 1.437 |
| Sprint 2 | 41 / 1.767 | 57 / 1.187 |
| Sprint 78 | 29 / 2.575 | 427 / 3.422 |

Counts use unique mapped vertices more than 0.02 cm inside the posed neutral
body, classified by oriented solid angles. They are not visible-pixel counts
or triangle-intersection counts. Hair is hidden and the diagnostic render
does not apply footwear visibility masks; exposed original feet in these
images do not establish a new footwear defect.

For the deepest witness in each panel/pose, all three source simulation
particles are also inside the body. Their saved-recipe collider distances are
positive, approximately 0.47..4.86 cm. This rules out render mapping alone as
the cause at those witnesses and identifies gaps between the collision
envelope and the posed calf/hip surface. `mapped-body-contact.json` and
`mapped-contact-witnesses.json` retain the points and limits. The native
simulation still needs contact repair; the candidate is not fit-accepted.

Using the earlier, larger `PA_Legs` envelope as a selected-panel control does
not resolve the overall problem: left walk fails 52 strain frames, left sprint
91 and right walk 101, with maximum extensions 9.445, 12.499 and 10.015 cm.
Right sprint passes its strain screen, but contact was not established for
that control. All anchors still pass. This is evidence against simply restoring
the older collider wholesale, not against repairing the measured local gaps.
Each control replaces only the reported panel's collision asset; nothing is
saved into the candidate by this comparison.

Continue from these witnesses through gait poses and transitions, check supported
body morphs and modular/palette controls, then verify the final Windows cook
and gameplay. Preserve the merged trim and correct combined material during
integration. Facial replacement, CINE, Astral doubles, action interruption,
continuous motion and measured game performance remain in the full goal.

Do not request a game launch until a complete trial is installed and verified.

## Reproduction and preservation

The native patches are `peony-panel-contact-probe.patch`,
`peony-panel-cloth-settings.patch` and the subsequent
`peony-cloth-factory-guard.patch` in `tools/eve-fit/native`. The last patch
requires the exact Chaos factory before every solver readback, including the
new actual-substep metrics. It does not change simulation settings. Earlier
captures retain their pinned helper module; the rebuilt helper is for later
projects. No CSS runtime or installed file changes in this study.

`resolve_panel_render.py` now selects the requested cloth section and checks
its asset index and verification identity. The earlier one-panel replay stays
byte-equivalent as parsed JSON; a mismatched panel is rejected without output.
The two-panel views retain both capture hashes and explicitly identify their
separate runs. Do not present them as simultaneous render-thread evidence.

Private projects retain stage hashes, scripts, command exits, native reports,
saved assets and study receipts. Regenerable caches are removed only after
the relevant editor/Blender processes are terminal and the project lock is
acquired. Original assets, rejection evidence and installed packages remain.
