"""Author isolated Black Pearl heel supports. Blender 5.2.2 / Python 3.13.

The original blend and all existing objects remain unchanged. This creates a
candidate blend for visual/export review, not a deployment or floor-offset fix.
"""
import hashlib
import json
import math
import os
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform

ROOT = Path(__file__).resolve().parents[4]
MOD = ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
OUT = Path(os.environ['CSS_FOOTWEAR_OUTPUT']).resolve()
assert OUT.parent == ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT.mkdir(exist_ok=False)
SOURCE = MOD/'work/CSS_SeduXtress_ArmRestV44B2.blend'
TARGET = Path(os.environ.get('CSS_FOOTWEAR_BLEND', str(MOD/'work/CSS_SeduXtress_HeelSupportsV45C.blend'))).resolve()
assert TARGET.parent == MOD/'work' and TARGET.suffix == '.blend'
assert not TARGET.exists()
source_sha = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
sys.path.insert(0, str(MOD/'tools'))
from export_seduxtress_eve import fitted_mesh

bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
shoe = bpy.data.objects['Eve Black Pearl - Footwear']
rig = bpy.data.objects['SKEL_CSS_Base']
# Inspect an independent copy so fit baking cannot remove source modifiers or
# clear source shape drivers. The copy is deleted before saving the candidate.
copy = shoe.copy()
copy.data = shoe.data.copy()
bpy.context.scene.collection.objects.link(copy)
mesh, deltas, _ = fitted_mesh(copy)
mesh.calc_loop_triangles()
points = [copy.matrix_world@v.co for v in mesh.vertices]
triangles = [tuple(t.vertices) for t in mesh.loop_triangles]
bvh = BVHTree.FromPolygons(points, triangles, all_triangles=True)
uv = mesh.uv_layers.active
assert uv and len(shoe.data.materials) == 1

def hit(x, y):
    p, normal, tri, distance = bvh.ray_cast(Vector((x, y, -.1)), Vector((0, 0, 1)), .5)
    if p is None:
        raise ValueError('Heel collar extends outside the existing sole')
    return p, tri

