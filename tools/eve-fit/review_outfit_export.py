"""Review an existing outfit interchange without opening or saving its source blend."""
import argparse
import hashlib
import json
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector, Matrix, Quaternion
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--probe',nargs=3,metavar=('VIEW','X','Y'),help='Trace an orthographic review pixel through each visible object')
p.add_argument('--mesh', type=Path, required=True)
p.add_argument('--audit', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--hide-material', action='append', default=[])
p.add_argument('--hide-part', action='append', default=[])
p.add_argument('--upper-body', action='store_true')
p.add_argument('--hip-detail', action='store_true')
p.add_argument('--foot-detail', action='store_true')
p.add_argument('--body-mask', type=Path)
p.add_argument('--exported-normals', action='store_true', help='Inspect saved corner normals in the unposed base mesh')
p.add_argument('--morph', action='append', default=[], help='Name=weight, applied to exported deltas')
p.add_argument('--pose-motion', type=Path, help='Recorded upstream animation snapshots, without cloth simulation')
p.add_argument('--pose-frame', type=int, default=0)
p.add_argument('--cloth-render', type=Path, help='Verified native mapping replay replacing its named material sections')
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
if a.probe:
    assert a.probe[0] in ('front','back','side','quarter')
    assert 0<=float(a.probe[1])<720 and 0<=float(a.probe[2])<960
assert not a.exported_normals or not (a.pose_motion or a.morph or a.cloth_render), 'Saved normals are base-pose only'
a.output.mkdir(exist_ok=False)
raw = a.mesh.read_bytes()
source = json.loads(raw)
morphs = {}
targets = {target['name']: target['deltas'] for target in source['morph_targets']}
for setting in a.morph:
    name, weight = setting.rsplit('=', 1)
    assert name in targets and name not in morphs
    weight = float(weight)
    assert -1 <= weight <= 1
    morphs[name] = weight
    for index, x, y, z in targets[name]:
        source['points'][index] = [v+weight*d for v, d in zip(source['points'][index], (x, y, z))]
if a.pose_motion:
    motion = json.loads(a.pose_motion.read_text())
    row = motion['frames'][a.pose_frame]
    snapshot = row['upstream'] if 'upstream' in row else row['pose']['Snapshot']
    assert snapshot.get('bIsValid', True)
    recorded = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
    bind, pose = [], []
    for bone in source['bones']:
        q = bone['rotation']
        local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
        parent = bone['parent']
        bind.append(bind[parent] @ local if parent >= 0 else local)
        t = recorded[bone['name']]
        local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),
            Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector([t['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[parent] @ local if parent >= 0 else local)
    matrices = np.asarray([np.asarray(p @ b.inverted()) for p, b in zip(pose, bind)])
    points = np.asarray(source['points'])
    influences = np.asarray(source['influences'])
    vertices, bones, weights = influences[:, 0].astype(int), influences[:, 1].astype(int), influences[:, 2]
    transformed = np.zeros_like(points)
    np.add.at(transformed, vertices, (np.einsum('nij,nj->ni', matrices[bones, :3, :3], points[vertices])+matrices[bones, :3, 3])*weights[:, None])
    assert np.isfinite(transformed).all()
    source['points'] = transformed.tolist()
assert set(a.hide_material) <= set(source['materials'])
hidden_slots = {source['materials'].index(name) for name in a.hide_material}
cloth = None
if a.cloth_render:
    assert a.pose_motion and not morphs, 'Cloth replay requires its recorded pose and no extra morphs'
    cloth = json.loads(a.cloth_render.read_text())
    assert cloth['frame'] == a.pose_frame
    assert cloth['simulation_sha256'] == hashlib.sha256(a.pose_motion.read_bytes()).hexdigest()
    for section in cloth['sections']:
        hidden_slots.add(source['materials'].index(section['material']))
hidden_faces = set(json.loads(a.body_mask.read_text())['hidden_body_faces']) if a.body_mask else set()
audit = json.loads(a.audit.read_text())
assert set(a.hide_part) <= {part['name'] for part in audit['parts']}
assert hashlib.sha256(raw).hexdigest() == audit['output_sha256']
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
totals = [0.] * len(source['points'])
for vertex, bone, weight in source['influences']:
    totals[vertex] += weight
point_offset = face_offset = 0
parts = []
face_records = {}
probe_hits = []
for part in audit['parts']:
    name, count, face_count = part['name'], part['points'], part['faces']
    points = source['points'][point_offset:point_offset+count]
    faces = [[source['wedges'][w][0]-point_offset for w in f[:3]]
             for f in source['faces'][face_offset:face_offset+face_count]]
    assert all(0 <= i < count for f in faces for i in f)
    visible_faces = [face for index, (face, record) in enumerate(zip(faces, source['faces'][face_offset:face_offset+face_count]), face_offset)
                     if record[3] not in hidden_slots and index not in hidden_faces]
    face_records[name] = [index for index,record in enumerate(source['faces'][face_offset:face_offset+face_count],face_offset) if record[3] not in hidden_slots and index not in hidden_faces]
    mesh = bpy.data.meshes.new(name)
    # The Y reflection converts Unreal clockwise faces to outward Blender winding.
    mesh.from_pydata([(x/100, -y/100, z/100) for x, y, z in points], [], visible_faces)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    hair = 'Hair' in name
    obj.hide_render = hair or name in a.hide_part
    obj.color = (.58, .36, .22, 1) if name == 'Eve Body' or name == 'Eve Skin Suit - Footwear Lining' else (.12, .38, .55, 1)
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    if a.exported_normals:
        corners=[source['normals'][w] for index,record in enumerate(source['faces'][face_offset:face_offset+face_count],face_offset)
                 if record[3] not in hidden_slots and index not in hidden_faces for w in record[:3]]
        mesh.normals_split_custom_set([(x,-y,z) for x,y,z in corners])
    weights = totals[point_offset:point_offset+count]
    parts.append(dict(name=name, points=count, faces=face_count,
                      visible_faces=len(visible_faces),
                      unweighted=sum(w == 0 for w in weights),
                      maximum_weight_sum_error=max(abs(w-1) for w in weights)))
    point_offset += count
    face_offset += face_count
assert point_offset == len(source['points']) and face_offset == len(source['faces'])
if cloth:
    for section in cloth['sections']:
        mesh = bpy.data.meshes.new(section['material'])
        faces = np.asarray(section['indices']).reshape(-1,3).tolist()
        mesh.from_pydata([(x/100,-y/100,z/100) for x,y,z in section['positions_cm']], [], faces)
        obj = bpy.data.objects.new(section['material'],mesh)
        bpy.context.collection.objects.link(obj)
        obj.color = (.65,.45,.12,1)
        for polygon in mesh.polygons:
            polygon.use_smooth = True
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
if a.upper_body:
    cam.data.ortho_scale = .85
    target = Vector((0, 0, 1.30))
if a.hip_detail:
    assert not a.upper_body
    cam.data.ortho_scale = .65
    target = Vector((0, 0, 1.05))
if a.foot_detail:
    assert not a.upper_body and not a.hip_detail
    cam.data.ortho_scale = .65
    target = Vector((0, 0, .12))
for label, direction in [('front', (0, -1, 0)), ('back', (0, 1, 0)), ('side', (1, 0, 0)), ('quarter', (1, -1, 0))]:
    cam.location = target + Vector(direction)*3
    cam.rotation_euler = (target-cam.location).to_track_quat('-Z', 'Y').to_euler()
    if a.probe and a.probe[0]==label:
        px,py=map(float,a.probe[1:]);height=cam.data.ortho_scale
        width=height*s.render.resolution_x/s.render.resolution_y
        rotation=cam.rotation_euler.to_matrix()
        origin=cam.location+rotation@Vector(((px+.5)/s.render.resolution_x*width-width/2,height/2-(py+.5)/s.render.resolution_y*height,0))
        direction=rotation@Vector((0,0,-1))
        for obj in s.objects:
            if obj.type!='MESH' or obj.hide_render:continue
            tree=BVHTree.FromPolygons([v.co for v in obj.data.vertices],[list(poly.vertices) for poly in obj.data.polygons])
            hit,normal,index,distance=tree.ray_cast(origin,direction)
            if hit is None:continue
            original_face=face_records.get(obj.name,[])[index] if obj.name in face_records else None
            vertices=[source['wedges'][k][0] for k in source['faces'][original_face][:3]] if original_face is not None else None
            probe_hits.append(dict(object=obj.name,distance_m=distance,point_m=list(hit),face=original_face,vertices=vertices))
        probe_hits.sort(key=lambda hit:hit['distance_m'])
    s.render.filepath = str(a.output/(label+'.png'))
    bpy.ops.render.render(write_still=True)
(a.output/'review.json').write_text(json.dumps(dict(
    scope='Offline garment review, neutral materials and hair hidden. Optional morphs and recorded upstream pose are explicit below. No physics simulation or game acceptance.',
    source=str(a.mesh), source_sha256=hashlib.sha256(raw).hexdigest(),
    hidden_materials=a.hide_material, body_mask=str(a.body_mask) if a.body_mask else None,
    hidden_parts=a.hide_part, upper_body=a.upper_body,
    probe=a.probe,probe_hits=probe_hits,
    hip_detail=a.hip_detail,
    foot_detail=a.foot_detail,
    exported_normals=a.exported_normals,
    hidden_face_count=len(hidden_faces), morphs=morphs,
    pose_motion=str(a.pose_motion) if a.pose_motion else None,
    cloth_render=str(a.cloth_render) if a.cloth_render else None,
    pose_frame=a.pose_frame if a.pose_motion else None, parts=parts), indent=2)+'\n')
