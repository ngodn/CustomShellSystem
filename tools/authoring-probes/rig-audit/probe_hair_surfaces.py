"""Export current hair/body geometry and stepped head-turn poses in UE 5.6.1.

Direct production rigs with mesh-local input poses, matching the graphs' local
pose transfer and unchanged rig reference pose. This does not run the AnimBP.
"""
import gzip
import hashlib
import json
import math
import os
from pathlib import Path

import unreal


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def encode(t):
    return [t.translation.x, t.translation.y, t.translation.z,
            t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w,
            t.scale3d.x, t.scale3d.y, t.scale3d.z]


def decode(values):
    t = unreal.Transform()
    t.translation = unreal.Vector(*values[:3])
    t.rotation = unreal.Quat(*values[3:7])
    t.scale3d = unreal.Vector(*values[7:])
    return t


def key(name):
    return unreal.RigElementKey(name=name, type=unreal.RigElementType.BONE)


def set_value(rig, name, value):
    if not rig.set_variable_from_string(name, str(value)):
        raise RuntimeError('Cannot set ' + name)


def snapshot(raw):
    if raw.startswith('ERROR:'):
        raise RuntimeError(raw)
    return json.loads(raw)


out = Path(os.environ['CSS_HAIR_SURFACE_OUTPUT']).resolve(strict=True)
plan = json.loads((out / 'input.json').read_text())
content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()))
results, protected = [], {}
for item in plan['cases']:
    for package in (item['mesh'], item['rig']):
        source = (content / (package.removeprefix('/Game/') + '.uasset')).resolve(strict=True)
        protected[str(source)] = sha(source)
    mesh = unreal.load_asset(item['mesh'])
    blueprint = unreal.load_asset(item['rig'])
    if not isinstance(mesh, unreal.SkeletalMesh) or not isinstance(blueprint, unreal.ControlRigBlueprint):
        raise ValueError('Missing current mesh/rig')
    info = snapshot(unreal.CSSRigAuditLibrary.snapshot_mesh(mesh))
    bones = info['bones']
    slots = item['body_slots'] + item['hair_slots']
    geometry = snapshot(unreal.CSSRigAuditLibrary.snapshot_render_sections(mesh, slots, []))
    for group in geometry['groups']:
        if group['cloth_asset'] != -1:
            raise ValueError('This skinning diagnostic cannot evaluate cloth')
        group['vertices'] = [row[:6] for row in group['vertices']]
        del group['cloth_mappings'], group['morphs']
    geometry['bones'] = bones
    target = out / (item['id'] + '-geometry.json.gz')
    with gzip.open(target, 'xt', compresslevel=1) as handle:
        json.dump(geometry, handle, separators=(',', ':'), allow_nan=False)
    mesh_result = dict(id=item['id'], mesh=item['mesh'], rig=item['rig'], geometry=target.name,
                       geometry_sha256=sha(target), cases=[])
    motions = plan.get('motions', [dict(axis=axis) for axis in range(3)])
    for motion in motions:
        axis = motion['axis']
        animation = None
        if 'animation' in motion:
            package = motion['animation']
            source = (content / (package.removeprefix('/Game/') + '.uasset')).resolve(strict=True)
            protected[str(source)] = sha(source)
            animation = unreal.load_asset(package)
            if not isinstance(animation, unreal.AnimSequence) or animation.sequence_length <= 0:
                raise ValueError('Missing animation or invalid duration')
            options = unreal.AnimPoseEvaluationOptions(
                evaluation_type=unreal.AnimDataEvalType.COMPRESSED, should_retarget=True,
                extract_root_motion=False, optional_skeletal_mesh=mesh,
                retrieve_additive_as_full_pose=False, evaluate_curves=False)
        component = unreal.SceneComponent()
        rig = blueprint.create_control_rig()
        if not rig.rename(outer=component):
            raise RuntimeError('Cannot set transient owner')
        hierarchy = rig.get_hierarchy()
        mapped = [(bone, key(bone['name'])) for bone in bones if hierarchy.contains(key(bone['name']))]
        for bone, bone_key in mapped:
            expected_parent = bones[bone['parent']]['name'] if bone['parent'] >= 0 else 'None'
            if str(hierarchy.get_first_parent(bone_key).name) != expected_parent:
                raise ValueError('Rig/mesh parents differ: ' + bone['name'])
        mapped_names = {b['name'] for b, _ in mapped}
        weighted = {bones[index]['name'] for group in geometry['groups'] for weights in group['weights']
                    for index, weight in weights if weight}
        if not weighted.issubset(mapped_names):
            raise ValueError('Rig does not contain every weighted bone')
        reference = {b['name']: decode(b['local_transform']) for b, _ in mapped}
        initial = {str(k.name): encode(hierarchy.get_local_transform(k, True)) for _, k in mapped}
        for name, value in item['controls'].items():
            set_value(rig, name, value)
        samples = []
        hair_keys = [k for _, k in mapped if str(k.name).startswith('CSS_Hair_')]
        for frame in range(241):
            hierarchy.reset_pose_to_initial(unreal.RigElementType.BONE)
            for bone, bone_key in mapped:
                hierarchy.set_local_transform(bone_key, reference[bone['name']], False, True)
            head_key = key('head')
            if animation:
                pose = animation.get_anim_pose_at_time((frame / 60) % animation.sequence_length, options)
                if not pose.is_valid():
                    raise ValueError('Invalid evaluated animation pose')
                pose_names = {str(n) for n in pose.get_bone_names()}
                if not {b['name'] for b, _ in mapped}.issubset(pose_names):
                    raise ValueError('Animation evaluation omitted mapped mesh bones')
                for bone, bone_key in mapped:
                    hierarchy.set_local_transform(bone_key, pose.get_bone_pose(bone['name']), False, True)
            else:
                angle = math.radians(60) * math.sin(math.pi * frame / 60) if frame <= 120 else 0
                head = hierarchy.get_local_transform(head_key)
                q = [0, 0, 0, math.cos(angle / 2)]
                q[axis] = math.sin(angle / 2)
                head.rotation = head.rotation * unreal.Quat(*q)
                hierarchy.set_local_transform(head_key, head, False, True)
            input_pose = {str(k.name): encode(hierarchy.get_local_transform(k)) for _, k in mapped}
            set_value(rig, 'UpdateSerial', frame)
            set_value(rig, 'ResetEpoch', 0)
            set_value(rig, 'Enabled', True)
            rig.set_delta_time(0 if frame == 0 else 1 / 60)
            if not rig.execute('Forwards Solve'):
                raise RuntimeError('Hair evaluation failed')
            output = {str(k.name): encode(hierarchy.get_local_transform(k)) for k in hair_keys}
            if not all(math.isfinite(n) for v in output.values() for n in v):
                raise ValueError('Nonfinite hair output')
            for _, k in mapped:
                if k not in hair_keys and encode(hierarchy.get_local_transform(k)) != input_pose[str(k.name)]:
                    raise ValueError('Hair solve changed a non-hair bone')
            samples.append(dict(frame=frame, seconds=frame / 60, head=input_pose['head'], hair=output,
                                total_steps=int(rig.get_variable_as_string('TotalSteps')),
                                resets=int(rig.get_variable_as_string('ResetCount'))))
            if animation:
                samples[-1]['pose'] = {b['name']: encode(pose.get_bone_pose(b['name'])) for b in bones}
        if samples[-1]['total_steps'] != 480 or samples[-1]['resets'] != 1:
            raise ValueError('Unexpected continuous stepping/reset count')
        rig_reference_preserved = all(encode(hierarchy.get_local_transform(k, True)) == initial[str(k.name)]
                                      for _, k in mapped)
        if not rig_reference_preserved:
            raise ValueError('Rig reference changed')
        target = out / (item['id'] + '-axis' + str(axis) + '.json.gz')
        with gzip.open(target, 'xt', compresslevel=1) as handle:
            json.dump(dict(axis=axis, motion=motion, controls=item['controls'], samples=samples), handle,
                      separators=(',', ':'), allow_nan=False)
        mesh_result['cases'].append(dict(axis=axis, file=target.name, sha256=sha(target), samples=len(samples)))
        unreal.log('CSS_HAIR_SURFACE ' + item['id'] + ' axis ' + str(axis))
    results.append(mesh_result)
for path, digest in protected.items():
    if sha(Path(path)) != digest:
        raise ValueError('Source changed: ' + path)
(out / 'report.json').write_text(json.dumps(dict(passed=True, results=results, sources=protected,
    motions=plan.get('motions', 'synthetic 60-degree head turns'),
    scope='Neutral body shapes, local pose transfer. Optional animation input uses compressed evaluation retargeted for the mesh. Full 60 Hz hair timeline; geometry contact must be evaluated separately. No AnimBP, cloth, facial morph, gameplay or frame-time acceptance.'), indent=2) + '\n')
