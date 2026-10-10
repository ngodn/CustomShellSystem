# Peony cloth body-shape GPU check

The shared cloth correction passes 22 held-pose comparisons on Peony's normal
and facial meshes. This verifies the render offset, not walking contact,
shading, frame time or gameplay acceptance. Nothing new is installed.

## Comparison

`CSS_SeduXtress_eins0fx/work/peony-clothgpu1` runs UE 5.6.1 offscreen with
Vulkan. Each case settles both cloth simulations for 91 frames, then holds
the same component and particle state while switching from the stock cloth
deformer to the unchanged shared `DG_ClothMorphBody`. Each pair has an identical
cloth-state hash. Both simulations retain all 1,060 particles.

The eleven cases per mesh cover neutral, each of five body shapes at one,
all five at half and one, and three cloth-disabled controls. The five shapes
are glute size, hip size, thigh tone, waist width and body tone. The trial uses
the same `CSS_Morph_*` variables as the runtime.

`check_peony_cloth_gpu.py` compares 44 GPU readbacks against separately exported
render-morph deltas. All position, normal and tangent values are finite. The
largest position residual is **0.014997 cm**, below the 0.05 cm comparison
tolerance. Non-cloth vertex positions are unchanged in every pair. Neutral and
cloth-disabled cases introduce no correction. Both negative controls fail when
the glute correction is replaced with the stock output.

The normal-mesh all-max stock/trial PNGs were inspected. The trial moves the
upper green panel surfaces over the enlarged body where the stock output
intersects. Flat diagnostic colors expose coverage; they do not validate the
actual garment materials or establish full-body collision acceptance.

## Reproduction and preservation

The first rendering attempt stopped before readback because the material
readiness helper rejected a diagnostic material outside its permitted study
namespace. `attempt1` retains that script, command, log and exit result. The
successful retry reuses the existing permitted diagnostic materials and waits
for their morph-capable shaders. No helper guard was relaxed for those materials.

The rig-audit helper permits the two exact Peony study mesh paths in addition
to its existing Genessa path. Commandlet, game-thread and preview-world checks
remain. This is an editor probe change, not a CSS runtime change.

The retry exits zero and `gpu-comparison.json` passes. Its
`validation-receipt.json` pins 129 evidence/recipe files and 249 protected input
hashes. Original meshes, shared graph and diagnostic material packages remain
unchanged. Keep the shader cache for the next GPU study to avoid rebuilding it.

Windows cooking/readback, moving-pose contact, actual materials, performance and
in-game regression checks remain. The accessory walking-contact defect is not
fixed by this cloth correction.
