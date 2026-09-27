"""Create a private lower-skirt weight-continuity trial without changing geometry."""
import copy
import hashlib
import json
from pathlib import Path

import numpy as np
from mathutils.kdtree import KDTree

work = Path(__file__).resolve().parents[2] / 'work/eve26'
path = work / 'knit-export2/knit.mesh.json'
source = json.loads(path.read_text())
audit = json.loads(path.with_suffix('.audit.json').read_text())
assert hashlib.sha256(path.read_bytes()).hexdigest() == audit['output_sha256']
out = work / 'knit-w2'
assert not out.exists()
start = 0
for part in audit['parts']:
    if part['name'] == 'Eve Extras - Sweater':
        count = part['points']
        break
    start += part['points']
else:
    raise AssertionError('Sweater part missing')
points = np.asarray(source['points'])[start:start+count]
rows = [row for row in source['influences'] if start <= row[0] < start+count]
bones = sorted({row[1] for row in rows})
lookup = {bone: i for i, bone in enumerate(bones)}
weights = np.zeros((count, len(bones)))
for vertex, bone, weight in rows:
    weights[vertex-start, lookup[bone]] += weight
assert np.allclose(weights.sum(axis=1), 1, atol=1e-5)
original = weights.copy()
pairs = set()
for face in source['faces']:
    ids = [source['wedges'][w][0]-start for w in face[:3]]
    if not all(0 <= i < count for i in ids):
        continue
    pairs.update(tuple(sorted((ids[i], ids[(i+1)%3]))) for i in range(3))
tree = KDTree(count)
for i, point in enumerate(points):
    tree.insert(point, i)
tree.balance()
for i, point in enumerate(points):
    for _, j, distance in tree.find_range(point, .01):
        if j > i:
            pairs.add((i, j))
edges = np.asarray(sorted(pairs))
i, j = edges.T
length = np.linalg.norm(points[i]-points[j], axis=1)
# The torso stays fixed; the transition fades over the upper skirt in centimeters.
freedom = np.clip((110-points[:, 2])/8, 0, 1)
freedom = freedom*freedom*(3-2*freedom)
limit = np.maximum(length, .01)*.12
eligible = (freedom[i]+freedom[j]) > 0
for iteration in range(400):
    diff = weights[i]-weights[j]
    magnitude = np.linalg.norm(diff, axis=1)
    active = eligible & (magnitude-limit > 1e-5)
    if not active.any():
        break
    ia, ja = i[active], j[active]
    correction = diff[active]*((magnitude[active]-limit[active])/magnitude[active])[:, None]
    total = freedom[ia]+freedom[ja]
    move = np.zeros_like(weights)
    degree = np.zeros(count)
    np.add.at(move, ia, -correction*(freedom[ia]/total)[:, None])
    np.add.at(move, ja, correction*(freedom[ja]/total)[:, None])
    np.add.at(degree, ia, 1)
    np.add.at(degree, ja, 1)
    selected = degree > 0
    weights[selected] += move[selected]/degree[selected, None]
assert weights.min() > -1e-8
weights = np.maximum(weights, 0)
changed = np.max(np.abs(weights-original), axis=1) > 1e-8
assert not changed[freedom == 0].any()
result = copy.deepcopy(source)
changed_ids = {start+int(v) for v in np.flatnonzero(changed)}
result['influences'] = [row for row in source['influences'] if row[0] not in changed_ids]
for vertex in np.flatnonzero(changed):
    kept = sorted(enumerate(weights[vertex]), key=lambda x: -x[1])[:8]
    kept = [(bone, value) for bone, value in kept if value > 1e-7]
    total = sum(value for _, value in kept)
    assert total > 0
    result['influences'].extend([start+int(vertex), bones[bone], float(value/total)] for bone, value in kept)
assert all(result[k] == value for k, value in source.items() if k != 'influences')
assert [r for r in result['influences'] if r[0] not in changed_ids] == [r for r in source['influences'] if r[0] not in changed_ids]
out.mkdir()
target = out / 'knit.mesh.json'
target.write_text(json.dumps(result, separators=(',', ':'))+'\n')
audit['output_sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
target.with_suffix('.audit.json').write_text(json.dumps(audit, indent=2)+'\n')
receipt = dict(changed_vertices=len(changed_ids), iterations=iteration+1,
    source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
    body_and_geometry_and_morphs_unchanged=True, torso_above_110cm_unchanged=True,
    scope='Lower skirt weight-continuity trial. Eight-influence pruning applied. Requires pose, contact and physics review.')
(out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
print(receipt)
