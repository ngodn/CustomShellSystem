"""Check the cooked B2 game-reference binding, defaults, mesh payload and pose path."""
import hashlib
import json
import struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=WORK/'b2-reference-cooked-readback-v1'
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
HERE=Path(__file__).resolve().parent
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert not (OUT/'validation.json').exists()
assert load(OUT/'report.json')['passed']
assert all(load(OUT/(n+'.exit.json'))['exit_code']==0 for n in ('pack','verify','decode'))
assert load(WORK/'b2-reference-cook-v1/exit.json')['exit_code']==0
source=load(WORK/'hand-morph-import-regression-v1/input.mesh.json')
stem='SK_B2GameReferenceMetadata_V2';decoded=OUT/'decoded'
mesh_package=load(decoded/(stem+'.json'))
metadata=next(x for x in mesh_package if x['Type']=='AnimCurveMetaData')
curves=metadata['Properties']['CurveMetaData']
assert len(curves)==22 and {c['Key'] for c in curves}=={c['name'] for c in source['morph_targets']}
assert all(c['Value']['Type']==dict(bMaterial=False,bMorphtarget=True) and
           c['Value']['LinkedBones']==[] and c['Value']['MaxLOD']==255 for c in curves)
mesh=next(x for x in mesh_package if x['Type']=='SkeletalMesh')
assert any('AnimCurveMetaData' in p['ObjectName'] for p in mesh['Properties']['AssetUserData'])
previous=WORK/'arm-rest-b2-cooked-readback-v1/decoded'
old=next(previous.rglob('SK_ArmRestV44B2Correctives_V1.pskx'));new=next(decoded.rglob(stem+'.pskx'))
def chunks(path):
    raw=path.read_bytes();offset=0;result={}
    while offset<len(raw):
        assert offset+32<=len(raw)
        name,_,size,count=struct.unpack_from('<20s3i',raw,offset);offset+=32
        name=name.rstrip(b'\0').decode()
        assert name not in result and size>=0 and count>=0 and offset+size*count<=len(raw)
        result[name]=(size,count,raw[offset:offset+size*count]);offset+=size*count
    assert offset==len(raw)
    return result
old_chunks,new_chunks=chunks(old),chunks(new)
assert set(old_chunks)==set(new_chunks)
for name in old_chunks:
    if name!='MATT0000':assert old_chunks[name]==new_chunks[name],name
# Assigned material names change; geometry, faces, weights, UVs and morphs do not.
a,b=old_chunks['MATT0000'],new_chunks['MATT0000'];assert a[:2]==b[:2] and a[0]==88
for index in range(a[1]):assert a[2][index*88+64:(index+1)*88]==b[2][index*88+64:(index+1)*88]

assert load(previous/'SK_ArmRestV44B2Correctives_V1.refskel.json')==load(decoded/(stem+'.refskel.json'))
blueprint=load(decoded/'ABP_CSS_ControlRigProbeHandB2ReferenceV2.json')
cls=next(x for x in blueprint if x['Type']=='AnimBlueprintGeneratedClass');assert cls['bCooked']
defaults=next(x for x in blueprint if x['Name'].startswith('Default__'))
props=defaults['Properties']
nodes={k:v for k,v in props.items() if k.startswith('AnimGraphNode_ControlRig')}
assert len(nodes)==3
by_count={len(n['OutputBonesToTransfer']):n for n in nodes.values()}
assert set(by_count)=={7,19,29}
hand=by_count[19]
assert [b['BoneName'] for b in hand['OutputBonesToTransfer']]==[p['bone'] for p in load(HERE/'left-finger-calibration-v1.json')['parameters']]
assert hand['DestPropertyNames']==['Enabled','InputIsV43Compatible']
assert hand['ControlRigClass']['ObjectPath'].rsplit('.',1)[0]=='/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCombinedV1'
assert hand['bTransferInputPose'] and hand['bTransferInputCurves'] and hand['bResetInputPoseToInitial']
assert not hand['bTransferPoseInGlobalSpace'] and hand['InputBonesToTransfer']==[] and hand['bExecute'] and hand['Alpha']==1
assert props['CSSHandEnabled'] and not props.get('CSSHandInputIsV43Compatible',False)
assert props['AnimGraphNode_Root']['Result']['LinkID']==4 and hand['Source']['LinkID']==2
assert by_count[29]['Source']['LinkID']==3 and by_count[7]['Source']['LinkID']==1
assert props['AnimGraphNode_LinkedInputPose']['bIsOutputLinked']
outputs=[{b['BoneName'] for b in by_count[n]['OutputBonesToTransfer']} for n in (7,19,29)]
assert all(not a&b for i,a in enumerate(outputs) for b in outputs[i+1:])
baseline_path=next((MOD/'work/cooked-readback-hand-v43/decoded').rglob('ABP_SeduXtress_BodyHairV42.json'))
baseline_default=next(x for x in load(baseline_path) if x['Name'].startswith('Default__'))
baseline=baseline_default['Properties']
preserved={k:v for k,v in baseline.items() if k.startswith('CSS')}
assert all(props[k]==v for k,v in preserved.items() if k not in ('CSSStiffness','CSSDamping'))
assert props['CSSStiffness']==200 and props['CSSDamping']==24
old_nodes={len(v['OutputBonesToTransfer']):v for k,v in baseline.items() if k.startswith('AnimGraphNode_ControlRig')}
for n in (7,29):
    # Duplication assigns new node GUIDs to generated property names. Check
    # their complete member-to-pin mapping below, not literal GUID equality.
    assert {k:v for k,v in old_nodes[n].items() if k not in ('Source','SourcePropertyNames')}=={
        k:v for k,v in by_count[n].items() if k not in ('Source','SourcePropertyNames')}
