"""Render original and fitted collision geometry against unchanged B2 skin.

Run with the pinned Blender 5.2.2. These are neutral fit views, not collision tests.
"""
import hashlib
import json
import math
import os
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT / 'CustomShellSystem/work/grip-grounding-v1'
FIT = Path(os.environ['CSS_BODY_PHYSICS_FIT_DIR']).resolve()
OUT = Path(os.environ['CSS_BODY_PHYSICS_RENDER_DIR']).resolve()
assert FIT.parent == WORK.resolve() and OUT.parent == WORK.resolve()
assert not OUT.exists()
OUT.mkdir()
sys.path.insert(0, str(ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools'))
from export_seduxtress_eve import read_bones, TO_UE

source_path = WORK / 'arm-rest-correctives-export-v1/candidate.mesh.json'
bind_path = WORK / 'arm-rest-b2-full-import-v1/engine-b2-bind.json'
candidate_path = FIT / 'candidate.json'
template_path = Path(os.environ.get('CSS_BODY_PHYSICS_BASELINE', str(WORK / 'b2-body-physics-reference-v1/template.json'))).resolve()
assert template_path.is_relative_to(WORK.resolve())
inputs = [source_path, bind_path, candidate_path, template_path]
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
hashes = {str(p): digest(p) for p in inputs}
source = json.loads(source_path.read_text())
bones, world = read_bones(bind_path)
by_name = {b['name'].lower(): i for i, b in enumerate(bones)}
to_blender = TO_UE.inverted()
slots = set(range(8)) | {23, 24, 25, 26}
faces = [[source['wedges'][w][0] for w in f[:3]] for f in source['faces'] if f[3] in slots]
ids = sorted({i for face in faces for i in face})
remap = {original: index for index, original in enumerate(ids)}
morph_names = ('FBMBodyTone', 'PBMBreastsSize', 'PBMGlutesSize', 'PBMHipSize', 'PBMThighsTone', 'PBMWaistWidth')
morph_values = json.loads(os.environ.get('CSS_BODY_PHYSICS_RENDER_MORPHS', '[0,0,0,0,0,0]'))
assert len(morph_values) == 6 and all(0 <= value <= 1 for value in morph_values)
coordinates = {i: Vector(source['points'][i]) for i in ids}
for shape in source['morph_targets']:
    if shape['name'] not in morph_names:
        continue
    weight = morph_values[morph_names.index(shape['name'])]
    for vertex, *delta in shape['deltas']:
        if vertex in coordinates:
            coordinates[vertex] += Vector(delta)*weight
vertices = [to_blender @ coordinates[i] for i in ids]
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
mesh = bpy.data.meshes.new('Unmodified B2 exported skin')
mesh.from_pydata(vertices, [], [[remap[i] for i in f] for f in faces])
mesh.update()
body = bpy.data.objects.new(mesh.name, mesh)
bpy.context.scene.collection.objects.link(body)
body.color = (.35, .38, .41, 1)
for polygon in mesh.polygons:
    polygon.use_smooth = True


def rotator(value):
    p, y, r = [math.radians(value.get(k, 0)) / 2 for k in ('Pitch', 'Yaw', 'Roll')]
    sp, sy, sr = math.sin(p), math.sin(y), math.sin(r)
    cp, cy, cr = math.cos(p), math.cos(y), math.cos(r)
    return Quaternion((cr*cp*cy+sr*sp*sy, cr*sp*sy-sr*cp*cy,
                       -cr*sp*cy-sr*cp*sy, cr*cp*sy-sr*sp*cy)).normalized()


def wire(collection, name, paths, transform, color):
    curve = bpy.data.curves.new(name, 'CURVE')
    curve.dimensions = '3D'
    curve.bevel_depth = .0012
    curve.bevel_resolution = 1
    for points, closed in paths:
        spline = curve.splines.new('POLY')
        spline.points.add(len(points) - 1)
        for point, coords in zip(spline.points, points):
            value = transform @ Vector(coords)
            point.co = (*value, 1)
        spline.use_cyclic_u = closed
    obj = bpy.data.objects.new(name, curve)
    collection.objects.link(obj)
    obj.color = color


def shape_paths(kind, shape):
    if kind == 'BoxElems':
        corners = [(x*shape['X']/2, y*shape['Y']/2, z*shape['Z']/2)
                   for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)]
        return [([corners[i], corners[j]], False) for i in range(8) for j in range(i+1, 8)
                if bin(i ^ j).count('1') == 1]
    radius = shape.get('Radius', shape.get('Radius0'))
    assert radius == shape.get('Radius1', radius)
    half = shape['Length']/2
    # Capsule rings include hemispherical caps; Length excludes those caps.
    rings = []
    for angle in (-math.pi/2, -math.pi/3, -math.pi/6, 0):
        rings.append((radius*math.cos(angle), -half+radius*math.sin(angle)))
    for angle in (0, math.pi/6, math.pi/3, math.pi/2):
        rings.append((radius*math.cos(angle), half+radius*math.sin(angle)))
    paths = [([(r*math.cos(t*math.tau/48), r*math.sin(t*math.tau/48), z)
               for t in range(48)], True) for r, z in rings if r > .001]
    for t in range(8):
        angle = t*math.tau/8
        paths.append(([(r*math.cos(angle), r*math.sin(angle), z) for r, z in rings], False))
    return paths


collections = {}
for label, path, color in [('original', template_path, (1, .3, .08, 1)),
                            ('fitted', candidate_path, (.15, 1, .5, 1))]:
    collection = bpy.data.collections.new(label)
    bpy.context.scene.collection.children.link(collection)
    collections[label] = collection
    data = json.loads(path.read_text())
    for entry in data['bodies']:
        name = entry['BoneName'].lower()
        for kind, shapes in entry['AggGeom'].items():
            for index, shape in enumerate(shapes):
                center = Vector([shape['Center'][k] for k in 'XYZ'])
                transform = to_blender @ world[by_name[name]] @ Matrix.Translation(center) @ rotator(shape['Rotation']).to_matrix().to_4x4()
                wire(collection, f'{label}_{name}_{kind}_{index}', shape_paths(kind, shape), transform, color)

scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'OBJECT'
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.background_type = 'WORLD'
scene.world = bpy.data.worlds.new('Neutral fit background')
scene.world.color = (.04, .04, .04)
scene.render.resolution_x = 1000
scene.render.resolution_y = 1100
scene.render.resolution_percentage = 100
camera = bpy.data.cameras.new('Camera')
camera.type = 'ORTHO'
camera.ortho_scale = 2.3
view = bpy.data.objects.new('Camera', camera)
scene.collection.objects.link(view)
scene.camera = view
minimum = Vector([min(v[a] for v in vertices) for a in range(3)])
maximum = Vector([max(v[a] for v in vertices) for a in range(3)])
target = (minimum+maximum)/2
views = {'front': Vector((0, -5, 0)), 'side': Vector((5, 0, 0)),
         'back': Vector((0, 5, 0))}
for label in collections:
    for name, collection in collections.items():
        collection.hide_render = name != label
    for name, offset in views.items():
        view.location = target+offset
        view.rotation_euler = (target-view.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(OUT / f'{label}-{name}.png')
        bpy.ops.render.render(write_still=True)
assert all(digest(Path(p)) == h for p, h in hashes.items())
(OUT / 'report.json').write_text(json.dumps(dict(
    protected_hashes=hashes, skin_vertices=len(ids), skin_triangles=len(faces),
    bounds_metres=[list(minimum), list(maximum)], views=list(views), morph_values=morph_values,
    scope=__doc__, visual_review_pending=True), indent=2)+'\n')
