"""Check independently decoded B2 transforms, GPU morph data and rig bytecode."""
import hashlib
import json
import math
import struct
from pathlib import Path
import numpy as np
from mathutils.kdtree import KDTree

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=WORK/'arm-rest-b2-cooked-readback-v1'
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert not (OUT/'validation.json').exists()
assert load(OUT/'report.json')['passed']
assert all(load(OUT/(name+'.exit.json'))['exit_code']==0 for name in ('pack','verify','decode','geometry'))
assert load(OUT/'geometry-validation.json')['pass']
source_path=WORK/'arm-rest-correctives-export-v1/candidate.mesh.json'
source=load(source_path)
imported=load(WORK/'arm-rest-b2-full-import-v1/readback.json')
assert digest(source_path)==imported['source_sha256']
stem=source['mesh_package'].rsplit('/',1)[1]
decoded=OUT/'decoded'
bind=load(decoded/(stem+'.refskel.json'))
saved=load(WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json')
skeleton=next(x for x in load(decoded/(source['skeleton_package'].rsplit('/',1)[1]+'.json')) if x['Type']=='Skeleton')
reference=skeleton['ReferenceSkeleton']
assert len(bind)==len(saved)==len(reference['FinalRefBoneInfo'])==len(reference['FinalRefBonePose'])==379
# The independent decoder exposes cooked transforms as float32. Compare on
# that representation, without widening the source/import orientation gate.
for actual,expected,info,pose in zip(bind,saved,reference['FinalRefBoneInfo'],reference['FinalRefBonePose'],strict=True):
    assert actual['name']==expected['name']==info['Name']
    assert actual['parent']==expected['parent']==info['ParentIndex']
    for field,key,axes in [('translation','Translation','XYZ'),('rotation','Rotation','XYZW'),('scale','Scale3D','XYZ')]:
        wanted=np.asarray(expected[field],dtype=np.float32)
        assert np.array_equal(np.asarray(actual[field],dtype=np.float32),wanted),(actual['name'],field,'mesh')
        assert np.array_equal(np.asarray([pose[key][a] for a in axes],dtype=np.float32),wanted),(actual['name'],field,'skeleton')

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
assert mesh['Properties']['Skeleton']['ObjectPath'].rsplit('.',1)[0]==source['skeleton_package']
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

rig=load(decoded/'CR_CSS_LeftHandCombinedV1.json')
cls=next(x for x in rig if x['Type'] in ('ControlRigBlueprintGeneratedClass','RigVMBlueprintGeneratedClass'))
assert cls['bCooked'] and cls['SuperStruct']==dict(ObjectName="Class'ControlRig'",ObjectPath='/Script/ControlRig')
properties={x['Name']:x['Type'] for x in cls['ChildProperties']}
assert all(properties[n]=='BoolProperty' for n in ('Enabled','InputIsV43Compatible','HandValid'))
default=next(x for x in rig if x['Name'].startswith('Default__'))['Properties']
# False scalar defaults may be omitted by cooked property serialization.
assert not default.get('Enabled',False) and not default.get('InputIsV43Compatible',False)
vm=cls['VM'];other=next(x for x in rig if x['Type']=='RigVM')
assert vm['ByteCodeStorage']==other['ByteCodeStorage'] and vm['FunctionNamesStorage']==other['FunctionNamesStorage']
instructions=vm['ByteCodeStorage']['Instructions'];functions=vm['FunctionNamesStorage']
assert instructions and functions and vm['ByteCodeStorage']['Entries']==['(Name="Forwards Solve")']
assert all(0<=x['FunctionIndex']<len(functions) for x in instructions if 'FunctionIndex' in x)
hierarchy=next(x for x in rig if x['Type']=='RigHierarchy')['Elements']
curves={x['LoadedKey']['Name'] for x in hierarchy if x['LoadedKey']['Type']==8}
expected_curves={p['shape'] for p in load(Path(__file__).parent/'left-finger-correctives-v1.json')['parameters']}
assert curves==expected_curves|{'CSS_Diagnostic_UnrelatedCurve'}
assert all(digest(Path(p))==h for p,h in imported['protected_hashes'].items())
report=dict(passed=True,bones=379,decoded_bind_float32_equal=True,morph_count=22,morphs=rows,
    position_precision_cm=precision,delta_comparison_limit_cm=limit,import_threshold_cm=.015,
    rig=dict(instructions=len(instructions),functions=len(functions),curves=len(curves),default_enabled=False),
    source_sha256=digest(source_path),actorx_sha256=digest(path),
    scope='Cooked transforms, geometry/UV report, compressed morph data and static VM structure. Not GPU morph execution, live function resolution, game Skeleton compatibility or weapon acceptance.')
(OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(passed=True,bones=379,morphs=22,maximum_delta_error_cm=max(r['maximum_delta_error_cm'] for r in rows),rig=report['rig'])))
