"""Blender 5.2: sample evaluated source rigs, retaining NLA timing and constraints."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--rig', required=True)
parser.add_argument('--start', type=float, required=True)
parser.add_argument('--end', type=float, required=True)
parser.add_argument('--sample-rate', type=int, choices=(25, 30, 60), required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
if bpy.context.preferences.filepaths.use_scripts_auto_execute:
    raise RuntimeError('Start Blender with --disable-autoexec')
source = args.source.resolve(strict=True)
before = hashlib.sha256(source.read_bytes()).hexdigest()
if args.output.exists():
    raise FileExistsError(args.output)
bpy.ops.wm.open_mainfile(filepath=str(source), use_scripts=False)
rig = bpy.data.objects[args.rig]
if rig.type != 'ARMATURE':
    raise ValueError('Expected an armature')
scene = bpy.context.scene
rate = scene.render.fps / scene.render.fps_base
if not all(math.isfinite(v) for v in (args.start, args.end, rate)) or args.end <= args.start:
    raise ValueError('Invalid scene range')
duration = (args.end - args.start) / rate
intervals = round(duration * args.sample_rate)
if not 1 <= intervals <= 100000 or abs(intervals / args.sample_rate - duration) > 1e-6:
    raise ValueError('Sampling would change the scene duration')
if abs(scene.unit_settings.scale_length - .01) > 1e-6:
    raise ValueError('This source adapter requires the verified centimetre scene units')
if rig.animation_data and rig.animation_data.drivers:
    raise ValueError('Source rig drivers require a separate dependency review')
names = [b.name for b in rig.data.bones]
parents = [names.index(b.parent.name) if b.parent else -1 for b in rig.data.bones]
if any(not -1 <= p < i for i, p in enumerate(parents)):
    raise ValueError('Expected parent-first bone order')
# The supplied centimetre scenes mirror Y relative to decoded source-game positions.
basis = Matrix.Diagonal((1, -1, 1, 1))
worst_reconstruction = 0.0


def transform(matrix, name):
    global worst_reconstruction
    translation, rotation, scale = matrix.decompose()
    rotation.normalize()
    if any(not math.isfinite(v) for row in matrix for v in row) or min(abs(v) for v in scale) <= 1e-8:
        raise ValueError(f'Invalid source transform at scene frame {scene.frame_current_final}: '
                         f'{name}, scale {tuple(scale)}')
    rebuilt = Matrix.LocRotScale(translation, rotation, scale)
    error = max(abs(matrix[i][j] - rebuilt[i][j]) for i in range(4) for j in range(4))
    worst_reconstruction = max(worst_reconstruction, error)
    if error > .001:
        raise ValueError(f'Source shear cannot be represented by UE transforms: {error}')
    return dict(Translation=dict(zip('XYZ', translation)),
                Rotation=dict(zip('XYZW', (rotation.x, rotation.y, rotation.z, rotation.w))),
                Scale3D=dict(zip('XYZ', scale)))


def local_transforms(world):
    return [transform(world[p].inverted() @ matrix if p >= 0 else matrix, name)
            for name, p, matrix in zip(names, parents, world, strict=True)]


reference = local_transforms([basis @ rig.matrix_world @ b.matrix_local @ basis
                              for b in rig.data.bones])
bones = [dict(name=name, parent=parent, reference=ref, animated=True,
              rotations=[], translations=[], scales=[],
              rotationTimes=[], translationTimes=[], scaleTimes=[])
         for name, parent, ref in zip(names, parents, reference, strict=True)]
for sample in range(intervals + 1):
    frame = args.start + sample * rate / args.sample_rate
    integral = math.floor(frame)
    scene.frame_set(integral, subframe=frame-integral)
    evaluated = rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
    values = local_transforms([basis @ evaluated.matrix_world @ evaluated.pose.bones[n].matrix @ basis
                               for n in names])
    for bone, value in zip(bones, values, strict=True):
        for channel, key in [('rotations', 'Rotation'), ('translations', 'Translation'), ('scales', 'Scale3D')]:
            bone[channel].append(value[key])
if hashlib.sha256(source.read_bytes()).hexdigest() != before:
    raise RuntimeError('Source file changed during sampling')
document = dict(schema=1, sourceGame='BlenderScene', asset=f'{source.name}:{args.rig}',
                skeleton=args.rig, frameCount=intervals+1, duration=duration,
                rateScale=1, interpolation='Linear', endpointSampleRate=args.sample_rate,
                trackCount=len(bones), bones=bones,
                provenance=dict(source=str(source), sha256=before, scene=scene.name,
                                scene_rate=rate, scene_frames=[args.start, args.end],
                                centimetres_per_unit=1, basis=[list(row) for row in basis],
                                nla_preserved=True, constraints_evaluated=True,
                                max_transform_reconstruction_error=worst_reconstruction),
                scope='Evaluated source scene samples. Not retargeted, cooked or visually accepted.')
args.output.mkdir(parents=True)
with (args.output/'source-tracks.json').open('x') as stream:
    json.dump(document, stream)
    stream.write('\n')
print(json.dumps(dict(rig=args.rig, frames=intervals+1, bones=len(bones), duration=duration,
                      max_transform_reconstruction_error=worst_reconstruction)))
