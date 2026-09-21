"""Replay measured body motion as skinned diagnostic frames and collider overlays.

Animation uses actual component bone poses and active morph weights. Fall skin
is reconstructed from measured rigid-body transforms with other joints at rest;
it is not the game's post-ragdoll animation blend or a gameplay recording.
"""
import hashlib
import json
import math
import os
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT/'CustomShellSystem/work/grip-grounding-v1'
SOURCE = Path(os.environ['CSS_BODY_MOTION_DIR']).resolve()
OUT = Path(os.environ['CSS_BODY_MOTION_RENDER_DIR']).resolve()
assert SOURCE.parent == OUT.parent == WORK.resolve() and not OUT.exists()
assert json.loads((SOURCE/'report.json').read_text())['passed']
OUT.mkdir()
render_images = os.environ.get('CSS_BODY_MOTION_RENDER_IMAGES', '1') == '1'
sys.path[:0] = [str(Path(__file__).parent), str(ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools')]
from export_seduxtress_eve import read_bones, TO_UE
from body_physics_geometry import SKIN_SLOTS, rotator, sdf, skin_regions

mesh_path = WORK/'arm-rest-correctives-export-v1/candidate.mesh.json'
bind_path = WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json'
asset_path = Path(os.environ.get('CSS_BODY_MOTION_DEFINITION', str(WORK/'b2-body-refined-fit-v1/candidate.json'))).resolve()
assert asset_path.is_relative_to(WORK.resolve())
inputs = [mesh_path, bind_path, asset_path, SOURCE/'animation-raw.json', SOURCE/'fall-raw.json']
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
hashes = {str(p): digest(p) for p in inputs}
source = json.loads(mesh_path.read_text())
asset = json.loads(asset_path.read_text())
bones, reference = read_bones(bind_path)
indices = {b['name'].lower(): i for i, b in enumerate(bones)}
bind_inverse = [m.inverted() for m in reference]
local_reference = [reference[b['parent']].inverted()@reference[i] if b['parent'] >= 0 else reference[i]
                   for i, b in enumerate(bones)]
faces = [[source['wedges'][w][0] for w in f[:3]] for f in source['faces'] if f[3] in SKIN_SLOTS]
ids = sorted({i for f in faces for i in f})
lookup = {vertex: i for i, vertex in enumerate(ids)}
region_ids, regions = skin_regions(source, bones, asset['bodies'])
assert region_ids == ids
region_indices = {name: np.asarray([lookup[v] for v in vertices], dtype=int)
                  for name, vertices in regions.items() if vertices}
points = np.asarray(source['points'], dtype=float)[ids]
weighted = {}
for vertex, bone, weight in source['influences']:
    if vertex in lookup:
        weighted.setdefault(bone, []).append((lookup[vertex], weight))
weighted = {bone: (np.asarray([v for v, _ in rows]), np.asarray([w for _, w in rows]))
            for bone, rows in weighted.items()}
total_weights = np.zeros(len(ids))
for vertices, weights in weighted.values():
    total_weights[vertices] += weights
assert np.max(np.abs(total_weights-1)) < .0001
deltas = {}
for shape in source['morph_targets']:
    rows = [(lookup[v], delta) for v, *delta in shape['deltas'] if v in lookup]
    if rows:
        deltas[shape['name']] = (np.asarray([v for v, _ in rows]), np.asarray([delta for _, delta in rows]))


def transform(row):
    x, y, z, w = row['rotation']
    return Matrix.LocRotScale(Vector(row['translation']), Quaternion((w, x, y, z)), Vector(row['scale']))


def skin(pose, morphs):
    source_points = points.copy()
    for name, value in morphs.items():
        if value and name in deltas:
            vertices, change = deltas[name]
            source_points[vertices] += change*value
    result = np.zeros_like(points)
    for bone, (vertices, weights) in weighted.items():
        matrix = np.asarray(pose[bone]@bind_inverse[bone], dtype=float)
        result[vertices] += (source_points[vertices]@matrix[:3, :3].T+matrix[:3, 3])*weights[:, None]
    return result


def paths(kind, shape):
    if kind == 'BoxElems':
        vertices = [(x*shape['X']/2, y*shape['Y']/2, z*shape['Z']/2)
                    for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)]
        return [([vertices[i], vertices[j]], False) for i in range(8) for j in range(i+1, 8)
                if bin(i ^ j).count('1') == 1]
    radius = shape.get('Radius', shape.get('Radius0'))
    assert radius == shape.get('Radius1', radius)
    half = shape['Length']/2
    rings = [(radius*math.cos(a), sign*half+radius*math.sin(a))
             for sign, angles in [(-1, (-math.pi/2, -math.pi/3, -math.pi/6, 0)),
                                  (1, (0, math.pi/6, math.pi/3, math.pi/2))] for a in angles]
    result = [([(r*math.cos(t*math.tau/32), r*math.sin(t*math.tau/32), z) for t in range(32)], True)
              for r, z in rings if r > .001]
    result += [([(r*math.cos(t*math.tau/8), r*math.sin(t*math.tau/8), z) for r, z in rings], False)
               for t in range(8)]
    return result


bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
mesh = bpy.data.meshes.new('Measured skin')
mesh.from_pydata(points/100, [], [[lookup[i] for i in face] for face in faces])
mesh.update()
body = bpy.data.objects.new(mesh.name, mesh)
bpy.context.scene.collection.objects.link(body)
body.color = (.4, .43, .47, 1)
for face in mesh.polygons:
    face.use_smooth = True
colliders = []
for entry in asset['bodies']:
    for kind, shapes in entry['AggGeom'].items():
        for shape in shapes:
            curve = bpy.data.curves.new(entry['BoneName'], 'CURVE')
            curve.dimensions = '3D'
            curve.bevel_depth = .12
            curve.bevel_resolution = 1
            for values, closed in paths(kind, shape):
                spline = curve.splines.new('POLY')
                spline.points.add(len(values)-1)
                for point, value in zip(spline.points, values):
                    point.co = (*value, 1)
                spline.use_cyclic_u = closed
            obj = bpy.data.objects.new(entry['BoneName']+' collision', curve)
            bpy.context.scene.collection.objects.link(obj)
            obj.color = (.1, .9, .45, 1)
            local = Matrix.Translation(Vector([shape['Center'][k] for k in 'XYZ']))@rotator(shape['Rotation']).to_matrix().to_4x4()
            colliders.append((obj, entry['BoneName'].lower(), local))
bpy.ops.mesh.primitive_plane_add(size=12, location=(0, 0, 0))
bpy.context.object.color = (.15, .15, .15, 1)
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'OBJECT'
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.background_type = 'WORLD'
scene.world = bpy.data.worlds.new('Motion background')
scene.world.color = (.035, .035, .035)
scene.render.resolution_x, scene.render.resolution_y = 900, 1000
scene.render.resolution_percentage = 100
camera = bpy.data.cameras.new('Motion camera')
camera.type = 'ORTHO'
camera.ortho_scale = 3.6
view = bpy.data.objects.new('Motion camera', camera)
scene.collection.objects.link(view)
scene.camera = view
target = Vector((0, 0, 1.2))
view.location = target+Vector((3, -6, 2))
view.rotation_euler = (target-view.location).to_track_quat('-Z', 'Y').to_euler()
reports = {}
hand_clouds = {'hand_l': [], 'hand_r': []}
for mode in ('animation', 'fall'):
    data = json.loads((SOURCE/(mode+'-raw.json')).read_text())
    dt = data['samples'][1]['time']
    step = max(1, round(1/dt/15))
    samples = [s for s in data['samples'] if 'raw_pose' in s] if mode == 'animation' else data['samples'][::step]
    # Frame the entire measured trajectory, including the settling position.
    bounds = np.asarray([b['translation'] for s in samples for b in s['bodies']])/100
    bounds[:, 1] *= -1
    target = Vector((bounds.min(axis=0)+bounds.max(axis=0))/2)
    view.location = target+Vector((3, -6, 2))
    view.rotation_euler = (target-view.location).to_track_quat('-Z', 'Y').to_euler()
    camera.ortho_scale = max(3.6, float(np.linalg.norm(bounds.max(axis=0)-bounds.min(axis=0)))+1)
    folder = OUT/mode
    folder.mkdir()
    rows = []
    for index, sample in enumerate(samples):
        physical = {b['bone'].lower(): transform(b) for b in sample['bodies']}
        if mode == 'animation':
            assert len(sample['raw_pose']) == len(bones)
            assert all(a['bone'].lower() == b['name'].lower() for a, b in zip(sample['raw_pose'], bones))
            pose = [transform(row) for row in sample['raw_pose']]
        else:
            pose = []
            for i, bone in enumerate(bones):
                parent = pose[bone['parent']] if bone['parent'] >= 0 else Matrix.Translation((0, 0, 100))
                pose.append(physical.get(bone['name'].lower(), parent@local_reference[i]))
        cloud = skin(pose, sample.get('morph_weights', {}))
        blender_points = cloud/100
        blender_points[:, 1] *= -1
        mesh.vertices.foreach_set('co', blender_points.astype(np.float32).ravel())
        mesh.update()
        for obj, name, local in colliders:
            obj.matrix_world = TO_UE.inverted()@physical[name]@local
        row = dict(frame=sample['frame'], time=sample['time'])
        if mode == 'animation':
            distance = np.full(len(ids), np.inf)
            for entry in asset['bodies']:
                inverse = np.asarray(physical[entry['BoneName'].lower()].inverted(), dtype=float)
                local_cloud = cloud@inverse[:3, :3].T+inverse[:3, 3]
                name = entry['BoneName'].lower()
                if name in hand_clouds:
                    hand_clouds[name].extend(local_cloud[region_indices[name]].tolist())
                for kind, shapes in entry['AggGeom'].items():
                    for shape in shapes:
                        distance = np.minimum(distance, sdf(shape, kind, local_cloud))
            row.update(outside_vertices=int(np.sum(distance > .001)), maximum_outside_cm=max(0., float(distance.max())))
            row['regions'] = {name: dict(outside_vertices=int(np.sum(distance[vertices] > .001)),
                maximum_outside_cm=max(0., float(distance[vertices].max())))
                for name, vertices in region_indices.items()}
            row['worst_vertices'] = [dict(vertex=ids[v], position_cm=cloud[v].tolist(),
                distance_cm=float(distance[v]), influences=[dict(bone=bones[b]['name'], weight=float(w))
                    for b, (vertices, weights) in weighted.items()
                    for vi, w in zip(vertices, weights) if vi == v])
                for v in np.argsort(distance)[-10:][::-1]]
        rows.append(row)
        scene.render.filepath = str(folder/f'frame-{index:03}.png')
        if render_images:
            bpy.ops.render.render(write_still=True)
    reports[mode] = dict(frames=len(samples), frame_rate=1/(samples[1]['time']-samples[0]['time']), samples=rows)
assert all(digest(Path(p)) == h for p, h in hashes.items())
(OUT/'hand-local-clouds.json').write_text(json.dumps(dict(scope='Measured animation skin in each hand body local frame',
    protected_hashes=hashes, points=hand_clouds))+'\n')
(OUT/'report.json').write_text(json.dumps(dict(scope=__doc__, protected_hashes=hashes, modes=reports,
    images_rendered=render_images, visual_review_pending=True), indent=2)+'\n')
