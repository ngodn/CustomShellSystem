# Slipstream boot fit and reversible body coverage

`slipstream-bootfit2` restores the source heel profile and fits the shafts to
the accepted Eve body. The neutral authoring checks pass, but the boots are
not assembled, cooked or installed. The installed Peony trial is unchanged.

## Source and fit

The source is the verified Normal Slipstream outfit, `slipstream-retail-02`.
Its body ends at 49.14 cm in source coordinates, so it does not provide feet or
lower legs beneath the boots. The aligned source skin begins around 46.16 cm.
The new outfit continues to use the complete accepted Eve body for barefoot
restoration; it does not replace that body with the incomplete source mesh.

The initial joint-alignment study distorted the heel/toe profile while adapting
the source heeled skeleton to Eve's flat-foot bind. The new fitter anchors each
original boot at the target ankle, aligns horizontal foot direction and scales
by the source/target foot-segment length ratio. It blends into the existing
shaft alignment above the ankle. The lower heel profile is preserved, rather
than stretched to enclose a flat bare foot.

Each shaft uses the continuous displacement-field solver from the torso work.
Its radial samples use the actual same-side leg surface and 4 mm clearance.
The first candidate still had 23 boot/body triangle overlaps despite a passing
vertex/interior sample screen. Constraints at the intersecting body triangles
removed those overlaps in one refinement per side (8 to 0 left, 15 to 0 right).
The fitted field carries boot straps, fasteners and shin insets with the shell.
It changes only boot-owned vertices. This is offline work, not a new runtime
solver or an increase in gameplay physics iterations.

## Independent reload results

| Check | Result |
| --- | --- |
| Original indexed surfaces, UVs, named weights and raw fields | Preserved across all 23 source parts |
| Accepted Eve body | 36,787 points and 61,814 triangles preserved |
| Non-boot clothing vertices | All 20,976 unchanged from the retained torso fit |
| Boot-owned triangles | 6,014 retained |
| Contact samples against visible skin | 28,516 checked; no inward samples beyond the 0.5 mm threshold |
| Boot/visible-body triangle overlaps | 0 |
| Original lower heel shape | 582 points per side match a proper rigid transform and uniform scale |
| Maximum lower-profile residual | 0.000001533 cm |

The heel-shape check solves its own least-squares rigid transform and rejects
reflection or deformation. It does not merely compare a writer's reported
coordinates. Front, side and rear close views were inspected. Their colors are
diagnostic, and final material opacity, shading and motion remain unverified.

The first contact checker used only visible body faces for nearest queries.
This created an artificial boundary and falsely classified some points below
the coverage cutoff as inside visible skin. The retained checker uses the
complete body for nearest queries, classifies its returned face by visibility,
and separately tests boot triangles against the visible-body triangle tree.
The triangle test caught real intersections that the sampled check missed.
See Blender's [BVHTree query API](https://docs.blender.org/api/current/mathutils.bvhtree.html).

## Coverage and height

The authored recipe identifies 5,328 original body faces wholly below 42 cm.
It leaves 56,486 body faces visible and records the exact original face IDs.
Boots shown should hide only those covered faces; boots hidden should restore
them and return to barefoot height. The source body omission supports this
coverage design, but final boot materials still need verification before relying
on it in the packaged outfit. No body faces have been deleted or deformed.

The measured boot lift is **11.81042492389679 cm**. CSS's old 10 cm limit would
reject this outfit's automatic footwear rule. Commit `bf76c02` makes the bounded
range consistently -20 to 20 cm in catalog validation, saved customization,
runtime placement, command clamping and the existing Ground Height slider.
Legacy values, half-centimeter manual steps, reset behavior and automatic
shown/hidden selection remain unchanged. This is a CSS feature limit, not a
claim about an Unreal Engine limit.

The C++23 Windows core candidate is
`43b8f58185bcb692acc1d9bd7b23f43259d022ee02641aa100b375d14ad4daef`.
The first build invocation terminated after linking with exit 143; repeating
the same target completed with exit 0 and no remaining build work. The data,
controls, profiles, animations, facial-library and Astral-adapter suites pass.
The data suite also passes the added 11.81/20 cm automatic-footwear cases.
Tests cover bounds, saved heights, repeated application, boot removal and
baseline restoration. The changed runtime is staged, **not installed**.

## Retained artifacts and remaining work

Paths relative to the Eve mod:

- `tools/fit_slipstream_boots.py`: profile and shaft fit.
- `tools/prepare_slipstream_footwear.py`: independent contact/profile checks and
  exact-face visibility recipe.
- `work/slipstream-bootfit2.blend` and `.json`: retained fitted candidate.
- `work/slipstream-footwear4.json`: coverage, contact and heel-profile evidence.
- `work/slipstream-bootfit2-{source,alignment}-check.json`: source/body reload checks.
- `work/slipstream-bootfit-body-review3`: inspected diagnostic views.
- `work/slipstream-boots1/receipt.json`: frozen scripts, evidence identities and
  cleanup records.

CSS build/test logs and the staged DLL are under
`work/three-outfit-rig-physics/slipstream-footwear1` in the CSS repository.

Next, bind the complete garment set to the target rig and transfer body-shape
controls. Integrate the exact body coverage and automatic height into assembly
and CSS customization. Validate posed foot/leg contact, barefoot restoration,
materials, all five palettes, physics, facial and Astral variants, Windows
cooking and gameplay. None of those steps is proved by this neutral fit.
