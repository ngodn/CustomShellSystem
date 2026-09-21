"""Exercise animated kinematic bodies and a simulated fall in UE 5.6.1.

H2 poses are retained game-derived samples spread over a diagnostic two-second
sequence. The fall steps the real Chaos scene, not the complete game world.
"""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_BODY_MOTION_DIR']).resolve()
assert OUT.parent == WORK.resolve() and not OUT.exists()
OUT.mkdir()
write = lambda name, value: (OUT/name).write_text(json.dumps(value, indent=2)+'\n')
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
protected = json.loads((WORK/'b2-body-refined-query-v2/report.json').read_text())['protected_hashes']
mesh_path = '/Game/CSSAuthoring/DiagnosticReferences/SK_B2GameReferenceMetadata_V2'
asset_path = os.environ.get('CSS_BODY_MOTION_ASSET', '/Game/CSSAuthoring/DiagnosticReferences/PA_B2BodyFit_V2')
assert asset_path.startswith('/Game/CSSAuthoring/DiagnosticReferences/PA_B2BodyFit')
content = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
for path in (mesh_path, asset_path):
    p = content/(path.removeprefix('/Game/')+'.uasset')
    protected[str(p)] = digest(p)
mesh, asset = unreal.load_asset(mesh_path), unreal.load_asset(asset_path)
assert mesh and asset
snapshots = []
for index in range(5):
    p = WORK/f'b2-metadata-animation-binding-v1/candidate_game_rotations-s{index}-compressed-ik1.json'
    protected[str(p)] = digest(p)
    snapshots.append(json.loads(p.read_text())['pose']['Snapshot'])
bones = json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))
assert len(bones) == 379
factory = unreal.AnimSequenceFactory()
factory.set_editor_property('target_skeleton', mesh.get_editor_property('skeleton'))
factory.set_editor_property('preview_skeletal_mesh', mesh)
sequence = unreal.AssetToolsHelpers.get_asset_tools().create_asset('AN_B2BodyMotion_'+OUT.name.replace('-', '_'),
    '/Game/CSSAuthoring/TransientProbes', unreal.AnimSequence, factory)
assert sequence
sequence.set_retarget_source_asset(mesh)
sequence.update_retarget_source_asset_data()
controller = sequence.get_editor_property('controller')
controller.open_bracket('Retained H2 poses for body collision updates', False)
try:
    controller.set_number_of_frames(unreal.FrameNumber(30), False)
    controller.set_frame_rate(unreal.FrameRate(2, 1), False)
    controller.set_number_of_frames(unreal.FrameNumber(4), False)
    for index, bone in enumerate(bones):
        name = bone['name']
        assert all(s['BoneNames'][index].lower() == name.lower() for s in snapshots)
        values = [s['LocalTransforms'][index] for s in snapshots]
        assert controller.add_bone_curve(name, False)
        assert controller.set_bone_track_keys(name,
            [unreal.Vector(*[v['Translation'][k] for k in 'XYZ']) for v in values],
            [unreal.Quat(*[v['Rotation'][k] for k in 'XYZW']) for v in values],
            [unreal.Vector(*[v['Scale3D'][k] for k in 'XYZ']) for v in values], False)
finally:
    controller.close_bracket(False)
assert unreal.CSSRetargetLibrary.prepare_compressed_animation(sequence, mesh)
assert math.isclose(sequence.get_play_length(), 2.)
fps = int(os.environ.get('CSS_BODY_MOTION_FPS', '60'))
assert fps in (30, 60)
checks = []
for simulate, frames in ((False, fps*2), (True, fps*4)):
    label = 'fall' if simulate else 'animation'
    raw = unreal.CSSPhysicsProbeLibrary.probe_body_motion(mesh, asset, sequence, simulate, fps, frames)
    (OUT/(label+'-raw.json')).write_text(raw+'\n')
    result = json.loads(raw)
    samples = result['samples']
    assert result['world_destroyed'] and len(samples) == frames+1
    assert all(len(s['bodies']) == 22 for s in samples)
    bodies = [body for s in samples for body in s['bodies']]
    metrics = dict(frames=len(samples), body_samples=len(bodies),
        maximum_position_error_cm=max(b['position_error_cm'] for b in bodies),
        maximum_rotation_error_degrees=max(b['rotation_error_degrees'] for b in bodies),
        maximum_anchor_error_cm=max(s['maximum_anchor_error_cm'] for s in samples),
        maximum_speed_cm_s=max(math.sqrt(sum(v*v for v in b['velocity'])) for b in bodies),
        minimum_bone_height_cm=min(b['translation'][2] for b in bodies),
        maximum_bone_height_cm=max(b['translation'][2] for b in bodies))
    if simulate:
        pelvis = [next(b for b in s['bodies'] if b['bone'] == 'pelvis') for s in samples]
        metrics['pelvis_drop_cm'] = pelvis[0]['translation'][2]-pelvis[-1]['translation'][2]
        checks.extend([
            ('all bodies simulate', all(b['simulating'] for b in bodies)),
            ('fall occurs', metrics['pelvis_drop_cm'] > 30),
            ('no runaway speed', metrics['maximum_speed_cm_s'] < 2000),
            ('body pivots remain near or above floor', metrics['minimum_bone_height_cm'] > -20),
            ('joint gap remains within projection tolerance', metrics['maximum_anchor_error_cm'] <= 5)])
    else:
        checks.extend([
            ('post-process exists', result['post_process_instance']),
            ('bodies remain kinematic', not any(b['simulating'] for b in bodies)),
            ('body positions follow animation', metrics['maximum_position_error_cm'] < .05),
            ('body rotations follow animation', metrics['maximum_rotation_error_degrees'] < .05)])
    write(label+'-metrics.json', metrics)
    print('CSS_BODY_MOTION_METRICS', label, json.dumps(metrics), flush=True)
assert all(digest(Path(p)) == h for p, h in protected.items())
write('report.json', dict(passed=all(value for _, value in checks), checks=checks,
    protected_hashes=protected, physics_package=asset_path, scope=__doc__, full_gameplay_verified=False, visual_review_pending=True))
assert all(value for _, value in checks), checks
