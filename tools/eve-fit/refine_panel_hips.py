"""Add contact samples inside existing hip triangles without changing the rest surface."""
import argparse
import hashlib
import json
import math
from collections import defaultdict
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--receipt', type=Path, required=True)
a = p.parse_args()
assert not a.output.exists() and not a.receipt.exists()
data = json.loads(a.source.read_text())
slot = data['slots']['MI_CH_P_EVE_Christmas_01_01.001']
points = slot['positions']
original_count = len(points)
indices = slot['indices']
faces = [indices[i:i+3] for i in range(0, len(indices), 3)]
assert all(len(f) == 3 for f in faces)
new_faces, added = [], []
for fi, face in enumerate(faces):
    xyz = [points[i] for i in face]
    zs = [v[2] for v in xyz]
    largest_edge = max(math.dist(xyz[i], xyz[(i+1)%3]) for i in range(3))
    if max(zs) < 100 or min(zs) > 119 or largest_edge < 2:
        new_faces.append(face)
        continue
    index = len(points)
    points.append([sum(v[k] for v in xyz)/3 for k in range(3)])
    normal = [sum(slot['normals'][i][k] for i in face) for k in range(3)]
    length = math.sqrt(sum(x*x for x in normal))
    assert length > 1e-8
    slot['normals'].append([x/length for x in normal])
    weights = defaultdict(float)
    for i in face:
        for name, weight in slot['weights'][i]:
            weights[name] += weight/3
    total = sum(weights.values())
    assert abs(total-1) < .001
    slot['weights'].append([[name, weight/total] for name, weight in sorted(weights.items())])
    new_faces.extend([[face[j], face[(j+1)%3], index] for j in range(3)])
    added.append({'vertex': index, 'source_face': fi, 'vertices': face, 'barycentric': [1/3]*3})
assert added and len(points) < 5000
slot['indices'] = [i for f in new_faces for i in f]
# Original source transfer records cannot describe the new centroid vertices.
# Keep their provenance separately so callers cannot mistake them for a full map.
slot['original_vertex_transfer'] = slot.pop('transfer')
slot['refinement'] = added
data['scope'] = 'Private contact-sampling trial. Centroid subdivision preserves the piecewise planar rest surface and original vertices/weights. Not production or morph acceptance.'
data['refinement_source'] = str(a.source)
data['refinement_source_sha256'] = hashlib.sha256(a.source.read_bytes()).hexdigest()
a.output.write_text(json.dumps(data, separators=(',', ':'))+'\n')
report = {'source': str(a.source), 'source_sha256': data['refinement_source_sha256'],
          'output_sha256': hashlib.sha256(a.output.read_bytes()).hexdigest(),
          'original_vertices': original_count, 'vertices': len(points),
          'original_faces': len(faces), 'faces': len(new_faces),
          'added_centroids': len(added), 'rest_height_band_cm': [100, 119],
          'minimum_longest_edge_cm': 2,
          'scope': data['scope']}
a.receipt.write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report))
