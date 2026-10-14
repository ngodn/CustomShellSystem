# Peony doubles: normal summons in both forms

The four normal-input cases pass mesh selection and CSS group cleanup on the
installed `d7cb0635d112f5fa` development core, PID 1944353. This is partial
coverage of the doubles gate, not acceptance of the full physics or visual matrix.

Evidence: `work/three-outfit-rig-physics/doubles-current-20261015/`.
The preceding requested Naylshotte shot is separately recorded in
[the sidearm report](2026-10-15-sidearm-aim.md).

## Cases and observations

The current input mappings bind F to the normal shell ability. Each case sends
one press and release, observes the pressed key, then waits for the owned double
to deactivate and any cached CSS visual to expire. No ability is directly
activated through reflection, and no resources are written.

| Form / mode | Active observations | Observed kind | Result |
| --- | ---: | --- | --- |
| Faithful / Default | 3 | AstralCopy | Native mesh only; no CSS visual prepared |
| Faithful / Use CSS | 1 | AstralCopy | Native combat mesh plus Peony V2 visual; one group prepared and removed |
| Stray / Default | 14 | AstralClonePrimary | Native mesh only; no CSS visual prepared |
| Stray / Use CSS | 1 | AstralClonePrimary | Native combat mesh plus Peony V2 visual; one group prepared and removed |

All cases finish with zero active/cached CSS groups, zero fallbacks and no
reported Astral error. The source remains Peony. There are 168 finite sampled
bone transforms across pelvis, thighs, calves, feet and head. This does not
establish exact pose matching or correct floor/body contact.

Both CSS visuals resolve `ABP_Secondary` and preserve all eight captured body
input fields: regional frequency, damping, motion and enabled arrays, global
frequency/damping/motion, and region selection. Cloth and rigid-body disable
flags match the source. CSS visual collision is disabled as designed; the
native combat component remains separate. These are configuration observations,
not proof of cloth motion, collision quality or solver performance.

The Stray visual has 46 material slots and nine distinct shared Astral parents,
including all six Peony companions: Fabric, FlowFabric, FlowPanel, FlowStrap,
SourceGlow and Stocking. Faithful's detailed material read occurs after its
short visible lifetime and returns empty bindings. Its sampled active pose
still identifies the correct Peony mesh. Do not count this as a Faithful ghost
material verification. Screenshots taken after the long detail read are too late
for useful visual acceptance.

An earlier `faithful-css/` probe outside `forms/` also passes one normal summon
and complete group cleanup. The four-case verifier does not double-count it.

## Restoration and the retained failed guard

All four child fixtures exit 0. Each preserves its saved settings, CINE model,
camera view/control rotation, health and process, and releases input. The parent
restores the original doubles modes, mirror setting and Faithful Peony.

The parent exits 1 at its accumulated 500 cm restoration guard. Recorded
movement is 551.37 cm during the form transition and 442.99 / 443.00 cm during
the two Stray ability uses, totaling 1437.30 cm displacement. The original
failure receipt is unchanged. It is not a failed summon or a passing parent run.

`position-restoration.json` and its request log record a separate guarded
recovery. Before restoring, the current transform must exactly equal the failed
parent's final transform, the player must remain stationary/alive with no held
input, and the full saved state and appearance must match their originals.
Each recorded movement segment is below 600 cm and total travel below 1600 cm.
The original pre-test position and facing are restored with exact position/view
readback and actor-rotation tolerance of 1e-8 degrees. Health is 88 again after
normal return to Faithful; no health write is performed.

The reusable parent now records checkpoints and applies those separate step and
total bounds. Offline negative controls reject an excessive step, excessive
total travel and nonfinite input. This revised parent has not been replayed.
Whole-matrix CINE-model equality was not reached by the failed parent and is
not established by the child-only checks. Future runs also archive its initial
CINE response explicitly.

## Reproduction and remaining coverage

Host Python is 3.14.7. The existing project toolchain supplies all dependencies.
Use `tools/diagnostics/check_doubles_forms.py` with a new `--output` under work
and the installed `--core-sha256`. It uses temporary normal CSS settings and
restores them. `check_doubles_summon.py` is the individual normal-input case.
The preserved recipes describe the actual runs; the current capture script now
photographs the active double before its long detail reads and inspects the CSS
visual first. That ordering improvement is not yet live verified.

Independent verification:

```sh
python3 tools/diagnostics/verify_doubles_summon.py \
  work/three-outfit-rig-physics/doubles-current-20261015/forms
```

The verifier checks actual actor kinds, meshes, bone samples, physics input
readbacks, material parents, state and counter deltas. It explicitly excludes
the secondary Stray clone, full-cloth motion/contact, attack/damage behavior,
all customization combinations and full restoration proof.

Next coverage includes that secondary clone, Faithful ghost materials, live
morph/visibility/palette comparisons, and the other scoped outfits. No game
binary or cooked asset changed. Full release and performance gates remain open.
