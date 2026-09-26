"""Blender 5.2: bounded bind-pose clearance candidate for the Prototype suit.

Nearest normals propose local edits, not a containment or gameplay acceptance test.
Source blend and production assets are never written.
"""
import argparse
import copy
import hashlib
import json
import sys
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--mesh', type=Path, required=True)
p.add_argument('--audit', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--surface-pass', action='store_true')
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
a.output.mkdir(exist_ok=False)
raw = a.mesh.read_bytes()
data = json.loads(raw)
audit = json.loads(a.audit.read_text())
assert hashlib.sha256(raw).hexdigest() == audit['output_sha256']
assert audit['parts'][0]['name'] == 'Eve Body'
assert audit['parts'][1]['name'] == 'Eve Prototype Planet Diving Suit - Suit'
body_count = audit['parts'][0]['points']
suit_count = audit['parts'][1]['points']
body_faces = [[data['wedges'][w][0] for w in f[:3]]
              for f in data['faces'][:audit['parts'][0]['faces']]]
points = [Vector((x, -y, z)) for x, y, z in data['points']]
tree = BVHTree.FromPolygons(points[:body_count], body_faces, all_triangles=True)
face_normals = []
winding_disagreements = 0
for face, indices in zip(data['faces'][:audit['parts'][0]['faces']], body_faces):
    v0, v1, v2 = (points[i] for i in indices)
    geometric = (v1-v0).cross(v2-v0)
    normals = [Vector((data['normals'][w][0], -data['normals'][w][1], data['normals'][w][2])) for w in face[:3]]
    authored = sum(normals, Vector()).normalized()
    face_normals.append(authored)
    winding_disagreements += geometric.dot(authored) < 0
assert winding_disagreements < len(body_faces)*.01, 'Systemic body winding mismatch'
candidate = copy.deepcopy(data)
offsets = []
skipped = []
for i in range(body_count, body_count + suit_count):
    point = points[i]
    # Shoes need source-pose correction, not displacement toward a bare foot.
    if point.z < 22:
        continue
    location, normal, face, distance = tree.find_nearest(point)
    normal = face_normals[face]
    signed = (point-location).dot(normal)
    if signed >= .12:
        continue
    if distance > .6:
        if signed < 0:
            skipped.append(i)
        continue
    delta = normal * (.12-signed)
    value = point + delta
    candidate['points'][i] = [value.x, -value.y, value.z]
    offsets.append([i, delta.x, -delta.y, delta.z])
surface_hits = 0
if a.surface_pass:
    suit_start = audit['parts'][0]['faces']
    suit_faces = [[data['wedges'][w][0] for w in f[:3]]
                  for f in data['faces'][suit_start:suit_start+audit['parts'][1]['faces']]]
    updated = [Vector((x, -y, z)) for x, y, z in candidate['points']]
    garment = BVHTree.FromPolygons(updated, suit_faces, all_triangles=True)
    proposals = {}
    for point in points[:body_count]:
        if not 115 < point.z < 160:
            continue
        location, normal, face, distance = garment.find_nearest(point)
        signed = (point-location).dot(normal)
        # Reject nearest boundary points around intentional openings.
        if not .001 < signed < .6 or distance > signed*1.01:
            continue
        indices = suit_faces[face]
        v0, v1, v2 = (updated[j] for j in indices)
        u, v, w = v1-v0, v2-v0, location-v0
        denom = u.dot(u)*v.dot(v)-u.dot(v)**2
        if denom <= 1e-10:
            continue
        b = (v.dot(v)*w.dot(u)-u.dot(v)*w.dot(v))/denom
        c = (u.dot(u)*w.dot(v)-u.dot(v)*w.dot(u))/denom
        if min(1-b-c, b, c) < .03:
            continue
        surface_hits += 1
        delta = normal*(signed+.12)
        for j in indices:
            if j not in proposals or proposals[j].length < delta.length:
                proposals[j] = delta
    for j, delta in proposals.items():
        value = updated[j]+delta
        candidate['points'][j] = [value.x, -value.y, value.z]
    offsets = [[i, *[v-o for v, o in zip(candidate['points'][i], data['points'][i])]]
               for i in range(body_count, body_count+suit_count)
               if candidate['points'][i] != data['points'][i]]
assert candidate['points'][:body_count] == data['points'][:body_count]
assert candidate['points'][body_count+suit_count:] == data['points'][body_count+suit_count:]
assert all(candidate[k] == v for k, v in data.items() if k != 'points')
output = a.output/'planet.mesh.json'
output.write_text(json.dumps(candidate, separators=(',', ':'))+'\n')
audit['output_sha256'] = hashlib.sha256(output.read_bytes()).hexdigest()
audit['stage'] = 'Private bind-pose clearance candidate; not for import or release'
(a.output/'planet.mesh.audit.json').write_text(json.dumps(audit, indent=2)+'\n')
(a.output/'offsets.json').write_text(json.dumps({'offsets': offsets})+'\n')
receipt = dict(source=str(a.mesh), source_sha256=hashlib.sha256(raw).hexdigest(),
               changed_vertices=len(offsets), max_offset_cm=max(Vector(v[1:]).length for v in offsets),
               skipped_deep_candidates=len(skipped), body_unchanged=True,
               winding_disagreements=winding_disagreements,
               surface_hits=surface_hits,
               other_parts_unchanged=True, non_point_fields_unchanged=True,
               scope='Bind-pose proposal only. Requires visual review, source mapping, morph and motion validation.')
(a.output/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
print(json.dumps(receipt))
