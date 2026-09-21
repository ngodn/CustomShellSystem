"""Validate cooked heel morphs and exact existing skin weights against V44. Blender 5.2.2."""
import hashlib
import json
import math
import os
import struct
from pathlib import Path
import numpy as np
from mathutils.kdtree import KDTree
CSS = Path(__file__).resolve().parents[2]
OUT = Path(os.environ['CSS_CLOSURE_WORK']).resolve()
assert OUT.is_relative_to(CSS/'work') and not (OUT/'mesh-validation.json').exists()
WORK = CSS/'work/grip-grounding-v1'
load = lambda p: json.loads(p.read_text())
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert load(OUT/'geometry-validation.json')['pass']
assert load(OUT/'geometry-exit.json')['exit_code'] == load(OUT/'mesh-decode-exit.json')['exit_code'] == 0
source_path = OUT/'source.mesh.json'
source = load(source_path)
decoded = OUT/'decoded'
stem = source['mesh_package'].rsplit('/', 1)[1]
original = load(WORK/'heel-support-export-v2/candidate.mesh.json')
if (OUT/'ties.json').exists():
    corrections = load(OUT/'ties.json')
    assert digest(Path(corrections['source'])) == corrections['source_sha256']
    original_weights = {(r[0],r[1]):r for r in original['influences']}
    for change in corrections['changes']:
        for bone, before, after in zip(change['bones'],change['before'],change['after'],strict=True):
            row = original_weights[change['vertex'],bone]
            assert row[2] == before
            row[2] = after
assert {k:v for k,v in source.items() if k not in ('mesh_package','skeleton_package')} == {k:v for k,v in original.items() if k not in ('mesh_package','skeleton_package')}
old_decoded = WORK/'b2-body-bound-cooked-readback-v1/decoded'
assert load(decoded/(stem+'.refskel.json')) == load(old_decoded/'SK_B2PhysicsBound_V1.refskel.json')

path=next(decoded.rglob(stem+'.pskx'))
raw=path.read_bytes();offset=0;chunks={}
while offset<len(raw):
    assert offset+32<=len(raw)
    name,_,size,count=struct.unpack_from('<20s3i',raw,offset);offset+=32
    name=name.rstrip(b'\0').decode()
    assert name not in chunks and size>=0 and count>=0 and offset+size*count<=len(raw)
    chunks[name]=(size,count,raw[offset:offset+size*count]);offset+=size*count
assert offset==len(raw) and chunks['MRPHINFO'][0]==68 and chunks['MRPHDATA'][0]==28
morphs={};offset=0
for name,count in struct.iter_unpack('<64si',chunks['MRPHINFO'][2]):
    name=name.rstrip(b'\0').decode()
    assert name not in morphs and count>0 and offset+28*count<=len(chunks['MRPHDATA'][2])
    morphs[name]=chunks['MRPHDATA'][2][offset:offset+28*count];offset+=28*count
assert offset==len(chunks['MRPHDATA'][2])
assert set(morphs)=={m['name'] for m in source['morph_targets']} and len(morphs)==22
mesh=next(x for x in load(decoded/(stem+'.json')) if x['Type']=='SkeletalMesh')
assert mesh['Properties']['Skeleton']['ObjectPath'].rsplit('.',1)[0]=='/Game/CSS/Shared/SKEL_Base'
gpu=mesh['LODModels'][0]['MorphTargetVertexInfoBuffers']
assert len(gpu['BatchesPerMorph'])==22 and all(n>0 for n in gpu['BatchesPerMorph'])
points=np.frombuffer(chunks['PNTS0000'][2],dtype='<f4').reshape(-1,3).astype(float);points[:,1]*=-1
original=np.asarray(source['points']);tree=KDTree(len(original))
for i,p in enumerate(original):tree.insert(p,i)
tree.balance()
precision=float(gpu['PositionPrecision']);limit=precision*math.sqrt(3)+.00005
rows=[]
for shape in source['morph_targets']:
    name=shape['name'];expected={int(r[0]):np.asarray(r[1:]) for r in shape['deltas']}
    worst=0.;represented=set();seen=set();normal_peak=0.
    for dx,dy,dz,nx,ny,nz,index in struct.iter_unpack('<6fi',morphs[name]):
        assert 0<=index<len(points) and index not in seen;seen.add(index)
        assert all(math.isfinite(x) for x in (dx,dy,dz,nx,ny,nz))
        nearby=tree.find_range(points[index],.00005);assert nearby,(name,index)
        target=np.asarray([dx,dy,dz])
        choices=[(float(np.linalg.norm(target-expected.get(i,np.zeros(3)))),i) for _,i,_ in nearby]
        error=min(e for e,_ in choices);worst=max(worst,error)
        assert error<=limit,(name,index,error,limit)
        represented.update(i for e,i in choices if e<=limit)
        normal_peak=max(normal_peak,float(np.linalg.norm([nx,ny,nz])))
    missing=[float(np.linalg.norm(delta)) for i,delta in expected.items() if i not in represented]
    max_missing=max(missing,default=0.)
    # Default UE MorphThresholdPosition is 0.015 cm per component. Cooked
    # GPU deltas are quantized on PositionPrecision; retain both bounds.
    assert max_missing<=.015*math.sqrt(3)+limit,(name,max_missing)
    rows.append(dict(name=name,cooked_deltas=len(seen),source_deltas=len(expected),maximum_delta_error_cm=worst,
        omitted_source_points=len(missing),largest_omitted_delta_cm=max_missing,maximum_tangent_delta=normal_peak))


