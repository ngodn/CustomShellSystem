"""Compare proxy normals with barycentrically transferred render normals."""
import hashlib
import json
from pathlib import Path
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

work = Path(__file__).resolve().parents[2] / 'work/eve26'
output = work / 'knit-normals1.json'
assert not output.exists()
mesh_path = work / 'knit-export2/knit.mesh.json'
mesh = json.loads(mesh_path.read_text())
audit = json.loads(mesh_path.with_suffix('.audit.json').read_text())
proxy = json.loads((work / 'knit-proxy1.json').read_text())
assert hashlib.sha256(mesh_path.read_bytes()).hexdigest() == proxy['report']['source_sha256'] == audit['output_sha256']
slot = proxy['slots']['Collar-1']
point_start = audit['parts'][0]['points']
face_start = audit['parts'][0]['faces']
faces = mesh['faces'][face_start:face_start+audit['parts'][1]['faces']]
corner_normals = {}
for face in faces:
    key = tuple(mesh['wedges'][i][0]-point_start for i in face[:3])
    assert key not in corner_normals
    corner_normals[key] = np.asarray([mesh['normals'][i] for i in face[:3]])
transferred = []
for row in slot['transfer']:
    normal = np.asarray(row['barycentric']) @ corner_normals[tuple(row['vertices'])]
    assert np.linalg.norm(normal) > .1
    transferred.append(normal/np.linalg.norm(normal))
transferred = np.asarray(transferred)
dot = np.einsum('ij,ij->i', transferred, slot['normals'])
body_faces = [[mesh['wedges'][i][0] for i in face[:3]] for face in mesh['faces'][:face_start]]
body = BVHTree.FromPolygons(mesh['points'][:point_start], body_faces, all_triangles=True)
nearby = []
for i, point in enumerate(slot['positions']):
    hit, _, _, distance = body.find_nearest(Vector(point))
    if .05 < distance < 1:
        direction = (np.asarray(point)-np.asarray(hit))/distance
        nearby.append(dict(vertex=i, distance_cm=distance,
            old_dot=float(direction @ np.asarray(slot['normals'][i])),
            transferred_dot=float(direction @ transferred[i])))
report = dict(source_sha256=audit['output_sha256'], count=len(dot),
    opposite=int((dot < 0).sum()), strongly_opposite=int((dot < -.9).sum()),
    dot_minimum=float(dot.min()), dot_median=float(np.median(dot)), dot_maximum=float(dot.max()),
    transferred_normals=transferred.tolist(), nearby_body=nearby,
    scope='Render-normal correspondence and unsigned nearest-body direction at rest; not an inside/outside or motion test.')
output.write_text(json.dumps(report, separators=(',', ':'))+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('transferred_normals','nearby_body')}))
