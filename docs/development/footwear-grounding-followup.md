# Footwear and grounding after accepted V44

V44 hand tuning is closed by user acceptance. Preserve the
[pinned baseline](v44-accepted-baseline.json), original body proportions,
accepted body dynamics and hair 200/24/0 while investigating footwear.

## Retained source evidence

The reviewed `footwear-v42-source-review-side.png` under the SeduXtress mod's
`work/nextgen-audit` shows the rear shoe sole without a narrow stiletto support.
The earlier untouched-source comparison records the same appearance. This is
not evidence of geometry being lost during the V44 cook. Do not enable the
separate optional pump pair over the Black Pearl shoes.

The v202 build inherits `Seductress_Genessa_Grounded.blend`. Its builder,
`CSS-eins0fx-collections/tools/fit_seductress_feet.py`, diagnoses source feet
weighted only to the knee. It refits the lower footwear, introduces calf/foot/
ball weights, and extends the below-ankle region toward the sole plane.
It does not lower the whole character or move its skeleton.

A fresh comparison of the retained before/after mesh JSONs is saved at
`work/grip-grounding-v1/footwear-followup-v1/v202-reference.json`:

| Part | Changed vertices | Minimum Z before | Minimum Z after |
| --- | ---: | ---: | ---: |
| Seductress_Boots.A | 318 / 1928 | 11.6843 | 0.0000 |
| Seductress_Source_Fitted_Heels | 1195 / 1951 | 11.9178 | 0.6055 |

Coordinates use the retained exporter convention (centimeters). Bone records,
faces and wedges match exactly. These values describe the older asset only;
they are not a measured V44 shoe-to-world-floor gap.

## Next discriminating work

Measure V44's evaluated soles and ankle/toe weights against its current floor
contact before applying any vertical correction. Treat the absent heel support
as a separate geometry task. The older shoe construction differs, so copying
its below-ankle deformation could distort Eve's preserved feet.

Any candidate must preserve the accepted body/hand data, provide front/side/rear
shoe renders and then in-world floor-contact checks. Keep changes isolated from
the baseline until reviewed. Preview lighting remains compiled locally and
queued after these appearance defects. Broader gameplay and modular Next-Gen
release acceptance remains open.