def skin(path):
    data = path.read_bytes()
    offset = 0
    buffers = {}
    while offset < len(data):
        assert offset + 32 <= len(data)
        name, _, size, count = struct.unpack_from('<20s3i', data, offset)
        offset += 32
        name = name.rstrip(b'\0').decode()
        assert name not in buffers and size >= 0 and count >= 0 and offset + size * count <= len(data)
        buffers[name] = data[offset:offset + size * count]
        offset += size * count
    positions = list(struct.iter_unpack('<3f', buffers['PNTS0000']))
    weights = [{} for _ in positions]
    for weight, vertex, bone in struct.iter_unpack('<fii', buffers['RAWWEIGHTS']):
        assert 0 <= vertex < len(positions) and bone not in weights[vertex]
        quantized = round(weight * 255)
        assert 0 < quantized <= 255 and abs(weight - quantized / 255) < 1e-7
        weights[vertex][bone] = quantized
    result = {}
    for point, weights_at_point in zip(positions, weights, strict=True):
        assert sum(weights_at_point.values()) == 255 and 1 <= len(weights_at_point) <= 8
        result.setdefault(point, set()).add(tuple(sorted(weights_at_point.items())))
    return result


old_actorx = next(old_decoded.rglob('SK_B2PhysicsBound_V1.pskx'))
old_skin, new_skin = skin(old_actorx), skin(path)
source_weights = [{} for _ in source['points']]
for vertex, bone, weight in source['influences']:
    source_weights[vertex][bone] = weight
source_points = {}
for i, (x, y, z) in enumerate(source['points']):
    key = struct.unpack('<3f', struct.pack('<3f', x, -y, z))
    source_points.setdefault(key, []).append(i)
assert old_skin.keys() <= new_skin.keys()
assert len(new_skin.keys() - old_skin.keys()) == 168
differences = [dict(point=point, before=sorted(values), after=sorted(new_skin[point]))
               for point, values in old_skin.items() if values != new_skin[point]]
(OUT/'skin-equality.json').write_text(json.dumps(dict(passed=not differences, differences=differences),indent=2)+'\n')
assert not differences, ('Existing cooked skin weights changed',len(differences))

for point in new_skin.keys() - old_skin.keys():
    assert all(v >= 133066 for v in source_points[point])
    for weights_at_point in new_skin[point]:
        actual = dict(weights_at_point)
        assert any(actual.keys() == source_weights[v].keys() and
                   all(abs(weight / 255 - source_weights[v][bone]) <= len(actual) / 255 for bone, weight in actual.items())
                   for v in source_points[point]), 'Heel weight differs beyond quantization'

report = dict(passed=True, morphs=rows, bones=379, source_sha256=digest(source_path),
    actorx_sha256=digest(path), baseline_actorx_sha256=digest(old_actorx),
    existing_skin_weights_exact=True, added_support_positions=168,
    original_positions_preserved=True,
    scope='Source geometry/UV gate, 22 compressed morphs, exact bind and existing skin-weight assignments; live appearance remains open.')
(OUT / 'mesh-validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(dict(passed=True, morphs=22, existing_skin_weights_exact=True)))
