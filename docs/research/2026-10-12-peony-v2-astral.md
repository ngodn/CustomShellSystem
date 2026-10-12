# Peony V2 doubles materials and installed trial

`work/three-outfit-rig-physics/peony-v2-package3` is installed. This establishes
the new assets and core on disk, not native visual or gameplay acceptance.
The first user check is Peony's fit in CSS and while walking in the world.

## Material construction

`/mnt/eins0fxE/CSS-Development/peony-v2-astral1` contains the six V2 ghost parents
and their six opposite-culling instances, authored through the existing Peony
ghost workflow in UE 5.6.1. They retain source scalar/vector controls, base-color
and trim-flow graphs. The stocking's vertex-alpha/opacity coverage multiplies
the native ghost fade. Source textures are replaced with shared placeholders
for runtime copying; all dependencies are shared assets or Engine classes.

The parents are Fabric, FlowFabric, FlowPanel, FlowStrap, SourceGlow and Stocking.
The runtime catalog now contains 23 definitions. Its previous 17 entries remain
byte-identical, and the six additions use independently decoded source graph
GUIDs and source texture names. The authoring workspace contains 17 packages:
12 ghost materials/instances, four neutral textures and the shared trim function.
The combined pack adds only 13 new assets. Existing neutral Color/Normal/Linear
textures and the trim function retain their accepted export/bulk bytes.

## Rejected first cook

`windows-cook1` exited zero but reported eight material warnings. The stocking
ghost inherited `output_translucent_velocity=True` from the regular stocking
material. The compiler rejected its DepthFade node because velocity output
writes scene depth while DepthFade reads it. Independent shader readback then
rejected the missing shader map, so this cook was not installed.

The correction sets velocity output false on ghost materials. Source outfit
materials are untouched. `rejected1` retains the initial generated assets,
recipes and receipts; the failed cook logs and small cooked outputs remain.
The cook wrapper now rejects explicit material compilation/null-shader warnings
even when UE exits zero. `windows-cook2` has zero errors and zero warnings.

## Checks

- Authoring and fresh-process reload exit zero. Original parameters, textures,
  sampler types, dependencies and culling overrides pass, with source hashes
  unchanged. The isolated Linux editor reports its existing read-only user
  preference warning at shutdown; it does not write outside the trial.
- `cooked-readback2/verified.json` passes the 17-package IoStore/import audit
  and matching export/bulk readback. All six roots have embedded SM5/SM6 skin
  and cloth shader variants.
- `cooked-culling-verified.json` checks all six instance parents, opposite
  culling overrides and embedded shaders for both Windows targets.
- `runtime-check1/receipt.json` accepts all 82 source materials (six parents,
  76 instances). Six source/ghost uniform pairs retain the compiled visible
  texture and numeric inputs, including flow, pattern, stocking and ghost-fade
  controls. Unlit ghosts do not require compiled normal or ORM inputs; those
  remain in the source-copy requirements. Missing compiled texture/control
  inputs and missing/placeholder source texture controls are rejected.
- Both Astral portable test targets pass. The Windows core builds. The combined
  package's 1,535 native catalog checks, imports and payload roundtrip pass.

The core SHA-256 is
`94bb1d08bc7d6dd4181e21d4605a3b87cfb7f6046c90f35194f0e090291c423b`.
`peony-v2-package3/core-verification.json` records native source hashes and test
results. The astral workspace's `completion-receipt.json` freezes recipes and
verification evidence, including the external Eve cook/check helpers.

## Installation and remaining acceptance

The installer verified the game was closed, checked the existing package/core/
loader identities, made rollback copies on theark, installed the two package
trios and selected the new core. Installed hashes were checked immediately and
again after cleanup. `peony-v2-package3/deployment.json` records the outcome.
Rollback: `/run/media/eins0fx/theark/CSS/peony-before-v2`.

The user relaunched during cleanup. Passive `first-launch-loader.json` confirms
the new core is active. `first-launch-status.json` reports 22 loaded packages,
zero rejected packages, no catalog errors and no maintenance/overlay errors.
No appearance was applied at that capture, so it does not establish outfit
rendering or doubles behavior. No character or camera input was sent.

The installed trial has 435 Eve packages and 283 shared packages. Existing six
Eve variants, CW/Genessa adapters and shared animation/facial resources remain.
Regular Peony V2 geometry and material controls are unchanged by the ghost work.

Check native fit, palette choices, component removal, stockings, body sliders,
Trim glow/Flow speed, expression playback, Faithful/Stray doubles, interruption,
profiles and gameplay. Static shader and catalog checks do not prove live
parameter copying, appearance, garment motion or performance. Minor unobtrusive
clipping remains deferred by the user; obvious fit defects are not accepted.

Temporary prefixes and duplicate staging/readback data were removed according
to `docs/development/cleanup-2026-10-12.md`. The installed build, rollback,
editor/cooked sources, recipes and evidence remain. The private Docker daemon
was stopped after its cook containers had exited.