vertices, faces, weights, uv_centers = [], [], [], []
morph_deltas = {name: [] for name, delta in deltas.items() if np.any(delta)}
report = dict(source=str(SOURCE), source_sha256=source_sha, candidate=str(TARGET), sides={})
segments = 12
for side in ('l', 'r'):
    ankle = rig.matrix_world@rig.data.bones['foot_'+side].head_local
    ball = rig.matrix_world@rig.data.bones['ball_'+side].head_local
    forward = ball-ankle
    forward.z = 0
    forward.normalize()
    right = forward.cross(Vector((0, 0, 1)))
    ids = [i for i, p in enumerate(points) if p.x*ankle.x > 0]
    along = np.array([(points[i]-ankle).dot(forward) for i in ids])
    # The rear seat is inside the heel counter, away from its outside edge.
    seat_u = float(along.min()+.15*(along.max()-along.min()))
    section = [i for i, u in zip(ids, along) if abs(u-seat_u) < .008]
    across = [(points[i]-ankle).dot(right) for i in section]
    seat_v = (min(across)+max(across))/2
    center = ankle+forward*seat_u+right*seat_v
    seat, triangle = hit(center.x, center.y)
    bottom = min(points[i].z for i in ids)
    assert .05 < seat.z-bottom < .15
    # Sample the existing heel-seat atlas island. No new material or texture
    # dependency is introduced. A textured render must still approve the match.
    tri = mesh.loop_triangles[triangle]
    uv_points = [Vector((*uv.data[i].uv, 0)) for i in tri.loops]
    sample = barycentric_transform(seat, *(points[i] for i in tri.vertices), *uv_points)
    seat_deltas = {name: barycentric_transform(
        seat, *(points[i] for i in tri.vertices),
        *(copy.matrix_world.to_3x3()@Vector(deltas[name][i]) for i in tri.vertices))
        for name in morph_deltas}
    nearest = min(tri.vertices, key=lambda i: (points[i]-seat).length_squared)
    assignment = {shoe.vertex_groups[g.group].name: g.weight for g in shoe.data.vertices[nearest].groups if g.weight > .0001}
    assert assignment and all(n in ('calf_'+side, 'foot_'+side) for n in assignment)
    total = sum(assignment.values())
    assignment = {n: w/total for n, w in assignment.items()}
    # Taper with a broad seated collar and a small flat ground-contact cap.
    rings = [(0, .0065, .0065), (.025, .0065, .0065), (.12, .0068, .007),
             (.45, .008, .010), (.77, .011, .015), (.93, .017, .023), (1, .022, .028)]
    base_index = len(vertices)
    for height, width, length in rings:
        axis = center+forward*(.008*(1-height))
        z = bottom+(seat.z-bottom)*height
        for i in range(segments):
            angle = math.tau*i/segments
            point = axis+right*(math.cos(angle)*width/2)+forward*(math.sin(angle)*length/2)
            point.z = z
            if height == 1:
                surface, _ = hit(point.x, point.y)
                point.z = surface.z+.002
            vertices.append(tuple(point))
            weights.append(assignment)
            uv_centers.append((float(sample.x), float(sample.y)))
            for name, delta in seat_deltas.items():
                morph_deltas[name].append(delta.copy())
    for ring in range(len(rings)-1):
        for i in range(segments):
            j = (i+1)%segments
            faces.append((base_index+ring*segments+i, base_index+ring*segments+j,
                          base_index+(ring+1)*segments+j, base_index+(ring+1)*segments+i))
    faces.append(tuple(base_index+i for i in reversed(range(segments))))
    faces.append(tuple(base_index+(len(rings)-1)*segments+i for i in range(segments)))
    report['sides'][side] = dict(seat_m=list(seat), sole_plane_m=bottom,
                                height_cm=(seat.z-bottom)*100, weights=assignment,
                                atlas_sample=list(sample)[:2], vertices=len(rings)*segments)
    report['sides'][side]['morph_translation_m'] = {n: list(d) for n, d in seat_deltas.items()}

bpy.data.objects.remove(copy, do_unlink=True)
bpy.data.meshes.remove(mesh)
data = bpy.data.meshes.new('Black Pearl heel supports')
data.from_pydata(vertices, [], faces)
assert not data.validate(), 'Generated topology required repair'
data.materials.append(shoe.data.materials[0])
data.update()
layer = data.uv_layers.new(name='UVMap')
for polygon in data.polygons:
    polygon.use_smooth = len(polygon.vertices) == 4
    for loop in polygon.loop_indices:
        layer.data[loop].uv = uv_centers[data.loops[loop].vertex_index]
obj = bpy.data.objects.new('Eve Black Pearl - Heel Supports', data)
bpy.context.scene.collection.objects.link(obj)
obj['CSS_export'] = True
obj['CSS_part'] = 'shoes'
obj.shape_key_add(name='Basis', from_mix=False)
for name, changes in morph_deltas.items():
    key = obj.shape_key_add(name=name, from_mix=False)
    for point, delta in zip(key.data, changes, strict=True):
        point.co += delta
    # Geometry was fitted at the saved shoe baseline already. Public morphs
    # must start at zero so export cannot bake this displacement a second time.
    key.value = 0.0
for name in sorted({n for row in weights for n in row}):
    group = obj.vertex_groups.new(name=name)
    for i, row in enumerate(weights):
        if name in row:
            group.add([i], row[name], 'REPLACE')
armature = obj.modifiers.new('Armature', 'ARMATURE')
armature.object = rig
assert len(rig.data.bones) == 379
bpy.ops.wm.save_as_mainfile(filepath=str(TARGET), check_existing=False)
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == source_sha
report.update(candidate_sha256=hashlib.sha256(TARGET.read_bytes()).hexdigest(),
              vertices=len(vertices), polygons=len(faces), source_file_unchanged=True,
              scope='Isolated support geometry. No existing object, rig or body edit; no runtime deployment.')
(OUT/'build.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2), flush=True)
