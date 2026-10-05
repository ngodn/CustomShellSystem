"""Blender 5.2: inspect source rigs/actions without saving or running embedded scripts."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import bpy


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
if bpy.context.preferences.filepaths.use_scripts_auto_execute:
    raise RuntimeError('Start Blender with --disable-autoexec')
source = args.source.resolve(strict=True)
before = hashlib.sha256(source.read_bytes()).hexdigest()
if args.output.exists():
    raise FileExistsError(args.output)
if source.suffix.lower() == '.blend':
    bpy.ops.wm.open_mainfile(filepath=str(source), use_scripts=False)
elif source.suffix.lower() == '.fbx':
    bpy.ops.import_scene.fbx(filepath=str(source))
else:
    raise ValueError('Expected a Blender or FBX source')

rigs = []


def animation_state(owner):
    data = owner.animation_data
    if data is None:
        return None
    return dict(action=data.action.name if data.action else None,
                action_slot=data.action_slot.identifier if data.action_slot else None,
                blend_type=data.action_blend_type, influence=data.action_influence,
                use_nla=data.use_nla,
                nla=[dict(name=t.name, mute=t.mute, solo=t.is_solo,
                          strips=[dict(name=s.name, type=s.type,
                                       action=s.action.name if s.action else None,
                                       scene_range=[s.frame_start, s.frame_end],
                                       action_range=[s.action_frame_start, s.action_frame_end],
                                       scale=s.scale, repeat=s.repeat, mute=s.mute)
                                  for s in t.strips]) for t in data.nla_tracks],
                drivers=[dict(path=d.data_path, index=d.array_index,
                              expression=d.driver.expression, valid=d.driver.is_valid,
                              mute=d.mute) for d in data.drivers])


for obj in bpy.data.objects:
    if obj.type != 'ARMATURE':
        continue
    rigs.append(dict(name=obj.name, matrix_world=[list(row) for row in obj.matrix_world],
                     bones=[dict(name=bone.name, parent=bone.parent.name if bone.parent else None,
                                 rest_matrix=[list(row) for row in bone.matrix_local])
                            for bone in obj.data.bones],
                     active_action=obj.animation_data.action.name
                     if obj.animation_data and obj.animation_data.action else None,
                     animation=animation_state(obj),
                     constraints=[dict(bone=b.name, name=c.name, type=c.type,
                                       influence=c.influence, mute=c.mute,
                                       target=c.target.name if getattr(c, 'target', None) else None,
                                       subtarget=getattr(c, 'subtarget', None))
                                  for b in obj.pose.bones for c in b.constraints]))
actions = []
for action in bpy.data.actions:
    curves = []
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    keys = [list(key.co) for key in curve.keyframe_points]
                    curves.append(dict(path=curve.data_path, index=curve.array_index,
                                       keys=keys, interpolation=sorted({key.interpolation
                                                                      for key in curve.keyframe_points})))
    actions.append(dict(name=action.name, frame_range=list(action.frame_range), curves=curves,
                        slots=[dict(identifier=s.identifier, target_id_type=s.target_id_type)
                               for s in action.slots]))
if hashlib.sha256(source.read_bytes()).hexdigest() != before:
    raise RuntimeError('Source bytes changed')
report = dict(source=str(source), sha256=before, blender=bpy.app.version_string,
              autoexec_enabled=bpy.context.preferences.filepaths.use_scripts_auto_execute,
              rigs=rigs, actions=actions,
              scenes=[dict(name=s.name, frames=[s.frame_start, s.frame_end], fps=s.render.fps,
                           fps_base=s.render.fps_base, unit_scale=s.unit_settings.scale_length)
                      for s in bpy.data.scenes],
              meshes=[dict(name=obj.name, shape_keys=[key.name for key in obj.data.shape_keys.key_blocks]
                           if obj.data.shape_keys else [])
                      for obj in bpy.data.objects if obj.type == 'MESH'],
              scope='Source metadata and keyframes only; no scripts, retargeting or visual acceptance.')
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open('x') as stream:
    json.dump(report, stream, indent=2)
    stream.write('\n')
print(json.dumps(dict(rigs=[(r['name'], len(r['bones'])) for r in rigs],
                      actions=[(a['name'], a['frame_range'], len(a['curves'])) for a in actions])))
