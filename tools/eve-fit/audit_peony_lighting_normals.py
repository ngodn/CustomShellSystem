"""Inspect cooked geometric normals; this does not sample normal-map textures."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import struct


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
audit = json.loads((args.source / 'skin-audit.json').read_text())
surface = json.loads((args.source / 'skin-surface.json').read_text())
assert audit['vertex_stride'] == 76 and surface['stride'] == 60
blobs = {}
for row in [*audit['files'], surface['file']]:
    blob = (args.source / row['name']).read_bytes()
    assert len(blob) == row['bytes']
    assert hashlib.sha256(blob).hexdigest() == row['sha256']
    blobs[row['name']] = blob
positions = [struct.unpack_from('<3f', blobs['skin-vertices.bin'], i * 76)
             for i in range(audit['vertex_count'])]
normals = [struct.unpack_from('<3f', blobs['skin-surface.bin'], i * 60)
           for i in range(audit['vertex_count'])]
indices = [row[0] for row in struct.iter_unpack('<I', blobs['skin-indices.bin'])]
rows = []
for section in audit['sections']:
    dots, degenerate = [], 0
    start = section['FirstIndex']
    for first in range(start, start + 3 * section['NumFaces'], 3):
        ids = indices[first:first + 3]
        a, b, c = [positions[i] for i in ids]
        u, v = [b[i] - a[i] for i in range(3)], [c[i] - a[i] for i in range(3)]
        cross = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
        mean = [sum(normals[j][i] for j in ids) for i in range(3)]
        length = math.sqrt(sum(x*x for x in cross) * sum(x*x for x in mean))
        if length < 1e-12:
            degenerate += 1
        else:
            dots.append(sum(x*y for x, y in zip(cross, mean)) / length)
    if not dots:
        continue
    rows.append(dict(slot=audit['material_slots'][section['MaterialIndex']],
                     material_index=section['MaterialIndex'], triangles=len(dots),
                     degenerate=degenerate, median_dot=statistics.median(dots),
                     dot_below_minus_half=sum(d < -.5 for d in dots),
                     dot_above_half=sum(d > .5 for d in dots)))
fabric = [row for row in rows if row['material_index'] in (21, 22, 26, 28)]
assert len(fabric) == 4
body = [row for row in rows if row['slot'] in ('Eve Body', 'Eve Legs', 'Eve Arms')]
assert len(body) == 3 and all(abs(row['median_dot']) > .9 for row in body)
orientation = math.copysign(1., body[0]['median_dot'])
assert all(row['median_dot'] * orientation > .9 for row in body)
for row in rows:
    row['median_alignment_relative_to_body'] = row['median_dot'] * orientation
result = dict(rows=rows, body_winding_reference=body,
              inputs=[*audit['files'], surface['file']],
              scope=__doc__, limitation='Reference pose only. Face-average alignment does not establish '
              'tangent handedness, normal-map interpretation, deformed normals or correct lighting.')
with args.output.open('x') as output:
    json.dump(result, output, indent=2)
    output.write('\n')
print(json.dumps(fabric, indent=2))
