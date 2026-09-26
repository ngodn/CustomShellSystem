"""Create an explicit garment-follow proxy trial, preserving fitted geometry and topology."""
import argparse
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a = p.parse_args()
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
source_path = w/'holiday-waist-source.mesh.json'
mesh = json.loads(source_path.read_text())
slot = 'MI_CH_P_EVE_Christmas_01_01.001'
reference = json.loads((w/'panel-fit-proxy.json').read_text())
proxy = reference['slots'][slot]
ids = json.loads((w/'skirt-f11.json').read_text())['source_vertices']
weights = {}
for vertex,bone,weight in mesh['influences']:
    weights.setdefault(vertex,[]).append((mesh['bones'][bone]['name'],weight))
before = hashlib.sha256(json.dumps({k:v for k,v in proxy.items() if k!='weights'},sort_keys=True).encode()).hexdigest()
changed = 0
for index,transfer in enumerate(proxy['transfer']):
    merged = {}
    assert min(transfer['barycentric'])>=0 and abs(sum(transfer['barycentric'])-1)<1e-6
    for source,amount in zip(transfer['vertices'],transfer['barycentric'],strict=True):
        for bone,weight in weights[ids[source]]:
            merged[bone] = merged.get(bone,0.)+amount*weight
    merged = {bone:weight for bone,weight in merged.items() if weight>1e-9}
    total = sum(merged.values())
    assert abs(total-1)<1e-5 and len(merged)<=12
    revised = [[bone,weight/total] for bone,weight in sorted(merged.items())]
    changed += int(dict(revised)!=dict(proxy['weights'][index]))
    proxy['weights'][index] = revised
after = hashlib.sha256(json.dumps({k:v for k,v in proxy.items() if k!='weights'},sort_keys=True).encode()).hexdigest()
assert before==after
a.output.write_text(json.dumps({'scope':'Explicit alternative to pelvis-based simulation weights. Uses fitted garment source weights through existing barycentric transfer. Geometry, normals, topology and anchor height unchanged. Not accepted physics.',
    'source_mesh':str(source_path),'source_sha256':hashlib.sha256(source_path.read_bytes()).hexdigest(),
    'changed_weight_rows':changed,'non_weight_data_sha256':after,'slots':{slot:proxy}},separators=(',',':'))+'\n')
print('Prepared',changed,'changed weight rows; geometry and topology preserved')
