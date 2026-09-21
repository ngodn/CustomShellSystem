"""UE 5.6.1: isolated Eve source animation import and IK retarget candidate."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT/'CustomShellSystem/work/anim2'
CONTENT = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
PACKAGE = '/Game/CSS/AnimLab'
tools = unreal.AssetToolsHelpers.get_asset_tools()
checkpoint = json.loads((WORK/'resume.json').read_text()) if (WORK/'resume.json').exists() else {}
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in checkpoint.items())


def write(name, value):
    (WORK/name).write_text(json.dumps(value, indent=2)+'\n')


def create(name, kind, factory):
    if unreal.EditorAssetLibrary.does_asset_exist(PACKAGE+'/'+name):
        assert str(CONTENT/'CSS/AnimLab'/(name+'.uasset')) in checkpoint, name
        return unreal.load_asset(PACKAGE+'/'+name), False
    asset = tools.create_asset(name, PACKAGE, kind, factory)
    assert asset, name
    return asset, True


def snapshot(animation, mesh, frame):
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('should_retarget', True)
    options.set_editor_property('optional_skeletal_mesh', mesh)
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(animation, frame/30, options)
    assert unreal.AnimPoseExtensions.is_valid(pose)
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
    transforms = []
    for name in names:
        t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
        transforms.append(dict(Translation=dict(zip('XYZ', (t.translation.x, t.translation.y, t.translation.z))),
                               Rotation=dict(zip('XYZW', (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w))),
                               Scale3D=dict(zip('XYZ', (t.scale3d.x, t.scale3d.y, t.scale3d.z)))))
    assert len(names) >= 379
    return dict(pose=dict(Snapshot=dict(bIsValid=True, SkeletalMeshName='SK_BlackPearl2',
                                       BoneNames=names, LocalTransforms=transforms)),
                scope='Offline UE IK retarget candidate, before game IK or secondary motion.')


assert not (WORK/'retarget-result.json').exists()
source = unreal.load_asset(PACKAGE+'/SK_EveSource')
target = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
assert source and target
protected = [CONTENT/'CSS/SeduXtress/SK_BlackPearl2.uasset', CONTENT/'CSS/Shared/SKEL_Base.uasset']
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
if (WORK/'protected.json').exists():
    assert hashes == json.loads((WORK/'protected.json').read_text())
write('protected.json', hashes)
write('target-bind.json', json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(target)))
source_bind = json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(source))
assert len(source_bind) == 54
write('source-bind.json', source_bind)

chains = [('Spine', 'Bip001-Spine', 'Bip001-Spine2', 'spine_01', 'spine_05'),
          ('Head', 'Bip001-Neck', 'Bip001-Head', 'neck_01', 'head')]
for side, suffix in [('L', 'l'), ('R', 'r')]:
    for name, start, end, first, last in [
        ('Clavicle', 'Clavicle', 'Clavicle', 'clavicle', 'clavicle'),
        ('Arm', 'UpperArm', 'Hand', 'upperarm', 'hand'),
        ('Leg', 'Thigh', 'Foot', 'thigh', 'foot'),
        ('Toe', 'Toe0', 'Toe0', 'ball', 'ball')]:
        chains.append((name+side, f'Bip001-{side}-{start}', f'Bip001-{side}-{end}', first+'_'+suffix, last+'_'+suffix))
    for digit, finger in enumerate(('thumb', 'index', 'middle', 'ring', 'pinky')):
        chains.append((finger+side, f'Bip001-{side}-Finger{digit}', f'Bip001-{side}-Finger{digit}2',
                       finger+'_01_'+suffix, finger+'_03_'+suffix))
write('chains.json', chains)
rigs = []
for name, mesh, root, start, end in [('IK_EveSource', source, 'Bip001-Pelvis', 1, 2),
                                      ('IK_EveCSS', target, 'pelvis', 3, 4)]:
    rig, fresh = create(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    if not fresh:
        rigs.append(rig)
        continue
    controller = unreal.IKRigController.get_controller(rig)
    assert controller.set_skeletal_mesh(mesh)
    assert controller.set_retarget_root(root)
    for chain in chains:
        assert str(controller.add_retarget_chain(chain[0], chain[start], chain[end], 'None')) == chain[0]
    assert unreal.EditorAssetLibrary.save_loaded_asset(rig, False)
    rigs.append(rig)

retarget, fresh = create('RT_EveCSS', unreal.IKRetargeter, unreal.IKRetargetFactory())
controller = unreal.IKRetargeterController.get_controller(retarget)
mode = unreal.RetargetSourceOrTarget
if fresh:
    controller.set_ik_rig(mode.SOURCE, rigs[0]); controller.set_ik_rig(mode.TARGET, rigs[1])
    controller.set_preview_mesh(mode.SOURCE, source); controller.set_preview_mesh(mode.TARGET, target)
    controller.add_default_ops()
    for chain in chains:
        assert controller.set_source_chain(chain[0], chain[0]), chain[0]
    controller.auto_align_all_bones(mode.TARGET)
    assert unreal.EditorAssetLibrary.save_loaded_asset(retarget, False)
write('retarget-settings.json', {'ops': [str(controller.get_op_name(i)) for i in range(controller.get_num_retarget_ops())],
      'target_pose': {b['name']:str(controller.get_rotation_offset_for_retarget_pose_bone(b['name'], mode.TARGET))
                      for b in json.loads((WORK/'target-bind.json').read_text())}})

outputs = []
for label, filename in [('Walk', 'Proto_Walk'), ('Idle', 'P_Eve_Peaceful_Idle01')]:
    data = json.loads((WORK/(filename+'.json')).read_text())
    factory = unreal.AnimSequenceFactory()
    factory.set_editor_property('target_skeleton', source.get_editor_property('skeleton'))
    factory.set_editor_property('preview_skeletal_mesh', source)
    animation, fresh = create('AN_'+label, unreal.AnimSequence, factory)
    if fresh:
        edit = animation.get_editor_property('controller')
        edit.open_bracket('Eve source keys at original sample times', False)
        try:
            edit.set_frame_rate(unreal.FrameRate(30, 1), False)
            edit.set_number_of_frames(unreal.FrameNumber(data['frames']-1), False)
            for track in data['tracks']:
                assert edit.add_bone_curve(track['name'], False)
                keys = track['keys']
                assert edit.set_bone_track_keys(track['name'], [unreal.Vector(*k['translation']) for k in keys],
                                                [unreal.Quat(*k['rotation']) for k in keys],
                                                [unreal.Vector(*k['scale']) for k in keys], False)
        finally:
            edit.close_bracket(False)
        assert unreal.EditorAssetLibrary.save_loaded_asset(animation, False)
    candidate = unreal.CSSAnimationLibrary.retarget_clip(source, target, animation, retarget, PACKAGE+'/RT_'+label)
    assert candidate
    assert candidate.get_editor_property('skeleton') == target.get_editor_property('skeleton')
    assert candidate.get_path_name().startswith(PACKAGE+'/')
    outputs.append(candidate.get_path_name())
    for frame in sorted({0, data['frames']//4, data['frames']//2, 3*data['frames']//4, data['frames']-1}):
        write(f'{label.lower()}-{frame}.json', snapshot(candidate, target, frame))
    write(label.lower()+'-motion.json', dict(fps=30, frames=[snapshot(candidate, target, f) for f in range(data['frames'])]))
    assert unreal.EditorAssetLibrary.save_loaded_asset(candidate, False)

assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in hashes.items())
write('retarget-result.json', dict(passed=True, source_bones=54, chains=len(chains), assets=outputs,
                                 protected=hashes, scope='Isolated IK retarget candidates. Skinned review and gameplay validation remain required.'))
