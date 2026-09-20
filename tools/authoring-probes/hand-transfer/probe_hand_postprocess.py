"""Check the complete hand stage after accepted body/hair on real components."""
import hashlib
import json
import math
import os
import sys
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=Path(os.environ['CSS_HAND_POSTPROCESS_DIR']).resolve()
assert OUT.parent==WORK.resolve()
OUT.mkdir(exist_ok=True);assert not (OUT/'report.json').exists()
HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE),str(ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools')]
from probe_controlrig_chain import key,set_value,transform
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
content=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
asset_path=lambda package:content/(package.removeprefix('/Game/')+'.uasset')
mesh_package='/Game/CSSAuthoring/DiagnosticReferences/SK_ArmRestV44B2Correctives_V1'
baseline_package='/Game/CSS/CSS_SeduXtress_eins0fx_P/ABP_SeduXtress_BodyHairV42'
rig_package='/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCombinedV1'
package='/Game/CSS/TransientProbes/ABP_CSS_ControlRigProbeHandB2V1'
readback=load(WORK/'arm-rest-b2-full-import-v1/readback.json')
assert readback['passed'] and load(WORK/'arm-rest-b2-cooked-readback-v1/validation.json')['passed']
protected=dict(readback['protected_hashes'])
protected.update({p:info['sha256'] for p,info in readback['saved_assets'].items()})
for p in (mesh_package,baseline_package,rig_package,
          '/Game/CSS/CSS_SeduXtress_eins0fx_P/CR_SeduXtress_HairV37',
          '/Game/CSS/CSS_SeduXtress_eins0fx_P/CR_SeduXtress_BodyV42'):
    protected[str(asset_path(p))]=digest(asset_path(p))
hand_names=[m['bone'] for m in load(HERE/'left-finger-calibration-v1.json')['parameters']]
curves=[m['shape'] for m in load(HERE/'left-finger-correctives-v1.json')['parameters']]
mesh,baseline,bp=[unreal.load_asset(p) for p in (mesh_package,baseline_package,rig_package)]
assert mesh and baseline and bp
if unreal.EditorAssetLibrary.does_asset_exist(package):
    previous=load(WORK/'hand-postprocess-integration-v1/asset.json')
    assert previous['package']==package and digest(asset_path(package))==previous['sha256']
    candidate=unreal.load_asset(package)
else:
    candidate=unreal.CSSControlRigLibrary.create_hand_post_process_candidate(mesh,baseline,bp,package,hand_names,curves)
    assert candidate
asset=dict(package=package,path=str(asset_path(package)),sha256=digest(asset_path(package)))
(OUT/'asset.json').write_text(json.dumps(asset,indent=2)+'\n')
fixture=load(WORK/'hand-combined-fixtures-v1/fixtures.json')['cases']
metadata_fix=os.environ.get('CSS_HAND_METADATA')=='1'
if metadata_fix:
    control=next(c for c in fixture if c['label']=='calibration-h2-0')
    control_doc=load(Path(control['source']))
    raw=unreal.CSSControlRigLibrary.evaluate_hand_post_process_candidate(mesh,candidate,
        json.dumps(control_doc['pose']['Snapshot']),curves,False)
    assert raw
    (OUT/'metadata-negative-control.json').write_text(raw+'\n')
    negative=json.loads(raw)
    assert all(not f['mesh_morph'] and not f['skeleton_morph'] for f in negative['curve_metadata'].values())
    assert max(abs(s['curves'][n]-s['morph_weights'][n]) for s in negative['samples'] for n in curves)>.1
    fixed_package='/Game/CSSAuthoring/DiagnosticReferences/SK_ArmRestV44B2MorphMetadata_V1'
    assert not unreal.EditorAssetLibrary.does_asset_exist(fixed_package)
    fixed=unreal.CSSControlRigLibrary.create_morph_metadata_candidate(mesh,fixed_package)
    assert fixed and fixed.get_editor_property('skeleton')==mesh.get_editor_property('skeleton')
    assert unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(fixed)==unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh)
    assert [str(m.get_name()) for m in fixed.get_editor_property('morph_targets')]==[str(m.get_name()) for m in mesh.get_editor_property('morph_targets')]
    assert fixed.get_editor_property('materials')==mesh.get_editor_property('materials')
    mesh=fixed
    (OUT/'metadata-mesh.json').write_text(json.dumps(dict(package=fixed_package,path=str(asset_path(fixed_package)),
        sha256=digest(asset_path(fixed_package))),indent=2)+'\n')
