"""UE 5.6.1: verify and repair unmapped attachment tracks without changing the rig."""
import copy
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
MODE = os.environ.get('CSS_ATTACHMENT_MODE', 'create')
assert MODE in ('create', 'readback')
assert not (WORK/(MODE+'-result.json')).exists()
old = ROOT/'CustomShellSystem/work/anim9/batch'
forward = ROOT/'CustomShellSystem/work/anim4'
bind = json.loads((old/'target-bind.json').read_text())
indices = {b['name']: i for i, b in enumerate(bind)}


def ancestors(index):
    result = set()
    parent = bind[index]['parent']
    while parent >= 0:
        assert parent < index
        result.add(parent)
        parent = bind[parent]['parent']
    return result


# Independent parent-link traversal, not Unreal's contiguous branch scan.
parents = [ancestors(i) for i in range(len(bind))]
mapped = {indices['root'], indices['pelvis']}
for _, _, _, first, last in json.loads((old/'chains.json').read_text()):
    current = indices[last]
    while True:
        mapped.add(current)
        if current == indices[first]:
            break
        current = bind[current]['parent']
        assert current >= 0
attachments = {i for i in range(1, len(bind)) if i not in mapped
               and parents[i] & (mapped - {0}) and not any(i in parents[j] for j in mapped)}
assert all(indices[b['name']] in attachments for b in bind if b['name'].startswith('CSS_Hair_'))
jobs = [(label, 'D1', old) for label, _ in json.loads((old/'batch.json').read_text())]
jobs += [(label, 'V2', forward) for label in ('Walk', 'Jog', 'Sprint', 'Idle')]
content = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
protected = json.loads((old/'protected.json').read_text())
for package in ['CSS/SeduXtress/ABP_Secondary'] + [
        'CSS/AnimLab/RT_'+revision+'_'+label for label, revision, _ in jobs]:
    path = content/(package+'.uasset')
    protected[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
if MODE == 'create':
    (WORK/'protected.json').write_text(json.dumps(protected, indent=2)+'\n')
    (WORK/'target-bind.json').write_text(json.dumps(bind, indent=2)+'\n')
    (WORK/'batch.json').write_text(json.dumps([(label, '') for label, _, _ in jobs])+'\n')
    (WORK/'batch-config.json').write_text(json.dumps(dict(revision='D2', component_loops=1))+'\n')
else:
    assert protected == json.loads((WORK/'protected.json').read_text())

mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
options = unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
options.set_editor_property('optional_skeletal_mesh', mesh)


def verify(animation, document, repair):
    output = copy.deepcopy(document)
    changed = set()
    maximum_position = maximum_rotation = 0.
    untouched_position = untouched_rotation = 0.
    for frame, entry in enumerate(document['frames']):
        expected = entry['pose']['Snapshot']
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(animation, frame/document['fps'], options)
        assert unreal.AnimPoseExtensions.is_valid(pose)
        assert [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)] == expected['BoneNames']
        for index, (name, previous) in enumerate(zip(expected['BoneNames'], expected['LocalTransforms'], strict=True)):
            wanted = previous
            if repair and index in attachments:
                b = bind[index]
                assert name == b['name']
                wanted = dict(Translation=dict(zip('XYZ', b['translation'])),
                              Rotation=dict(zip('XYZW', b['rotation'])), Scale3D=dict(zip('XYZ', b['scale'])))
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
            measured = dict(Translation=dict(zip('XYZ', (t.translation.x, t.translation.y, t.translation.z))),
                            Rotation=dict(zip('XYZW', (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w))),
                            Scale3D=dict(zip('XYZ', (t.scale3d.x, t.scale3d.y, t.scale3d.z))))
            position = math.dist(list(measured['Translation'].values()), list(wanted['Translation'].values()))
            q, r = [list(v['Rotation'].values()) for v in (measured, wanted)]
            rotation = min(math.dist(q, r), math.dist(q, [-x for x in r]))
            assert position < .001 and rotation < .0001, (animation.get_name(), frame, name, position, rotation)
            assert math.dist(list(measured['Scale3D'].values()), list(wanted['Scale3D'].values())) < .0001
            maximum_position = max(maximum_position, position)
            maximum_rotation = max(maximum_rotation, rotation)
            if index not in attachments or not repair:
                untouched_position = max(untouched_position, position)
                untouched_rotation = max(untouched_rotation, rotation)
            previous_q = list(previous['Rotation'].values())
            if min(math.dist(q, previous_q), math.dist(q, [-x for x in previous_q])) > .0001:
                changed.add(name)
            output['frames'][frame]['pose']['Snapshot']['LocalTransforms'][index] = measured
    assert untouched_position == untouched_rotation == 0
    return output, dict(frames=len(document['frames']), max_position_cm=maximum_position,
                        max_quaternion_distance=maximum_rotation, changed_bones=sorted(changed),
                        untouched_position_cm=untouched_position, untouched_quaternion_distance=untouched_rotation)


results = []
for label, revision, directory in jobs:
    source_mesh = unreal.load_asset('/Game/CSS/AnimLab/SK_'+revision+'_EveSource')
    source = unreal.load_asset('/Game/CSS/AnimLab/AN_'+revision+'_'+label)
    retarget = unreal.load_asset('/Game/CSS/AnimLab/RT_'+revision+'_EveCSS')
    original = json.loads((directory/(label.lower()+'-motion.json')).read_text())
    package = '/Game/CSS/AnimLab/RT_D2_'+label
    if MODE == 'create':
        if label == 'SF':
            control = unreal.CSSAnimationLibrary.retarget_clip(source_mesh, mesh, source, retarget,
                '/Game/CSS/AnimLab/RT_D2_Control', False)
            assert control
            _, report = verify(control, original, False)
            assert not report['changed_bones']
            (WORK/'control-result.json').write_text(json.dumps(report, indent=2)+'\n')
        animation = unreal.CSSAnimationLibrary.retarget_clip(source_mesh, mesh, source, retarget, package, True)
        assert animation
        measured, report = verify(animation, original, True)
        assert unreal.EditorAssetLibrary.save_loaded_asset(animation, False)
        (WORK/(label.lower()+'-motion.json')).write_text(json.dumps(measured)+'\n')
    else:
        animation = unreal.load_asset(package)
        assert animation
        _, report = verify(animation, original, True)
        repeated = unreal.CSSAnimationLibrary.retarget_clip(source_mesh, mesh, source, retarget,
            '/Game/CSS/AnimLab/RT_D2_Check'+label, True)
        assert repeated
        _, repeat_report = verify(repeated, original, True)
        assert report == repeat_report
    results.append(dict(clip=label, **report))
    (WORK/(MODE+'-progress.json')).write_text(json.dumps(results, indent=2)+'\n')
    unreal.log('Attachment verification completed: '+label)
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
(WORK/(MODE+'-result.json')).write_text(json.dumps(dict(passed=True, clips=results,
    attachment_bones=[bind[i]['name'] for i in sorted(attachments)],
    scope='Offline attachment locals and unchanged other tracks; no physics, skinned or game acceptance.'), indent=2)+'\n')
