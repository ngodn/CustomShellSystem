"""Blender 5.2: render cooked full-influence surfaces from captured UE poses."""
import argparse
import gzip
import hashlib
import json
import subprocess
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from reparent_source_legs import rotate, transforms

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--skin', type=Path, required=True)
parser.add_argument('--motion', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--materials', default='3,23,24,27')
parser.add_argument('--stride', type=int, default=4)
parser.add_argument('--frames', help='Comma-separated capture indices for still review instead of a video')
parser.add_argument('--follow-pelvis', action='store_true', help='Follow horizontal scene travel without changing the pose')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
root = Path(__file__).resolve().parents[4]
if not args.output.resolve().is_relative_to((root / 'CustomShellSystem/work').resolve()) or args.output.exists():
    raise ValueError('Use a fresh workspace output directory')
if args.stride < 1:
    raise ValueError('Positive stride required')
args.output.mkdir()


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


audit = json.loads((args.skin / 'skin-audit.json').read_text())
inputs = {str(args.skin / 'skin-audit.json'): sha(args.skin / 'skin-audit.json'),
          str(args.motion): sha(args.motion)}
for row in audit['files']:
    path = args.skin / row['name']
    if sha(path) != row['sha256'] or path.stat().st_size != row['bytes']:
        raise ValueError('Cooked skin audit changed')
    inputs[str(path)] = row['sha256']
dtype = np.dtype([('position', '<f4', 3), ('influences', [
    ('bone', '<u2'), ('raw', '<u2'), ('weight', '<f4')], audit['max_influences'])])
if dtype.itemsize != audit['vertex_stride']:
    raise ValueError('Unexpected cooked vertex layout')
vertices = np.fromfile(args.skin / 'skin-vertices.bin', dtype=dtype)
indices = np.fromfile(args.skin / 'skin-indices.bin', dtype='<u4')
materials = {int(x) for x in args.materials.split(',')}
sections = [s for s in audit['sections'] if s['MaterialIndex'] in materials]
if {s['MaterialIndex'] for s in sections} != materials:
    raise ValueError('Missing requested material section')
faces = np.concatenate([indices[s['FirstIndex']:s['FirstIndex'] + 3 * s['NumFaces']].reshape(-1, 3)
                        for s in sections])
ids, remap = np.unique(faces, return_inverse=True)
faces = remap.reshape(-1, 3)[:, ::-1]
subset = vertices[ids]
bind = transforms(audit['bones'])
components = []
reconstructed = np.zeros((len(ids), 3))
inf = subset['influences']
for bone in np.unique(inf['bone'][inf['weight'] > 0]):
    rows, slots = np.where((inf['bone'] == bone) & (inf['weight'] > 0))
    weights = inf['weight'][rows, slots].astype(float)
    bp, bq, bs = (x[0] for x in bind[bone])
    inverse = bq.copy(); inverse[:3] *= -1
    local = rotate(inverse, subset['position'][rows] - bp) / bs
    reconstructed[rows] += (bp + rotate(bq, local * bs)) * weights[:, None]
    components.append((int(bone), rows, local, weights))
bind_error = float(np.max(np.abs(reconstructed - subset['position'])))
if bind_error > .0001:
    raise ValueError('Bind reconstruction failed')
opener = gzip.open if args.motion.suffix == '.gz' else open
with opener(args.motion, 'rt') as stream:
    motion = json.load(stream)
selected = ([int(x) for x in args.frames.split(',')] if args.frames else
            list(range(0, len(motion['frames']), args.stride)))
if not selected or min(selected) < 0 or max(selected) >= len(motion['frames']):
    raise ValueError('Invalid review frames')
snapshots = [motion['frames'][i]['pose']['Snapshot'] for i in selected]
fps = motion['fps'] / args.stride
del motion
order = snapshots[0]['BoneNames']
if any(not s['bIsValid'] or s['BoneNames'] != order for s in snapshots):
    raise ValueError('Invalid captured pose')
tracks = []
for bone in audit['bones']:
    index = order.index(bone['name'])
    keys = []
    for s in snapshots:
        t = s['LocalTransforms'][index]
        keys.append({field: [t[key][c] for c in axes] for field, key, axes in (
            ('translation', 'Translation', 'XYZ'), ('rotation', 'Rotation', 'XYZW'), ('scale', 'Scale3D', 'XYZ'))})
    tracks.append(dict(name=bone['name'], keys=keys))
world = transforms(audit['bones'], tracks, len(selected))
del snapshots, tracks, vertices

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = 720
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'OBJECT'
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.background_type = 'WORLD'
scene.world = bpy.data.worlds.new('Contact review background')
scene.world.color = (.045, .045, .045)
mesh = bpy.data.meshes.new('Audited visible lower-body surfaces')
mesh.from_pydata((subset['position'] * (.01, -.01, .01)).tolist(), [], faces.tolist())
mesh.update()
obj = bpy.data.objects.new('Captured skin', mesh)
scene.collection.objects.link(obj)
obj.color = (.57, .63, .7, 1)
for poly in mesh.polygons:
    poly.use_smooth = True
bpy.ops.mesh.primitive_plane_add(size=20, location=(0, 0, -.0001))
bpy.context.object.color = (.13, .15, .17, 1)
bpy.ops.object.camera_add(location=(2.4, -3.4, 1.05))
camera = bpy.context.object
camera.rotation_euler = (Vector((0, 0, .63)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 1.6
scene.camera = camera
scene.render.image_settings.file_format = 'PNG'
frame_path = args.output / 'frame.png'
video = None
if not args.frames:
    video = subprocess.Popen(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-f', 'image2pipe',
        '-vcodec', 'png', '-framerate', str(fps), '-i', 'pipe:0', '-an', '-c:v', 'libx264',
        '-preset', 'fast', '-crf', '20', '-pix_fmt', 'yuv420p', str(args.output / 'contact-review.mp4')], stdin=subprocess.PIPE)
try:
    for frame, original in enumerate(selected):
        if args.follow_pelvis:
            pelvis = next(i for i, bone in enumerate(audit['bones']) if bone['name'] == 'pelvis')
            point = world[pelvis][0][frame] * (.01, -.01, .01)
            camera.location = Vector((point[0] + 2.4, point[1] - 3.4, 1.05))
        positions = np.zeros((len(ids), 3))
        for bone, rows, local, weights in components:
            p, q, scale = (x[frame] for x in world[bone])
            positions[rows] += (p + rotate(q, local * scale)) * weights[:, None]
        mesh.vertices.foreach_set('co', (positions * (.01, -.01, .01)).astype(np.float32).ravel())
        mesh.update()
        scene.render.filepath = str(frame_path)
        bpy.ops.render.render(write_still=True)
        if video:
            video.stdin.write(frame_path.read_bytes())
        if args.frames or frame in (0, len(selected) // 4, len(selected) // 2, len(selected) - 1):
            (args.output / f'pose-{original:05d}.png').write_bytes(frame_path.read_bytes())
finally:
    if video:
        video.stdin.close()
        if video.wait() != 0:
            raise RuntimeError('Video encoding failed')
if any(sha(Path(path)) != digest for path, digest in inputs.items()):
    raise ValueError('Protected render input changed')
(args.output / 'report.json').write_text(json.dumps(dict(complete=True, inputs=inputs,
    frames=selected, video_fps=fps if video else None, materials=sorted(materials), vertices=len(ids), faces=len(faces),
    blender_version=bpy.app.version_string, bind_error_cm=bind_error, follow_pelvis=args.follow_pelvis,
    scope='Neutral cooked linear skinning with every influence and rebuilt normals. Selected material surfaces only; no textures, morphs, cloth or post-process. Technical contact review, not full outfit or gameplay acceptance.'), indent=2) + '\n')
