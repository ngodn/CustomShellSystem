"""Check cooked morph metadata and the appended hand/body/hair pose path."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=WORK/'hand-postprocess-cooked-readback-v2'
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
HERE=Path(__file__).resolve().parent
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert not (OUT/'validation.json').exists()
assert load(OUT/'report.json')['passed']
assert all(load(OUT/(n+'.exit.json'))['exit_code']==0 for n in ('pack','verify','decode'))
assert load(WORK/'hand-postprocess-cook-v1/exit.json')['exit_code']==0
source=load(WORK/'hand-morph-import-regression-v1/input.mesh.json')
stem=source['mesh_package'].rsplit('/',1)[1];decoded=OUT/'decoded'
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
assert old.read_bytes()==new.read_bytes()
assert load(previous/'SK_ArmRestV44B2Correctives_V1.refskel.json')==load(decoded/(stem+'.refskel.json'))
blueprint=load(decoded/'ABP_CSS_ControlRigProbeHandB2ImportedV1.json')
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
assert not props.get('CSSHandEnabled',False) and not props.get('CSSHandInputIsV43Compatible',False)
assert props['AnimGraphNode_Root']['Result']['LinkID']==4 and hand['Source']['LinkID']==2
assert by_count[29]['Source']['LinkID']==3 and by_count[7]['Source']['LinkID']==1
assert props['AnimGraphNode_LinkedInputPose']['bIsOutputLinked']
outputs=[{b['BoneName'] for b in by_count[n]['OutputBonesToTransfer']} for n in (7,19,29)]
assert all(not a&b for i,a in enumerate(outputs) for b in outputs[i+1:])
baseline_path=next((MOD/'work/cooked-readback-hand-v43/decoded').rglob('ABP_SeduXtress_BodyHairV42.json'))
baseline_default=next(x for x in load(baseline_path) if x['Name'].startswith('Default__'))
baseline=baseline_default['Properties']
preserved={k:v for k,v in baseline.items() if k.startswith('CSS')}
assert all(props[k]==v for k,v in preserved.items())
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
report=dict(passed=True,morph_driver_flags=22,actorx_byte_identical=True,bind_identical=True,
    output_bone_counts=[7,29,19],body_hair_nodes_preserved=True,body_hair_defaults_preserved=list(preserved),
    hand_default_enabled=False,source_hashes={str(p):digest(p) for p in (new,old,baseline_path)},
    scope='Cooked metadata, unchanged decoded mesh data, graph connections and property copies. Actual component execution is separately verified in editor; no cooked/live gameplay execution claim.')
(OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('source_hashes','body_hair_defaults_preserved')}))
