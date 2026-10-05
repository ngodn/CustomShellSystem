"""Run a small Blender fixture through the source-scene sampler."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--blender', type=Path, required=True)
args = parser.parse_args()
sampler = Path(__file__).with_name('sample_blender_scene.py').resolve()
fixture = '''
import bpy, sys
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.scale_length = .01
scene.render.fps = 30
scene.render.fps_base = 1
bpy.ops.object.armature_add()
rig = bpy.context.object
rig.name = 'FixtureRig'
rig.location = (10, 20, 30)
bpy.ops.object.mode_set(mode='EDIT')
root = rig.data.edit_bones[0]
root.name = 'Root'
root.head = (0, 0, 0)
root.tail = (0, 0, 1)
child = rig.data.edit_bones.new('Child')
child.head = (0, 0, 1)
child.tail = (0, 0, 2)
child.parent = root
bpy.ops.object.mode_set(mode='OBJECT')
target = bpy.data.objects.new('Target', None)
scene.collection.objects.link(target)
for frame, location in [(1, (0, 0, 0)), (5, (4, 8, 0))]:
    target.location = location
    target.keyframe_insert(data_path='location', frame=frame)
action = target.animation_data.action
for layer in action.layers:
    for strip in layer.strips:
        for bag in strip.channelbags:
            for curve in bag.fcurves:
                for key in curve.keyframe_points:
                    key.interpolation = 'LINEAR'
track = target.animation_data.nla_tracks.new()
strip = track.strips.new('Retimed target', 1, action)
strip.scale = .5
assert strip.frame_end == 3
target.animation_data.action = None
constraint = rig.pose.bones['Root'].constraints.new('COPY_LOCATION')
constraint.target = target
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=sys.argv[-1])
'''
with tempfile.TemporaryDirectory(prefix='css-scene-sampler-') as temporary:
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
    with (work/'sample.log').open('w') as log:
        subprocess.run(common + ['--python', str(sampler), '--', str(source), str(work/'sample'),
                                '--rig', 'FixtureRig', '--start', '1', '--end', '3',
                                '--sample-rate', '60'], stdout=log, stderr=subprocess.STDOUT, check=True)
    assert hashlib.sha256(source.read_bytes()).hexdigest() == before
    data = json.loads((work/'sample/source-tracks.json').read_text())
    assert data['frameCount'] == 5 and abs(data['duration']-2/30) < 1e-10
    root, child = data['bones']
    assert root['name'] == 'Root' and child['parent'] == 0
    assert root['reference']['Translation'] == dict(X=10, Y=-20, Z=30)
    for frame, position in enumerate(root['translations']):
        expected = dict(X=frame, Y=-2*frame, Z=0)
        assert max(abs(position[k]-expected[k]) for k in 'XYZ') < 1e-5, (frame, position)
    for position in child['translations']:
        assert max(abs(position[k]-dict(X=0, Y=-1, Z=0)[k]) for k in 'XYZ') < 1e-5, position
print('PASS: evaluated constraint, NLA retiming, half-frame sampling, parent locals, Y reflection, immutable source')
