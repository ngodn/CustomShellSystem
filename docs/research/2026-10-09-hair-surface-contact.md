# Hair surface checks, October 9

These checks use the current migrated Eve Black Pearl facial mesh and Commander
White E9 facial mesh, with their existing hair rigs. They do not replace game
acceptance, the complete post-process graph or testing the other Eve variants.
The installed assets are unchanged by this investigation.

## Captures and measurement

Evidence under `work/three-outfit-rig-physics`:

| Directory | Evidence |
| --- | --- |
| `hair-surfaces1` | Three synthetic 60-degree head-axis turns per character, 1,446 samples at 60 Hz; source hashes preserved |
| `hair-surface-contact1` | Initial nearest-normal screen, fourteen winding/ray witnesses and eight diagnostic renders |
| `hair-surface-contact2` | Same head-turn data with a matching unsolved-pose control |
| `hair-animations1` | Walk, jog and sprint per character, another 1,446 samples at 60 Hz; seven mesh/rig/animation source hashes preserved |
| `hair-animation-contact1` | Seven selected frames per gait, matching unsolved controls, sixteen witnesses and centered diagnostic renders |

The exact engine is UE 5.6.1 CL44394996. Animation inputs use compressed pose
evaluation with retargeting enabled and the target mesh supplied. Editor-loaded
compression is not a game-only container playback test. Both characters use the
CSS UI hair values of stiffness 200, damping 24 and zero gravity. Each four-second
run executes 480 fixed steps with one reset. Non-hair local transforms retain the
supplied input, and the rigs' initial transforms are unchanged.

Blender 5.2.2 reconstructs weighted render geometry and checks every vertex with
a nonzero named hair-bone influence at the selected frames. Reference-pose
reconstruction error is below 0.000000001 cm. The comparison body is identical
with and without hair simulation. Enclosed/outside tetrahedron controls and a
nonidentity weighted-skinning control pass.

Signed-nearest distance is only a screening value. Commander White's left
fringe has a synthetic-turn false alarm: winding stays near zero and five of
seven rays escape. Eve's gait ponytail has a false 31.9 cm reading: winding is
0.0063 and all seven rays escape. Several Commander White sections are already
inside the supplied body in the unsolved pose, although their nearest-face
normals incorrectly classify them as outside. Do not quote those screen maxima
as new penetration depths or tune collision from them.

Independent witnesses do support smaller intersections introduced by the hair
solver: Eve's fringe during walk reaches 0.3325 cm, Commander White's left
fringe during jog reaches 1.4803 cm, and a main-hair point during sprint changes
from 0.0631 to 1.7836 cm. Their winding values are near one after simulation.
Opaque diagnostic renders were reviewed. They omit texture alpha, eyes,
garments and all morph settings; apparent empty eye sockets are excluded eye
sections, not a new eye regression. The first fixed-camera gait render clipped
the lowered head; the centered render supersedes it. Synthetic files labeled
`front` actually show the rear and must not be used as front-view evidence.

The measurement uses Blender's documented
[BVH nearest-point and ray queries](https://docs.blender.org/api/current/mathutils.bvhtree.html).
The animation API is verified against local 5.6.1 `AnimPose.h` and Epic's
[5.6 animation evaluation interface](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AnimSequenceBase?application_version=5.6).

## Commander White head-contact trial

The existing short hair chains collide with head/neck capsules. The ponytail
uses all fourteen capsules and a separate hair-thickness margin. A private
trial in the outfit's `work/hair-head-contact1` adds 1 cm to the head capsule
radius and subtracts 1 cm from the ponytail head margin. Other values remain
unchanged, including iterations and solver count. No asset is saved or installed.

All 723 paired gait samples preserve the incoming body pose and all 6,507
ponytail-bone samples exactly. Short-chain transforms change, including some
first-frame transforms. A visual silhouette check is therefore required before
acceptance. The original left-fringe witness falls from 1.4803 to 0.5701 cm;
the main-hair witness falls from 1.7836 to 0.8123 cm. Both still lie inside the
supplied face. The ponytail witnesses remain identical. One already-interior
flyhair witness worsens from 7.3369 to 7.3966 cm; it is not a new outside-to-inside
transition. The one-centimeter trial is not an accepted repair.

The second comparison, `work/hair-head-contact2` in the outfit, increases head
radius by 2 cm and reduces the ponytail margin by the same amount. All 723 paired
samples and 6,507 ponytail-bone samples again pass exact isolation. Decompressed
render geometry is identical. The two original face witnesses now lie outside:
the left-fringe point is 0.4851 cm from the surface (winding -0.0078), and the
main-hair point is 0.5404 cm away (winding -0.0072). Three/four rays escape.
Signed-nearest candidate counts decrease in all four short-hair sections and
remain identical for both ponytail sections. Those counts are screening only.

Six centered renders were inspected. The fringe still follows the original
overall hairstyle, but a temple gap and exposed strand ends warrant a root-fit
and textured check. First-frame short-chain changes also remain. Neither trial
has been cooked or installed, and the second is not an accepted final fix.
Do not increase the radius further from the remaining nearest-normal maxima.
Next inspect the affected roots with original alpha/materials, then validate
idle, head turns, attacks and continuous motion if the adjustment is retained.
Preserve the current installed hair until those checks pass.

Cleanup removed 751,543,230 logical bytes of derived data from the completed
synthetic capture, gait baseline and first head-clearance trial. Their sources,
recipes and captures retain their hashes. Immediate free space was unchanged,
so this is not a measured disk-space recovery claim. The second trial's cache
is retained for the next comparison. Receipt:
`work/three-outfit-rig-physics/cleanup-hair-surface-cache1.json`.