selected=[next(c for c in fixture if c['label']==name) for name in (
    'calibration-source-reference-anchor','calibration-h2-0',
    'calibration-hand-overlay-observation-v2-pose-19.json')]
motion=load(WORK/'hand-native-motion-fixtures-v1/fixtures.json')['cases']
selected += [next(c for c in motion if c['label']==name) for name in (
    'running-5-0.625-alpha-1.0','running-87-0.5-alpha-0.0','running-95-0.0-alpha-1.0')]
bones=load(WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json')
names=[b['name'] for b in bones];changed=set(hand_names)

def angle(a,b):
    dot=abs(sum(x*y for x,y in zip(a,b)))/math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
    return math.degrees(2*math.acos(min(1,dot)))

results=[]
for case in selected:
    path=Path(case['source']);assert digest(path)==case['source_sha256']
    raw=unreal.CSSControlRigLibrary.evaluate_hand_post_process_candidate(mesh,candidate,
        json.dumps(load(path)['pose']['Snapshot']),curves,case['v43_compatible'])
    assert raw,case['label']
    (OUT/(case['label']+'.component.json')).write_text(raw+'\n')
    measured=json.loads(raw);rig=bp.create_control_rig();h=rig.get_hierarchy()
    if metadata_fix:assert all(f['mesh_morph'] and not f['skeleton_morph'] for f in measured['curve_metadata'].values())
    for row in measured['samples']:
        h.reset_pose_to_initial(unreal.RigElementType.BONE)
        for name,value in zip(names,row['upstream'],strict=True):
            pose=h.get_local_transform(key(name))
            pose.translation=unreal.Vector(*value['translation'])
            pose.rotation=unreal.Quat(*value['rotation'])
            pose.scale3d=unreal.Vector(*value['scale'])
            h.set_local_transform(key(name),pose,False,True)
        set_value(rig,'Enabled',row['enabled']);set_value(rig,'InputIsV43Compatible',case['v43_compatible'])
        assert rig.execute('Forwards Solve')
        expected_valid=rig.get_variable_as_string('HandValid').lower()=='true'
        assert expected_valid==row['valid'],(case['label'],row['frame'],'valid')
        maximum_rotation=0.;maximum_position=0.;maximum_scale=0.
        for name,input_pose,output in zip(names,row['upstream'],row['output'],strict=True):
            expected=transform(h.get_local_transform(key(name))) if name in changed else input_pose
            rotation=angle(output['rotation'],expected['rotation'])
            position=max(abs(a-b) for a,b in zip(output['translation'],expected['translation']))
            scale=max(abs(a-b) for a,b in zip(output['scale'],expected['scale']))
            maximum_rotation=max(maximum_rotation,rotation);maximum_position=max(maximum_position,position);maximum_scale=max(maximum_scale,scale)
            assert rotation<.001 and position<.0001 and scale<.00001,(case['label'],row['frame'],name,rotation,position,scale)
        expected_curves={n:h.get_curve_value(unreal.RigElementKey(name=n,type=unreal.RigElementType.CURVE)) for n in curves}
        curve_error=max(abs(row['curves'][n]-v) for n,v in expected_curves.items())
        rig_curve_error=max(abs(row['rig_curves'][n]-v) for n,v in expected_curves.items())
        weight_error=max(abs(row['morph_weights'][n]-v) for n,v in expected_curves.items())
        assert curve_error<.00001 and rig_curve_error<.00001 and weight_error<.0001,(case['label'],row['frame'],curve_error,rig_curve_error,weight_error)
        results.append(dict(label=case['label'],frame=row['frame'],enabled=row['enabled'],
            maximum_rotation_degrees=maximum_rotation,maximum_translation_cm=maximum_position,maximum_scale_error=maximum_scale,
            curve_error=curve_error,rig_curve_error=rig_curve_error,morph_weight_error=weight_error))
    unreal.log('CSS_HAND_COMPONENT_CASE_PASSED '+case['label'])
assert len(results)==42 and all(digest(Path(p))==v for p,v in protected.items())
assert digest(asset_path(package))==asset['sha256']
(OUT/'report.json').write_text(json.dumps(dict(passed=True,asset=asset,cases=results,protected_hashes=protected,metadata_fix=metadata_fix,
    sources={c['source']:c['source_sha256'] for c in selected},
    scope='Actual compressed-source component post-process, native hand oracle, accepted hair/body coexistence, curves and render morph weights. Static poses, not full game animation, cooked execution, visual weapon or performance acceptance.'),indent=2)+'\n')
unreal.log('CSS_HAND_POSTPROCESS_PASSED')
