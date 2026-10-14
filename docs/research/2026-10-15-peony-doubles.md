# Peony doubles: normal summons in both forms

The four normal-input cases pass mesh selection and CSS group cleanup on the
installed `d7cb0635d112f5fa` development core, PID 1944353. This is partial
coverage of the doubles gate, not acceptance of the full physics or visual matrix.

**Follow-up: current customization now matches in both forms.** The focused
Faithful and Stray-primary captures each verify all six Peony ghost families,
58 explicitly set source parameters, both nonzero body morphs and seven hidden
sections. This closes the missing Faithful ghost-binding observation below.
The secondary clone requires Mirage level 2; the live eligibility getter returns
false in the current Faithful state. No progression was changed. Details follow
in [the customization capture](#current-customization-follow-up).

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

## Current customization follow-up

Evidence: `work/three-outfit-rig-physics/doubles-customization-20261015/`.

The first `faithful-details` run fails on an expired component during its long
sequential property read. The request/reply log retains the failure; input,
saved settings and camera view are preserved. Its early screenshot catches the
double against the bright opening, but is not a useful cloth/contact acceptance
image. Nothing was changed to extend the native double's lifetime.

The focused `--customization` capture reads material bindings, morphs and hidden
sections first, then one representative of each Peony shader family. Every
completed family is written immediately so a later expiry cannot erase earlier
observations. `faithful-customization` passes, followed by the single
`stray-customization/stray-css` case. Full `--details` is now opt-in in the form
runner because it can outlast short-lived components.

| Observation | Faithful | Stray primary |
| --- | ---: | ---: |
| Expected Peony companion parents | 6 | 6 |
| Explicit source scalar/vector/texture parameters copied exactly | 58 | 58 |
| Nonzero body morphs copied exactly | 2 | 2 |
| Hidden sections matching source LOD0 | 7 | 7 |
| Active/cached CSS groups after expiry | 0 / 0 | 0 / 0 |

The current morphs are `PBMBreastsSize=0.36000001430511475` and
`PBMGlutesSize=1`. Hidden slots are 38, 39, 40, 42, 43, 44 and 45. Material
observations include `FlowSpeed=0.2500000298023224`, `GlowStrength=17.25` and
`StockingOpacity=0.699999988079071`. Values are compared against the actual
player sources, not an independently assumed preset. Clone materials have
different object identities from player materials, correct companion parents,
the appropriate `CSS_AstralCorrupted` value, live time enabled and bounded fade
opacity. Reads span the lifetime/fade; they are not one simultaneous frame.

The Stray wrapper exits 0 and verifies full saved state, appearance, CINE model
and player/view restoration. `final-restoration.json` separately restores the
facing from before this turn's first Faithful press, after exact-current-state
and stationary-input guards. Position and camera view match, rotation differs
by at most the established 1e-8-degree roundoff tolerance, and health is 88.
The successful single-case wrapper does not rewrite the earlier four-case
parent's failed restoration receipt.

Verification commands:

```sh
python3 tools/diagnostics/verify_doubles_summon.py \
  work/three-outfit-rig-physics/doubles-customization-20261015/faithful-customization \
  --customization-form faithful
python3 tools/diagnostics/verify_doubles_summon.py \
  work/three-outfit-rig-physics/doubles-customization-20261015/stray-customization/stray-css \
  --customization-form stray
```

Negative controls reject changed morphs, hidden-section flags, texture values
and companion parents. Parameter keys include name, association and index;
expression GUIDs are not value identity. The comparison checks explicit source
overrides against the clone's copied values, not every inherited shader default
or visual equivalence to the lit player shader.

The preserved `HasSecondaryClone` bytecode reads
`UpgradeStat.Shell.Genessa.Mirage.Level` and requires its integer value to be at
least 2. The live signature is a return-only getter, and the current owned
ability returns false. Only primary clones were observed in the actual Stray
summons. An upgraded save was requested for later secondary-clone coverage;
current progression and save files remain untouched.

The later upgraded-save [secondary-clone check](2026-10-15-outfit-doubles.md#peony-and-both-genessa-variants-completed-secondary-clone-checks)
passes Peony mesh copying and cleanup on both Stray actors, with full state
and player/view restoration. It does not extend the material-parameter capture
above to the secondary actor. Remaining work includes that parameter coverage,
other outfits, more customization combinations and moving visual/contact
coverage. The captures above test the Black and white floral look only. No
core or cooked package changed for either check.
