"""Trial lower-dress weights using existing CSS skirt bones; preserve geometry and morphs."""
import hashlib
import json
import math
from pathlib import Path

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
source = WORK / 'holiday.mesh.json'
output = WORK / 'holiday-bones.mesh.json'
assert not output.exists(), output
data = json.loads(source.read_text())
names = {b['name']: i for i, b in enumerate(data['bones'])}
slots = {
    'MI_CH_P_EVE_Christmas_01_01.001', 'MI_CH_P_EVE_Christmas_01_Decal.001',
    'MI_EVE_HR_Christmas_01_Fur.001', 'MI_EVE_HR_15_Emissive1.001',
    'MI_CH_P_EVE_Christmas_01_03.001',
}
material_ids = {data['materials'].index(s) for s in slots}
vertices = {data['wedges'][w][0] for f in data['faces'] if f[3] in material_ids for w in f[:3]}
original = {}
for vertex, bone, weight in data['influences']:
    original.setdefault(vertex, {})[bone] = weight
protected = {k: hashlib.sha256(json.dumps(data[k], separators=(',', ':')).encode()).hexdigest()
             for k in data if k not in ('influences', 'mesh_package')}
changes = {}
used = set()
directions = ['L', 'B', 'R', 'F']
for index in sorted(vertices):
    x, y, z = data['points'][index]
    alpha = min(1., max(0., (117.8838-z)/9.8808))
    alpha = alpha*alpha*(3-2*alpha)
    if alpha == 0:
        continue
    # Continuous interpolation around the four existing skirt chains.
    angle = math.atan2(y+1.2014805, x-.00028577) % (2*math.pi)
    sector = angle/(math.pi/2)
    first = int(sector)
    fraction = sector-first
    depth = min(2., max(0., (108.002975-z)/8.))
    upper = min(1, int(depth))
    lower_fraction = depth-upper
    row = {bone: weight*(1-alpha) for bone, weight in original[index].items()}
    for side, side_weight in [(directions[first], 1-fraction), (directions[(first+1)%4], fraction)]:
        for segment, segment_weight in [(upper+1, 1-lower_fraction), (upper+2, lower_fraction)]:
            name = f'CSS_Cloth_Skirt_{side}_{segment:02}'
            bone = names[name]
            row[bone] = row.get(bone, 0.)+alpha*side_weight*segment_weight
    row = {bone: weight for bone, weight in row.items() if weight > 1e-8}
    # Match the established eight-influence export limit, measuring discarded mass.
    ordered = sorted(row.items(), key=lambda item: (-item[1], item[0]))
    dropped = sum(weight for _, weight in ordered[8:])
    assert dropped < .015, (index, dropped)
    total = sum(weight for _, weight in ordered[:8])
    changes[index] = {bone: weight/total for bone, weight in ordered[:8]}
    used.update(names_inv for names_inv in changes[index] if data['bones'][names_inv]['name'].startswith('CSS_Cloth_Skirt'))
assert changes and len(used) == 12
data['influences'] = [[vertex, bone, weight] for vertex in sorted(original)
                      for bone, weight in sorted(changes.get(vertex, original[vertex]).items())]
data['mesh_package'] = '/Game/CSS/EveTest/SK_HolidayBones'
assert all(hashlib.sha256(json.dumps(data[k], separators=(',', ':')).encode()).hexdigest() == digest
           for k, digest in protected.items())
output.write_text(json.dumps(data, separators=(',', ':')))
report = {
    'changed_vertices': len(changes), 'garment_vertices': len(vertices),
    'skirt_bones': [data['bones'][i]['name'] for i in sorted(used)],
    'protected_fields_sha256': protected, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'scope': 'Weight-only export trial. Body, geometry, UVs, skeleton and all 22 morphs unchanged. No dynamics or visual acceptance.',
}
(WORK / 'skirt-bones.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps({k: report[k] for k in ('changed_vertices', 'garment_vertices', 'skirt_bones')}, indent=2))
