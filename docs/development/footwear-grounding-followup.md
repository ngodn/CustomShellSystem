# Footwear and grounding after accepted V44

2026-09-21: The user accepted the 3 cm downward world-mesh trial and asked us
to stop fine-tuning contact. Slight visual floor overlap is acceptable; deeply
buried shoes are not. Native integration and a metadata-only package are built
and installed through a normal restart. World application and disable/re-enable
restoration pass. Ground-contact amount tuning is closed.

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

## Isolated support candidate

`tools/authoring-probes/footwear/build_heel_supports.py` creates
`CSS_SeduXtress_HeelSupportsV45C.blend` beside the preserved source. It adds a
separate shoe object with 168 vertices, 328 exported triangles, the existing
footwear material and two influences per vertex. Each collar seats into the
existing sole; the tapered lower cap reaches that shoe's authored sole plane.
Original objects and the source blend are unchanged. This was the pre-cook checkpoint; the short-path version is now installed as described below.

The optional exporter flag `--heel-supports` appends the object to material
section 20, so it shares the existing shoes toggle without another slot. The
renderer handles the same visibility. Their external authoring-tool patches
are retained under `tools/authoring-patches/heel-support-{export,review}.patch`.

`heel-support-export-v2/validation.json` verifies exact preservation of all
133,066 original points, 193,261 triangles, weights, wedges/UVs, normals,
colors, 379 bones, 30 materials and every existing delta in all 22 morphs.
The supports are closed/manifold. Sixty-four collar checks cover neutral plus
31 retained actual component poses at body-tone 0 and 1. Maximum nearest shoe
distance is 0.3606 cm; signed distances remain inside the shoe surface with
the audited winding. This is a bounded attachment check, not full locomotion
or cooked/live acceptance.

Reviewed textured stills under the mod's `work/nextgen-audit` are
`heel-support-v45-front.png`, `heel-support-v45-back.png` and
`heel-support-v45b-side.png`. The subsequent C candidate explicitly zeros the
new public key before export; its base construction is unchanged. The final
`heel-support-v45c-side.png` is also reviewed. Its shape has been offered for
user review; no user verdict is recorded yet.

The isolated mesh `/Game/CSSAuthoring/DiagnosticReferences/SK_HeelSupportsV45C`
imports with a separate temporary Skeleton, then binds to the accepted V44
Skeleton, nine virtual bones, Physics Asset and post-process. The saved binding
passes in `heel-support-binding-v2/report.json`. All six protected assets,
including the shared Skeleton and accepted hand graph, retain their hashes.
Fresh loading now passes in `heel-support-binding-fresh-v1/report.json`, with
the saved mesh hash unchanged and no corrective writes during verification.
Both editor processes reached terminal exit 0. Cooking and live verification
remain required; the installed V44 core/package hashes were rechecked unchanged.

## Actual floor observation

