"""Check the face audit against retimed NLA and an unassigned expression action."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--blender', type=Path, required=True)
args = parser.parse_args()
audit = Path(__file__).with_name('audit_blender_face.py').resolve()
fixture = '''
import bpy, sys
scene = bpy.context.scene
scene.render.fps = 30
scene.render.fps_base = 1
bpy.ops.mesh.primitive_plane_add()
mesh = bpy.context.object
mesh.name = 'Face'
mesh.shape_key_add(name='Basis')
key = mesh.shape_key_add(name='jaw_open')
key.slider_min = -1
key.slider_max = 2
for frame, value in [(1,-.25),(5,1.25)]:
    key.value = value
    key.keyframe_insert(data_path='value',frame=frame)
data = mesh.data.shape_keys.animation_data
action = data.action
action.name = 'Active face'
for layer in action.layers:
    for strip in layer.strips:
        for bag in strip.channelbags:
            for curve in bag.fcurves:
                for point in curve.keyframe_points:
                    point.interpolation = 'LINEAR'
track = data.nla_tracks.new()
strip = track.strips.new('Retimed face',1,action)
strip.scale = .5
assert strip.frame_end == 3
data.action = None
# Retain a conflicting library action without assigning it to the face.
unused = action.copy()
unused.name = 'Unused expression'
for layer in unused.layers:
    for strip in layer.strips:
        for bag in strip.channelbags:
            for curve in bag.fcurves:
                for point in curve.keyframe_points:
                    point.co.y = .7
unused.use_fake_user = True
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=sys.argv[-1])
'''
with tempfile.TemporaryDirectory(prefix='css-face-audit-') as temporary:
    work = Path(temporary)
    builder = work/'fixture.py'
    builder.write_text(fixture)
    source = work/'fixture.blend'
    common = [str(args.blender), '--background', '--factory-startup',
              '--disable-autoexec', '--python-exit-code', '1']
    with (work/'fixture.log').open('w') as log:
        subprocess.run(common + ['--python', str(builder), '--', str(source)],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    body = work/'body.json'
    body.write_text(json.dumps(dict(frameCount=3,duration=2/30,provenance=dict(
        source=str(source),sha256=before,scene='Scene',scene_rate=30,scene_frames=[1,3]))))
    output = work/'face.json'
    with (work/'audit.log').open('w') as log:
        subprocess.run(common + ['--python', str(audit), '--', str(body), str(output), '--mesh', 'Face'],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    result = json.loads(output.read_text())
    assert hashlib.sha256(source.read_bytes()).hexdigest() == before
    assert result['names'] == ['jaw_open'] and len(result['samples']) == 5
    assert len(result['active_curves']) == 1
    assert result['active_curves'][0]['action'] == 'Active face'
    for index, sample in enumerate(result['samples']):
        assert abs(sample['seconds']-index/60) < 1e-8
        assert abs(sample['values'][0]-(-.25+index*.375)) < 1e-6, sample
    assert not result['autoexec_enabled']
print('PASS: active NLA, retiming, half-frames, signed weights, unused action exclusion, immutable source')
