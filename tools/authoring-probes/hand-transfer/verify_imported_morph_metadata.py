"""Fresh-process regression for animation-driven morphs from the mesh importer."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=WORK/'hand-morph-import-regression-v1'
HERE=Path(__file__).resolve().parent
CONTENT=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
asset_path=lambda p:CONTENT/(p.removeprefix('/Game/')+'.uasset')
assert not (OUT/'report.json').exists()
assert load(OUT/'import.exit.json')['exit_code']==0
manifest=load(OUT/'input.json')
assert digest(Path(manifest['source']))==manifest['source_sha256']
assert digest(OUT/'input.mesh.json')==manifest['input_sha256']
original=load(Path(manifest['source']));source=load(OUT/'input.mesh.json')
assert all(v==source[k] for k,v in original.items() if k not in manifest['changed_fields'])
mesh=unreal.load_asset(source['mesh_package'])
baseline=unreal.load_asset('/Game/CSS/CSS_SeduXtress_eins0fx_P/ABP_SeduXtress_BodyHairV42')
hand=unreal.load_asset('/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCombinedV1')
assert mesh and baseline and hand
protected=load(WORK/'hand-postprocess-integration-v2/report.json')['protected_hashes']
assert all(digest(Path(p))==h for p,h in protected.items())
bind=load(WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json')
assert json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))==bind
assert sorted(str(m.get_name()) for m in mesh.get_editor_property('morph_targets'))==sorted(m['name'] for m in source['morph_targets'])
names=[p['bone'] for p in load(HERE/'left-finger-calibration-v1.json')['parameters']]
curves=[p['shape'] for p in load(HERE/'left-finger-correctives-v1.json')['parameters']]
package='/Game/CSS/TransientProbes/ABP_CSS_ControlRigProbeHandB2ImportedV1'
assert not unreal.EditorAssetLibrary.does_asset_exist(package)
candidate=unreal.CSSControlRigLibrary.create_hand_post_process_candidate(mesh,baseline,hand,package,names,curves)
assert candidate
fixture=next(c for c in load(WORK/'hand-combined-fixtures-v1/fixtures.json')['cases'] if c['label']=='calibration-h2-0')
path=Path(fixture['source']);assert digest(path)==fixture['source_sha256']
raw=unreal.CSSControlRigLibrary.evaluate_hand_post_process_candidate(mesh,candidate,json.dumps(load(path)['pose']['Snapshot']),curves,False)
assert raw
(OUT/'component.json').write_text(raw+'\n');report=json.loads(raw)
assert all(f['mesh_morph'] and not f['skeleton_morph'] for f in report['curve_metadata'].values())
errors=[];active=0
for sample in report['samples']:
    for name in curves:
        error=abs(sample['rig_curves'][name]-sample['morph_weights'][name])
        assert error<.0001,(sample['frame'],name,error)
        assert abs(sample['curves'][name]-sample['rig_curves'][name])<.00001
        if not sample['enabled']:assert sample['morph_weights'][name]==0
        if sample['enabled'] and abs(sample['morph_weights'][name])>.001:active+=1
        errors.append(error)
assert active>0 and len(report['samples'])==7
assert all(digest(Path(p))==h for p,h in protected.items())
packages=[source['mesh_package'],source['skeleton_package'],package,
    '/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCombinedV1']
(OUT/'report.json').write_text(json.dumps(dict(passed=True,frames=7,maximum_morph_weight_error=max(errors),
    nonzero_weight_checks=active,mesh_bind_unchanged=True,packages=packages,
    source_sha256=manifest['source_sha256'],assets={str(asset_path(p)):digest(asset_path(p)) for p in packages},
    protected_hashes=protected,scope='Fresh imported mesh metadata survives save/load and drives actual component morph weights. No cooked or live execution claim.'),indent=2)+'\n')
unreal.log('CSS_IMPORTED_MORPH_METADATA_PASSED')
