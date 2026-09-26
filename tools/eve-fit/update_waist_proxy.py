"""Transfer the verified source fit onto the existing cloth proxy without retopology."""
import hashlib
import json
import math
from pathlib import Path

w = Path(__file__).resolve().parents[2]/'work/eve26'
output = w/'panel-fit-proxy.json'
assert not output.exists()
slot = 'MI_CH_P_EVE_Christmas_01_01.001'
mesh_path = w/'holiday-waist-source.mesh.json'
mesh = json.loads(mesh_path.read_text())
baseline = json.loads((w/'holiday.mesh.json').read_text())
full = json.loads((w/'skirt-proxies.json').read_text())['slots'][slot]
ids = json.loads((w/'skirt-f11.json').read_text())['source_vertices']
assert len(ids) == len(full['positions'])
assert max(math.dist(baseline['points'][i], p) for i, p in zip(ids, full['positions'], strict=True)) < .001
proxy = json.loads((w/'panel-low-proxy.json').read_text())['slots'][slot]
normals = {i: [0., 0., 0.] for i in ids}
material = mesh['materials'].index(slot)
for face in mesh['faces']:
    if face[3] != material:
        continue
    for wedge in face[:3]:
        i = mesh['wedges'][wedge][0]
        if i in normals:
            normals[i] = [a+b for a, b in zip(normals[i], mesh['normals'][wedge])]
def unit(v):
    length = math.sqrt(sum(n*n for n in v))
    assert length > 1e-8
    return [n/length for n in v]
normals = {i: unit(v) for i, v in normals.items()}
for index, transfer in enumerate(proxy['transfer']):
    source = [ids[i] for i in transfer['vertices']]
    bary = transfer['barycentric']
    assert abs(sum(bary)-1.) < 1e-6 and min(bary) >= 0
    p = [sum(mesh['points'][i][j]*factor for i, factor in zip(source, bary)) for j in range(3)]
    n = unit([sum(normals[i][j]*factor for i, factor in zip(source, bary)) for j in range(3)])
    proxy['positions'][index] = p
    proxy['normals'][index] = n
output.write_text(json.dumps({'scope': 'Existing coarse proxy topology, anchor height and intentional pelvis-based simulation weights retained. Source positions/normals transferred through the original barycentric correspondences; native mapping and motion require fresh verification.',
    'source_mesh': str(mesh_path), 'source_sha256': hashlib.sha256(mesh_path.read_bytes()).hexdigest(),
    'slots': {slot: proxy}})+'\n')
print('Transferred', len(proxy['positions']), 'proxy vertices from verified source')
