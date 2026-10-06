"""UE 5.6.1: import/read back the isolated CW jaw and blink trial."""
import copy
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
if not WORK.is_relative_to(ROOT/'CustomShellSystem/work/cine-animation-port'):
    raise ValueError('Use the private animation workspace')
MODE = os.environ.get('CSS_FACE_TRIAL_MODE', 'create')
if MODE not in ('create', 'verify'):
    raise ValueError('Unknown trial mode')
SOURCE = '/Game/CSS/CommanderWhite/Dev/SK_CommanderC4'
REVISION = os.environ.get('CSS_FACE_TRIAL_REVISION', 'J1')
FIXTURES = {'J1': 'cw-facial-bake-v2', 'E1': 'cw-expression-bake-v1',
            'E2': 'cw-expression-compact-v1', 'E3': 'cw-expression-compact-v2'}
if REVISION not in FIXTURES:
    raise ValueError('Unknown CW facial trial revision')
TARGET = '/Game/CSS/AnimLab/SK_Face_CW'+REVISION
if REVISION in ('E2','E3'):
    TARGET='/Game/CSS/AnimLab/SK_Face_CWCompact/SK_Face_CW'+REVISION
INPUT = WORK.parent/FIXTURES[REVISION]/'facial-shapes.json'
CONTENT = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'


def disk(package):
    return CONTENT/(package.removeprefix('/Game/')+'.uasset')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


protected = {str(p): digest(p) for p in (disk(SOURCE),INPUT)}
report_path=WORK/(MODE+'-result.json')
if report_path.exists():
    raise FileExistsError(report_path)
report=dict(complete=False, mode=MODE, source=SOURCE, target=TARGET, negative_cases=[], installed=False)
report_path.write_text(json.dumps(report,indent=2)+'\n')
source=unreal.load_asset(SOURCE)
if not source:
    raise ValueError('Missing source')
if MODE=='create':
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        raise FileExistsError(TARGET)
    target=unreal.EditorAssetLibrary.duplicate_asset(SOURCE,TARGET)
else:
    target=unreal.load_asset(TARGET)
    protected[str(disk(TARGET))]=digest(disk(TARGET))
if not target:
    raise ValueError('Missing trial mesh')
payload=json.loads(INPUT.read_text())
before={str(m.get_name()) for m in source.get_editor_property('morph_targets')}
if MODE=='create':
    invalid=[]
    bad=copy.deepcopy(payload);bad['point_indices'][1]=bad['point_indices'][0];invalid.append(('duplicate-point',bad))
    bad=copy.deepcopy(payload);bad['point_indices'][0]=-1;invalid.append(('negative-point',bad))
    bad=copy.deepcopy(payload);bad['point_indices'][0]=.5;invalid.append(('fractional-point',bad))
    bad=copy.deepcopy(payload);bad['point_indices'][0]=2000001;invalid.append(('oversized-point',bad))
    bad=copy.deepcopy(payload);bad['point_indices'].pop();invalid.append(('point-count',bad))
    bad=copy.deepcopy(payload);bad['body_points'][0][0]+=1;invalid.append(('wrong-fit',bad))
    bad=copy.deepcopy(payload);bad['diagnostic_only']=True;invalid.append(('diagnostic-only',bad))
    bad=copy.deepcopy(payload);bad['schema']=3;invalid.append(('unknown-schema',bad))
    bad=copy.deepcopy(payload);bad['morph_targets'][0]['deltas'][0][0]=len(payload['body_points']);invalid.append(('delta-out-of-range',bad))
    for name,bad in invalid:
        path=WORK/(name+'.json');path.write_text(json.dumps(bad,separators=(',',':'))+'\n')
        result=unreal.CSSFacialLibrary.add_expression_shapes(target,str(path))
        if not result.startswith('ERROR:') or {str(m.get_name()) for m in target.get_editor_property('morph_targets')}!=before:
            raise ValueError('Invalid input accepted or changed mesh: '+name)
        report['negative_cases'].append(dict(case=name,result=result))
    result=unreal.CSSFacialLibrary.add_expression_shapes(source,str(INPUT))
    if not result.startswith('ERROR:'):
        raise ValueError('Importer accepted original source')
    report['import']=unreal.CSSFacialLibrary.add_expression_shapes(target,str(INPUT))
    if not report['import'].startswith('PASS:'):
        report_path.write_text(json.dumps(report,indent=2)+'\n')
        raise ValueError(report['import'])
report['comparison']=unreal.CSSFacialLibrary.verify_expression_shapes(source,target,str(INPUT))
if not report['comparison'].startswith('PASS:'):
    raise ValueError(report['comparison'])
report['preservation']=unreal.CSSFacialLibrary.verify_expression_preservation(source,target)
if not report['preservation'].startswith('PASS:'):
    raise ValueError(report['preservation'])
if unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(source)!=unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(target):
    raise ValueError('Mesh bone binding changed')
for field in ('skeleton','physics_asset','shadow_physics_asset','post_process_anim_blueprint','materials'):
    if source.get_editor_property(field)!=target.get_editor_property(field):
        raise ValueError('Source binding changed: '+field)
actual={str(m.get_name()) for m in target.get_editor_property('morph_targets')}
if actual!=before|{s['name'] for s in payload['morph_targets']}:
    raise ValueError('Morph set changed')
if MODE=='create' and not unreal.EditorAssetLibrary.save_loaded_asset(target,False):
    raise ValueError('Could not save trial')
if any(digest(Path(path))!=sha for path,sha in protected.items()):
    raise ValueError('Protected source changed')
report.update(complete=True,protected=protected,sources_unchanged=True,prior_morphs=len(before),
              total_morphs=len(actual),target_sha256=digest(disk(TARGET)),
              scope='Editor morphs, base description, built sections, persistent cloth properties and bindings; Windows cook and runtime remain pending.')
report_path.write_text(json.dumps(report,indent=2)+'\n')
print('CW_FACE_TRIAL_PASS',MODE,report['comparison'],flush=True)
