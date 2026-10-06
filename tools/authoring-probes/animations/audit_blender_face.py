"""Audit evaluated facial weights over an already sampled body clip, without saving."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('body', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--mesh', required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
if bpy.context.preferences.filepaths.use_scripts_auto_execute:
    raise RuntimeError('Start Blender with --disable-autoexec')
if args.output.exists():
    raise FileExistsError(args.output)
body_bytes = args.body.read_bytes()
body = json.loads(body_bytes)
source = Path(body['provenance']['source']).resolve(strict=True)
source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
if source_hash != body['provenance']['sha256']:
    raise ValueError('Body sample and source hashes differ')
bpy.ops.wm.open_mainfile(filepath=str(source), use_scripts=False)
scene = bpy.context.scene
start, end = body['provenance']['scene_frames']
fps = scene.render.fps / scene.render.fps_base
count = body['frameCount']
if (scene.name != body['provenance']['scene'] or fps != body['provenance']['scene_rate']
        or not 2 <= count <= 100001 or not end > start
        or abs((end-start)/fps-body['duration']) > 1e-7):
    raise ValueError('Body timing does not match the saved scene')
mesh = bpy.data.objects[args.mesh]
if mesh.type != 'MESH':
    raise ValueError('Expected a mesh')
keys = mesh.data.shape_keys
if keys is None or not keys.use_relative:
    raise ValueError('Expected a mesh with relative facial shapes')
if keys.animation_data and keys.animation_data.drivers:
    raise ValueError('Facial drivers require a dependency review')
names = [key.name for key in keys.key_blocks if key != keys.reference_key]
if not names or len(names) > 2048:
    raise ValueError('Unexpected facial shape count')
active = []
data = keys.animation_data
if data:
    if data.action:
        active.append((data.action, data.action_slot))
    if data.use_nla:
        solo = any(track.is_solo for track in data.nla_tracks)
        for track in data.nla_tracks:
            if track.mute or (solo and not track.is_solo):
                continue
            for strip in track.strips:
                if not strip.mute and strip.action:
                    active.append((strip.action, strip.action_slot))
curves = []
for action, slot in active:
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                if not slot or bag.slot_handle != slot.handle:
                    continue
                for curve in bag.fcurves:
                    curves.append(dict(action=action.name, slot=slot.identifier, path=curve.data_path,
                        index=curve.array_index, mute=curve.mute, extrapolation=curve.extrapolation,
                        modifiers=[modifier.type for modifier in curve.modifiers],
                        keys=[dict(co=list(key.co), left=list(key.handle_left), right=list(key.handle_right),
                                   interpolation=key.interpolation) for key in curve.keyframe_points]))
samples = []
# Include half-frames to detect motion hidden by integer-frame-only inspection.
for index in range(2*(count-1)+1):
    frame = start + (end-start)*index/(2*(count-1))
    scene.frame_set(math.floor(frame), subframe=frame-math.floor(frame))
    evaluated = keys.evaluated_get(bpy.context.evaluated_depsgraph_get())
    values = [evaluated.key_blocks[name].value for name in names]
    if any(not math.isfinite(value) for value in values):
        raise ValueError('Nonfinite evaluated facial weight')
    samples.append(dict(seconds=(frame-start)/fps, values=values))
summary = []
for index, name in enumerate(names):
    values = [sample['values'][index] for sample in samples]
    if any(value != 0 for value in values):
        summary.append(dict(name=name, minimum=min(values), maximum=max(values),
                            varying=min(values) != max(values)))
if hashlib.sha256(source.read_bytes()).hexdigest() != source_hash or args.body.read_bytes() != body_bytes:
    raise RuntimeError('An input changed during facial evaluation')
report = dict(source=str(source), sha256=source_hash, blender=bpy.app.version_string,
              body=str(args.body.resolve()), body_sha256=hashlib.sha256(body_bytes).hexdigest(),
              mesh=mesh.name, scene_frames=[start,end], scene_rate=fps, duration=body['duration'],
              autoexec_enabled=False, names=names, samples=samples, nonzero=summary,
              active_curves=curves,
              scope='Saved scene evaluation with active actions and NLA unchanged. '
                    'Half-frame diagnostic, not a runtime curve or interpolation acceptance.')
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open('x') as stream:
    json.dump(report, stream, allow_nan=False)
    stream.write('\n')
print(json.dumps(dict(samples=len(samples), shapes=len(names), nonzero=summary)))
