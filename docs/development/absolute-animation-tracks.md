# Preserve animated virtual bones in diagnostics

The old `CSS_ANIMATION_POSES=1` exporter samples the converted raw skeleton.
For the game's H2 idle this gives 1199 bones but drops eight animated virtual
tracks used by the player hand-IK graph. That output remains useful for raw
bone tests, but is not a complete input to a hand-contact replay.

`CSS_ABSOLUTE_TRACKS=1` adds a separate diagnostic export mode to MeshExport.
It reads every mapped ACL track through the existing native decoder ABI,
including virtual-bone indices, and writes `absolute-tracks.json`. The output
contains five exact sample times, track names, local transforms and the authored
retarget reference. Unmapped bones are omitted. This is neither a full-pose
export nor an animation ready to install.

The mode rejects additive sequences and simultaneous animation modes, validates
track indices against both names and reference transforms, bounds allocation,
and rejects non-finite or non-unit decoded transforms. It does not reinterpret
additive scale deltas as absolute scale.

## Verified H2 fixture

Built using the existing net10.0 project with mise-installed .NET 10.0.401.
Build exits zero; 32 warnings come from the referenced conversion project.
The game clip is
`/Game/Sparta/Characters/Shells/_Shared/Animation/Locomotion/Idles/A_Shared_Idle_H2`.
It has 384 ACL samples at 30 Hz, 102 mapped tracks, and eight virtual tracks:
the two ground references, both hands, both relative-hand targets and both elbows.

The new mode and the prior exact decoder agree on all 470 ordinary-bone samples
(94 tracks at five times), with zero translation, rotation or scale differences.
Actual conflicting-mode and additive-input runs fail with the expected errors
and create no absolute-track output.
The comparison checker also rejects a fixture with all virtual tracks removed,
reproducing the original diagnostic omission.

From the CSS repository:

```sh
python3 tools/check_absolute_tracks.py \
  work/grip-grounding-v1/active-h2-absolute-tracks-v1/absolute-tracks.json \
  work/grip-grounding-v1/active-h2-exact-v2/animation-poses.json \
  --expected-virtual-tracks 8
```

Evidence lives in `work/grip-grounding-v1/`:

- `absolute-track-exporter-v1/`: build command, environment, log, binary and
  two rejection runs with `guard-results.json`.
- `active-h2-absolute-tracks-v1/`: successful export, exact invocation with
  environment override and differential validation.
- `active-h2-exact-v2/`: unchanged decoder comparison fixture.

The first H2 export attempt in `active-h2-exact-v1/` omitted its mode environment
variable and failed in the unsupported GLTF animation path. Retain that failure
record; use v2. Command records must include environment overrides.

Next include these virtual tracks and the authored retarget source in the Unreal
graph test. This exporter fix is not evidence that the in-game fingers or
supporting-hand grip are repaired. Production mesh, game package and settings
remain unchanged.
