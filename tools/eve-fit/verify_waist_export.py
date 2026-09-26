"""Verify the freshly reloaded waist source against its tested geometry trial."""
import hashlib
import json
import math
from collections import Counter
from pathlib import Path

w = Path(__file__).resolve().parents[2]/'work/eve26'
output = w/'waist-source-verified.json'
assert not output.exists()
baseline = json.loads((w/'holiday-hip-clean.mesh.json').read_text())
expected = json.loads((w/'holiday-waist2.mesh.json').read_text())
actual = json.loads((w/'holiday-waist-source.mesh.json').read_text())
receipt = json.loads((w/'holiday-waist-source.mesh.receipt.json').read_text())
assert receipt['source_unchanged']
assert receipt['weight_cleanup_sha256'] == hashlib.sha256(Path(__file__).with_name('clean_weights.py').read_bytes()).hexdigest()
for key in ('bones', 'materials', 'uv_channels', 'colors', 'influences'):
    assert actual[key] == baseline[key], key
assert Counter(map(tuple, actual['wedges'])) == Counter(map(tuple, baseline['wedges'])), 'Corner UV assignments changed'
def triangles(mesh):
    return [tuple(mesh['wedges'][w][0] for w in f[:3]) for f in mesh['faces']]
old_faces, new_faces = triangles(baseline), triangles(actual)
changed = [i for i, (a, b) in enumerate(zip(old_faces, new_faces, strict=True)) if a != b]
offset_ids = {row[0] for row in json.loads((w/'waist2-offsets.json').read_text())['offsets']}
def boundary(pair):
    edges = Counter((face[i], face[(i+1)%3]) for face in pair for i in range(3))
    return Counter({edge: n-edges[edge[::-1]] for edge, n in edges.items() if n > edges[edge[::-1]]})
assert len(changed)%2 == 0
for i, j in zip(changed[::2], changed[1::2], strict=True):
    assert j == i+1
    old, new = old_faces[i:j+1], new_faces[i:j+1]
    vertices = set(old[0]+old[1])
    assert len(vertices) == 4 and vertices == set(new[0]+new[1])
    assert vertices & offset_ids
    assert boundary(old) == boundary(new), 'Quad boundary/winding changed'
    assert all(actual['faces'][k][3] == baseline['faces'][k][3] for k in (i, j))
point_error = max(math.dist(a, b) for a, b in zip(actual['points'], expected['points'], strict=True))
assert point_error < .0005, point_error
body_count = json.loads((w/'holiday.mesh.audit.json').read_text())['parts'][0]['points']
assert actual['points'][:body_count] == baseline['points'][:body_count]
assert len(actual['morph_targets']) == len(baseline['morph_targets']) == 22
morph_error = 0.
for a, b in zip(actual['morph_targets'], baseline['morph_targets'], strict=True):
    assert a['name'] == b['name']
    for u, v in zip(a['deltas'], b['deltas'], strict=True):
        assert u[0] == v[0]
        if u[0] < body_count:
            assert u == v, 'Body morph changed'
        morph_error = max(morph_error, math.dist(u[1:], v[1:]))
assert morph_error < .0005, morph_error
report = {'scope': 'Fresh saved-source reload and export matches the sampled waist fit trial. No native cloth, all-outfit or game acceptance.',
    'point_error_cm': point_error, 'relative_morph_error_cm': morph_error,
    'body_points_and_morphs_unchanged': True, 'weights_bones_quad_boundaries_uvs_unchanged': True,
    'local_quad_diagonals_changed': len(changed)//2,
    'morph_targets': 22, 'export_sha256': hashlib.sha256((w/'holiday-waist-source.mesh.json').read_bytes()).hexdigest()}
output.write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