`footwear-followup-v1/live-floor-v2.json` captures stationary V44, its actual
pose, component transform and six public morph values. The movement component
detects a walkable floor, with capsule floor distance 2.15 cm. This is the
capsule's distance, not the shoe-to-floor distance; see the pinned UE 5.6.1
`CharacterMovementComponent.h` and
[Epic's floor-result definition](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FFindFloorResult).

`heel-support-pose-v2/pose.json` replays the exported shoe skin with the actual
mesh bind and captured morphs. Four read-only scene traces then run at the
replayed low points. The final C candidate reproduces those points exactly in
`heel-support-pose-v3/comparison.json`. `floor-traces-v3.json` has stable transforms,
valid non-penetrating hits and agreement between trace Time and Distance:

| Point | Left clearance | Right clearance |
| --- | ---: | ---: |
| Existing shoe sole | 3.641 cm | 3.195 cm |
| Proposed heel cap | 2.789 cm | 3.277 cm |

These are one idle pose at one location. Supports restore missing geometry;
they do not correct the measured overall clearance. Do not apply the older
12 cm deformation or a universal offset based on these four traces alone.

## Failures retained so they are not repeated

- The first pose audit wrongly assumed footwear had no public morph deltas.
  `FBMBodyTone` moves the shoe by up to 0.20835 cm. The support now follows
  its heel seat's interpolated displacement, and replay uses captured morphs.
- The B candidate had its new public key active while exporting, which would
  bake its displacement twice. C explicitly starts it at zero; verification
  checks the export audit as well as preservation of old geometry. The passing
  C export and rejected B export are retained as positive/negative controls in
  `heel-support-export-v2/gates.json`.
- The first saved binding failed material-interface equality. Editing structs
  obtained by iterating the Python materials array did not update the array.
  Assigning each edited slot back by index fixes this; the v2 binding checks
  all slot names, imported names and actual interfaces against accepted V44.
- The bridge cannot encode the trace's actor-array argument. That request
  failed before invocation. Its reflected const-reference argument is an
  initialized output parameter, so the corrected request leaves it empty and
  uses `bIgnoreSelf=true` with the actual pawn context. The pinned
  `KismetTraceUtils.cpp` confirms the pawn is then excluded.
- Tracing with `PlayerMesh` found no floor because the actual profile ignores
  WorldStatic and WorldDynamic. Read-back of `Default__CollisionProfile`
  confirmed this. The subsequent query uses the existing `BlockAll` profile
  without changing any actor's collision settings.
- Net-quantized hit vectors decode as empty objects through this bridge.
  Reconstruct the vertical hit from trace Time and independently compare
  Distance; do not interpret empty vectors as zero coordinates.

## Next discriminating work

Check moving and barefoot contact before selecting a vertical correction.
The older shoe construction differs, so copying its below-ankle deformation
could distort Eve's preserved feet. Finish saved binding, cook/read-back and
live visibility/motion review for the isolated support candidate.

Any candidate must preserve the accepted body/hand data, provide front/side/rear
shoe renders and then in-world floor-contact checks. Keep changes isolated from
the baseline until reviewed. Preview lighting remains compiled locally and
queued after these appearance defects. Broader gameplay and modular Next-Gen
release acceptance remains open.

## Short-path live follow-up

2026-09-21: The supports are now cooked and installed in `SK_BlackPearl2`; see
[the combined package/live trial](short-mesh-package.md#live-trial-checkpoint).
The Steam rear still shows both supports. The world clip covers idle only, so
walking attachment and exact ground contact remain unverified.

`capture_footwear_floor.py --profile short1` accepts only the exact installed
mesh path, retaining V44 as its default profile. It checks velocity before and
after capture. `work/paths/live1/floor1.json` captures a stationary pose with
body tone zero and a capsule FloorDist of 2.239874 cm. This is not shoe clearance.
`pose1/pose.json` replays the protected original footwear and C support source
with those actual transforms/morph values; the Blender process exits zero.

The scene-trace step rejected this sample because the character transform had
changed before tracing. `traces1.requests.jsonl` retains the failed check. No
new shoe-to-floor clearance is claimed from this attempt. The analyzer and
tracer now accept explicit workspace inputs so a fresh stationary sample can
be used without overwriting historical evidence. Lighting input correction
and favorite sorting took priority following user feedback.

## Accepted 3 cm correction

`work/ground1/live.json`, `pose/pose.json` and `traces.json` capture one stable
world pose with valid floor hits. Shoe clearances are 4.025 cm left and 3.271 cm
right; support clearances are 3.121 and 3.212 cm. The underlying barefoot replay
in `bare/pose.json` has its low vertices about 0.30 cm above the shoe lows, at
different XY coordinates. It is not a separate barefoot floor trace.

`check_ground_offset.py` temporarily changed only the world mesh's relative Z
from -96 to -99 cm. The gameplay capsule, authored geometry and proportions
were not edited. The recorded 20-second world clip includes movement and
attacks; sampled frames and the before/lowered/restored Steam stills were
reviewed. The stills have different poses and views, so they are not a controlled
pixel comparison. `trial3/result.json` confirms the offset survived movement
and that exact -96 cm restoration succeeded. User: "i think its fine la bro for
now dont waste too much time on this". This closes visual amount tuning at -3 cm.

Each catalog variant can now declare `ground_offset_cm`, a finite number from
-10 to 10, defaulting to zero for existing packages. Negative values lower the
world mesh. CSS captures the first component height, applies without cumulative
drift and restores it when leaving the appearance. Cleanup preserves a height
changed by another owner. Preview framing and the gameplay capsule are unchanged.
This is an authored placement correction, not a per-frame floor solver.

The portable tests cover default/range/type validation, repeated application,
variant changes, native baseline reset, restoration, a new pawn baseline and
external-height ownership. Host tests, Windows development and shipping builds
all exit zero under `work/ground1/build`.

`prepare_ground_package.py` produces authoring `work/ground1/trio`, adding only
`ground_offset_cm: -3` to Black Pearl's manifest. The full manifest matches the
previous package after removing that field, packed resources match their staged
bytes, and `.ucas`/`.utoc` retain their original hashes. No mesh, texture,
animation, Skeleton or physics asset was recooked. The companion metadata lives
at authoring `work/ground1/metadata`; use it for subsequent release preparation.
The public rename and palettes remain queued after current fixes.

`deploy_ground_package.py` backs up the previous UI core, trio and saved state
under `work/ground1/live`, performs normal QuitGame, installs `css_core-ground1.dll`
and the new trio, verifies hashes/state preservation, and launches through Steam.
It exits zero. `deployment.json` is installation evidence; it does not claim
world application or appearance-switch lifecycle passed.

Live follow-up: `work/ground1/live/world.json` resolves the installed Black Pearl
mesh at relative Z -99 cm. The reviewed `installed.jpg` is a fresh Steam world
capture. `check_ground_restore.py` then verifies native CSS disable restores
the original game mesh at -96 cm, and enable restores Black Pearl at -99 cm.
The complete saved state matches before and after. Evidence is
`work/ground1/live/restore/verification.json`; the check exits zero. This covers
the accepted amount and one disable/enable cycle, not every travel transition.
