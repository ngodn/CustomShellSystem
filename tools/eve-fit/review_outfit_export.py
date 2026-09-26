"""Review an existing outfit interchange without opening or saving its source blend."""
import argparse
import hashlib
import json
import sys
from pathlib import Path
import bpy
from mathutils import Vector

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--mesh', type=Path, required=True)
p.add_argument('--audit', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--hide-material', action='append', default=[])
p.add_argument('--body-mask', type=Path)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
a.output.mkdir(exist_ok=False)
raw = a.mesh.read_bytes()
source = json.loads(raw)
assert set(a.hide_material) <= set(source['materials'])
hidden_slots = {source['materials'].index(name) for name in a.hide_material}
hidden_faces = set(json.loads(a.body_mask.read_text())['hidden_body_faces']) if a.body_mask else set()
audit = json.loads(a.audit.read_text())
assert hashlib.sha256(raw).hexdigest() == audit['output_sha256']
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
totals = [0.] * len(source['points'])
for vertex, bone, weight in source['influences']:
    totals[vertex] += weight
point_offset = face_offset = 0
parts = []
for part in audit['parts']:
    name, count, face_count = part['name'], part['points'], part['faces']
    points = source['points'][point_offset:point_offset+count]
    faces = [[source['wedges'][w][0]-point_offset for w in f[:3]]
             for f in source['faces'][face_offset:face_offset+face_count]]
    assert all(0 <= i < count for f in faces for i in f)
    visible_faces = [face for index, (face, record) in enumerate(zip(faces, source['faces'][face_offset:face_offset+face_count]), face_offset)
                     if record[3] not in hidden_slots and index not in hidden_faces]
    mesh = bpy.data.meshes.new(name)
    # The Y reflection converts Unreal clockwise faces to outward Blender winding.
    mesh.from_pydata([(x/100, -y/100, z/100) for x, y, z in points], [], visible_faces)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    hair = 'Hair' in name
    obj.hide_render = hair
    obj.color = (.58, .36, .22, 1) if name == 'Eve Body' else (.12, .38, .55, 1)
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    weights = totals[point_offset:point_offset+count]
    parts.append(dict(name=name, points=count, faces=face_count,
                      visible_faces=len(visible_faces),
                      unweighted=sum(w == 0 for w in weights),
                      maximum_weight_sum_error=max(abs(w-1) for w in weights)))
    point_offset += count
    face_offset += face_count
assert point_offset == len(source['points']) and face_offset == len(source['faces'])
s = bpy.context.scene
s.render.engine = 'BLENDER_WORKBENCH'
s.render.resolution_x, s.render.resolution_y, s.render.resolution_percentage = 720, 960, 100
s.display.shading.color_type = 'OBJECT'
s.display.shading.show_cavity = True
cam = bpy.data.objects.new('Review camera', bpy.data.cameras.new('Review camera'))
s.collection.objects.link(cam)
s.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = 2.05
target = Vector((0, 0, .94))
for label, direction in [('front', (0, -1, 0)), ('back', (0, 1, 0)), ('side', (1, 0, 0)), ('quarter', (1, -1, 0))]:
    cam.location = target + Vector(direction)*3
    cam.rotation_euler = (target-cam.location).to_track_quat('-Z', 'Y').to_euler()
    s.render.filepath = str(a.output/(label+'.png'))
    bpy.ops.render.render(write_still=True)
(a.output/'review.json').write_text(json.dumps(dict(
    scope='Existing exported geometry in bind pose, neutral materials and hair hidden for garment inspection. No source edit, animation, morph or game acceptance.',
    source=str(a.mesh), source_sha256=hashlib.sha256(raw).hexdigest(),
    hidden_materials=a.hide_material, body_mask=str(a.body_mask) if a.body_mask else None,
    hidden_face_count=len(hidden_faces), parts=parts), indent=2)+'\n')