library=defaults['SerializedSparseClassData']['AnimBlueprintExtension_PropertyAccess']['Library']
segments=library['PathSegments']
paths=lambda name:[segments[p['PathSegmentStartIndex']]['Name'] for p in library[name] if p['PathSegmentCount']==1]
sources=paths('SrcPaths');destinations=paths('DestPaths')
assert len(sources)==len(destinations)
mapping=dict(zip(sources,destinations))
assert all(mapping['CSSHand'+n]==p for n,p in zip(hand['DestPropertyNames'],hand['SourcePropertyNames'],strict=True))
def member_map(default):
    lib=default['SerializedSparseClassData']['AnimBlueprintExtension_PropertyAccess']['Library']
    seg=lib['PathSegments']
    paths=lambda name:[seg[p['PathSegmentStartIndex']]['Name'] for p in lib[name] if p['PathSegmentCount']==1]
    return dict(zip(paths('DestPaths'),paths('SrcPaths'),strict=True))
old_members,new_members=member_map(baseline_default),member_map(defaults)
for n in (7,29):
    assert [old_members[p] for p in old_nodes[n]['SourcePropertyNames']]==[new_members[p] for p in by_count[n]['SourcePropertyNames']]
# Verify the bound Skeleton separately from the unchanged mesh bind.
skel_package='/Game/CSSAuthoring/DiagnosticReferences/SKEL_B2GameReferenceMetadata_V2'
bp_package='/Game/CSS/TransientProbes/ABP_CSS_ControlRigProbeHandB2ReferenceV2'
assert mesh['Properties']['Skeleton']['ObjectPath'].rsplit('.',1)[0]==skel_package
assert mesh['Properties']['PostProcessAnimBlueprint']['ObjectPath'].rsplit('.',1)[0]==bp_package
assert cls['Properties']['TargetSkeleton']['ObjectPath'].rsplit('.',1)[0]==skel_package
skeleton=next(x for x in load(decoded/'SKEL_B2GameReferenceMetadata_V2.json') if x['Type']=='Skeleton')
reference=skeleton['ReferenceSkeleton'];saved=load(WORK/'b2-reference-metadata-v2/candidate-reference.json')
# Cooked serialization contains raw entries; virtual definitions are separate.
assert len(reference['FinalRefBoneInfo'])==len(reference['FinalRefBonePose'])==len(saved)==379
# JSON can erase the sign of zero. Compare numeric float32 values exactly.
f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
for expected,info,pose in zip(saved,reference['FinalRefBoneInfo'],reference['FinalRefBonePose'],strict=True):
    assert expected['name']==info['Name'] and expected['parent']==info['ParentIndex']
    for key,field,axes in [('translation','Translation','XYZ'),('rotation','Rotation','XYZW'),('scale','Scale3D','XYZ')]:
        assert all(f32(a)==f32(pose[field][axis]) for a,axis in zip(expected[key],axes,strict=True)),(expected['name'],key)
old_skel=next(x for x in load(next((MOD/'work/cooked-readback-hand-v43/decoded').rglob('SKEL_CSS_Base.json'))) if x['Type']=='Skeleton')
for name in ('BoneTree','VirtualBones','CompatibleSkeletons','bUseRetargetModesFromCompatibleSkeleton','Sockets','BlendProfiles','SlotGroups'):
    assert skeleton['Properties'].get(name)==old_skel['Properties'].get(name),name
assert len(skeleton['Properties']['VirtualBones'])==9
old_mesh=next(x for x in load(next((MOD/'work/cooked-readback-hand-v43/decoded').rglob('SK_SeduXtress_HandBindV43.json'))) if x['Type']=='SkeletalMesh')
assert len(mesh['SkeletalMaterials'])==len(old_mesh['SkeletalMaterials'])==30
for a,b in zip(mesh['SkeletalMaterials'],old_mesh['SkeletalMaterials'],strict=True):
    for key in ('MaterialSlotName','Material','ImportedMaterialSlotName'):
        assert a[key]==b[key],(key,a,b)
assert not mesh['Properties'].get('PhysicsAsset')
rig=load(decoded/'CR_CSS_LeftHandCombinedV1.json')
old_rig=load(WORK/'hand-postprocess-cooked-readback-v2/decoded/CR_CSS_LeftHandCombinedV1.json')
assert rig==old_rig
report=dict(passed=True,morph_driver_flags=22,actorx_geometry_skin_morph_bytes_identical=True,bind_identical=True,
    output_bone_counts=[7,29,19],body_hair_nodes_preserved=True,body_hair_defaults_preserved=[k for k in preserved if k not in ('CSSStiffness','CSSDamping')],
    hand_default_enabled=True,hair_defaults=dict(stiffness=200,damping=24),source_hashes={str(p):digest(p) for p in (new,old,baseline_path)},
    scope='Cooked reference rotations, nine virtual definitions, material bindings, morph metadata, unchanged geometry/skin/morph bytes, hand rig and control wiring. Physics Asset is still absent. Actual component execution is separately verified in editor; no cooked/live gameplay execution claim.')
(OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('source_hashes','body_hair_defaults_preserved')}))
