# Four video references: deformation, joints and footwear

Reviewed 2026-10-08 for the [proposed three-outfit goal](../development/three-outfit-rig-physics-goal.md). These are reference techniques, not evidence that our outfits have the same defects.

## Sources and method

| ID | Local source in `/home/eins0fx/Downloads` | Duration |
| --- | --- | --- |
| V1 | `peanar3d2026_10_07_23_10_44970c7a62-ed9d-44a8-8723-dc9f468523f4.mp4` | 50.20 s |
| V2 | `peanar3d2026_10_07_23_10_270fdf9ccd-d3e8-4d5b-8c04-9b916c62ac94.mp4` | 38.25 s |
| V3 | `2amgoodnight2026_10_07_23_10_19cafb8b55-94fd-46ac-a3d4-07f8fafb5ec7.mp4` | 26.94 s |
| V4 | `2amgoodnight2026_10_07_23_10_15aae9660f-99d9-4c0d-aa75-c7fe479e402f.mp4` | 27.63 s |

All four are 720x1280, 30 fps with AAC audio. Reviewed six overview sheets sampled throughout the clips at three-second intervals, then selected full-resolution frames around the demonstrated operations. Extracted 36 detail frames; direct detail inspection covered V1 at 34/40 s, V2 at 23/32 s, V3 at 7/13/17 s and V4 at 19/25 s.

Audio review used local English speech transcription with faster-whisper 1.2.1, CTranslate2 4.8.2 and base.en CPU int8, checked against visible operations and UI labels. This was not a direct listening pass. Automatic transcript wording can be wrong; uncertain words are not treated as technical evidence. The clips' music and sound design are not relevant to this rig study.

Evidence remains local in [work/rig-physics-review-20261008](../../work/rig-physics-review-20261008): [source hashes and probes](../../work/rig-physics-review-20261008/sources.json), [timestamped transcripts](../../work/rig-physics-review-20261008/audio-transcripts.json), overview/detail images and extracted audio. Source videos were not modified or published. Tool method: [faster-whisper](https://github.com/SYSTRAN/faster-whisper), [ffprobe](https://ffmpeg.org/ffprobe.html).

## V1: cloth-driven deformation proxy

- **0-5 s:** a stylized body demonstrates local belly indentation and secondary motion.
- **5-17 s:** grow a surface selection around the belly and assign a `belly` vertex group.
- **17-33 s:** duplicate and separate the patch, close its back and create a `pin` group for its anchored boundary.
- **33-36 s:** add **Cloth** physics. The narration directs viewers to settings in the original description, which is not supplied. Exact solver settings cannot be recovered from that instruction.
- **36-43 s:** add Surface Deform to the visible body, target the duplicate, restrict it to `belly` and bind. The 40 s frame shows this relationship.
- **43-50 s:** hide the proxy and use the hand as a collision object to demonstrate the result.

The useful principle is separating a constrained simulation surface from the visible mesh and limiting deformation to an intended region. This is **not** Blender's Soft Body modifier or a spring-bone tutorial. A Blender Surface Deform binding does not by itself become a UE runtime deformer on export. A later experiment would need an explicitly supported Unreal representation and measured cost. First inspect our existing regional springs, cloth and corrective shapes; do not replace them on the strength of this clip.

## V2: foot-to-shoe geometry fitting

- **0-5 s:** the foot visibly intersects a closed pump.
- **5-18 s:** select the ankle loop/inner region and create a sculpt face set.
- **18-25 s:** use the Sculpt Pose brush with Face Sets to tilt the foot into the shoe. The narration explicitly describes working without a rig.
- **25-38 s:** adjust the foot in side view, then use Topology origins for local posing. The 32 s panel targets geometry with Rotate/Twist and one Pose IK segment.

The transferable lesson is to fit the ankle, arch, forefoot and toes to the intended shoe before attempting collision fixes. This is **not proof of runtime foot IK**. For a skinned character, a geometry adjustment must remain coherent with the bind pose, weights, morph deltas and garment fit. Runtime heel height and floor contact require separate checks. Preserve the accepted shoe design.

## V3: finger-joint placement

- **0-4 s:** poorly positioned finger bones are shown.
- **4-10 s:** select a joint edge loop and use Mesh > Snap > Cursor to Selected.
- **10-14 s:** select the bone endpoint and use Armature > Snap > Selection to Cursor.
- **14-20 s:** repeat joint placement; a fingertip can use a selected vertex.
- **20-27 s:** finished-hand demonstration and outro.

This supplies a practical authoring check for joint centers. It does not establish bone roll, anatomical accuracy, normalized weights, thumb opposition or weapon grip. An edge-loop center is a useful starting point, not a universal joint center. Moving a shipping bone requires coordinated bind/animation/socket review. Our goal should inspect both hands and distinguish a bad pose from a bad joint or weight before editing.

## V4: footwear design and toe clearance

- **0-11 s:** a stylized open-forefoot shoe is contrasted with restrictive footwear.
- **11-18 s:** alternate curved supports and shoe references are shown.
- **18-28 s:** the narration discusses distributed support and toe freedom; close views show the ornamental heel and exposed toes.

The useful questions are whether toes have clearance, what surfaces should contact the floor, and whether articulation fits the design. The clip is a design demonstration, **not a validated load-distribution simulation or full foot-rig tutorial**. It does not justify copying its shoes or asserting biomechanical realism.

## What changes in our plan

1. Diagnose fit, pivots and skinning before adding collision or more springs.
2. Identify the actual simulation owner for each body, hair, garment and accessory region.
3. Compare body shape, garment shape and collider shape together, including permitted morph combinations.
4. Test the same pose in menu and world, then continuous motion and lifecycle transitions.
5. Treat the proxy idea as a bounded experiment only if existing Unreal mechanisms cannot solve a reproduced defect.

The clips do not supply an Unreal implementation, export recipe, exact cloth parameters or performance results. [UE 5.6 research](2026-10-08-rig-physics-ue56.md) and the [current asset inventory](2026-10-08-three-outfit-rig-baseline.md) provide the project-specific constraints.
