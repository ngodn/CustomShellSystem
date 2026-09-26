# Eve outfit fitting playbook

Updated September 27, 2026. Read this before starting the next outfit, then check the [current status](eve-outfit-goal.md) and [evidence index](eve-outfit-index.md). This records reusable lessons, not a claim that Holiday physics is solved.

## Start each outfit from evidence

1. Inventory every garment and accessory against the source: dress, sleeves, underwear, hair, hat, earrings and shoes as applicable. A partial diagnostic export is not the complete outfit.
2. Audit missing weights, influence counts, material sections, shape keys and garment ownership before fitting. Gemini's Holiday source contained unweighted parts; apparent static fit did not establish usable deformation.
3. Preserve the original body and the current shared skeleton. Record hashes before edits. The production baseline has 386 bones, 82 sockets and nine virtual bones. Older private 379-bone imports are diagnostic assets only.
4. Establish a skinning-only baseline across idle, walk, jog, sprint, turns and representative weapon poses. Include supported morph extremes and combinations. Diagnose clipping already present here before adding physics.
5. Change the garment locally where the baseline proves a garment error. Keep the body's proportions. Verify the newly exported mesh, weights, normals, UVs and morphs against the fitted Blender source.
6. Add physics to an already understood baseline. Validate particle motion, render attachment and body contact separately. Inspect front, side and rear views and late motion frames, not just a neutral pose or the first successful frame.
7. Measure cost with identical input poses and settings. Repeat the baseline when a small difference could be run-to-run variation. Editor timings identify expensive paths; they do not establish game FPS.
8. Assemble the whole outfit, integrate with the current shared rig and retained body/hair physics, then cook and test in the game. Only after fitting is sound, finish garment controls, Original plus five clothing palettes, persistence and release validation.

## Mistakes and the replacement rule

| What went wrong or misled us | What to do instead | Evidence |
| --- | --- | --- |
| Treating unusual cloth motion as a physics-only problem when the original garment skinning already deformed badly | Compare identical poses with physics disabled first. Preserve or repair the original animation-follow behavior before solver tuning. | Goal log: animation-follow baseline, `CR_HolidayFollow` |
| Expanding the hip to compensate for a swinging forearm intersecting almost-pinned cloth | Identify the actual contacting body region and cloth drivers. Separate garment fit, arm contact and attachment error. | `panel-repeat-trace.json`, `panel-support-trace.json` |
| Trusting simulation particles while native render attachments stretched fur into spikes | Check render reconstruction at rest and in motion, including normals, barycentric extrapolation, weight normalization and fully skinned vertices. | `db7be81`, `check_panel_mapping.py` |
| Copying weights from the rendered garment when updating a simulation proxy | Keep proxy provenance explicit. Preserve that proxy's own weights and correspondence when moving its vertices. | Rejected `panel-waist-proxy.json`; corrected `panel-fit-proxy.json` |
| Applying a local displacement along inconsistent normals | Check directions against the body surface and re-evaluate the affected poses before saving a new large Blender file. | F12 corrected 39 directions; 62 sampled pose/morph cases improved |
| Increasing triangle density because coarse attachment seemed suspicious | Test a geometry-preserving refinement as a controlled trial. Reject it if stretching, contact or cost worsens. More triangles alone are not a fix. | Rejected `CA_Hip`: edge ratio 7.919, worse clipping and cost |
| Selecting an attachment because numerical clipping counts decreased | Inspect the visible result. A larger exposed patch can be worse despite fewer intersecting samples. | Rejected single-support replay: `panel-localmap-*`, frame-68 views |
| Attributing a trajectory change to a smaller collider without measuring baseline variation | Repeat identical runs first. Check collider transforms separately from cloth trajectories. | Identical runs differed by 3.559 cm; subset change was 1.977 cm |
| Assuming a CCD toggle applies to every collider type | Read the installed engine's collision path. UE 5.6.1 skinned triangles use a separate constraint skipped by ordinary CCD. | `PBDSoftBodyCollisionConstraint.cpp`, `panel-contact-history.json` |
| Solving late-sprint tunneling by raising temporal substeps without a cost gate | Treat it as a diagnostic that identifies a temporal problem, then find an affordable solution. | 16 substeps reduced particle penetration but cost about 71.4 ms in the editor trial; rejected |
| Trusting capsule coverage from the rest pose | Measure animated and morphed body coverage, including secondary-bone deformation. Four extra spheres did not solve the observed gaps. | `capsule-motion-bands.json`, rejected `secondary-coverage.json` |
| Generating body collision from a combined body-and-outfit mesh | Extract and verify a separate body-only source first. Otherwise garment geometry can contaminate the collision volume. | `prepare_collision_body.py`, `cbody-source.json` |
| Treating a successful import, build or save as finished clothing | Reload saved assets, measure deformation and cost, visually inspect, then validate the cooked candidate in game. | Private collider generation is still unaccepted |

Evidence filenames above are under `work/eve26/` unless a script, asset or commit is named. The evidence index and dated goal appendices carry the detailed results and limitations.

## Do not reopen a rejected trial on a hunch

Before trying a change, write its hypothesis, the one variable being changed, baseline artifact, expected measurable result and rejection condition. Check the evidence index for prior attempts. Revisit a rejected method only when there is new evidence that addresses its recorded failure, and state what changed.

After the trial, record the result as adopted, rejected or still unverified, with artifact paths and the next action. Keep failed results beside successful ones. A new script without a completed trial is not evidence that the method works.

## What Planet Diving can reuse

Reuse the source/weight audit, protected-asset checks, recorded pose inputs where compatible, morph comparisons, export verification, render-attachment checks, contact analysis and visual review sequence. Begin with the original Planet Diving source and identify its actual flexible parts. Do not copy Holiday's skirt weights, collision cutoffs, proxy density, stiffness or clearance offsets blindly.

The faster route is to avoid disproven experiments and detect source errors early. Holiday's final physics recipe is not established yet, so there is no proven universal preset to apply to every outfit.

## Storage and handoff

Keep originals, the accepted working source, useful rollback points, scripts, measurements and representative renders. Delete only confirmed superseded generated files after recording their identity and why they are safe to remove. Do not use broad cleanup commands in shared worktrees.

After a coherent checkpoint, update the current status and evidence index and commit only this effort's explicit paths. Keep concurrent CSS runtime/UI work separate. A next session should find the current reference, rejected alternatives and next test without reading the entire transcript.
