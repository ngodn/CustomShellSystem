"""UE 5.6.1: append performance controls while preserving the verified CW face."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
BASE = ROOT/'CustomShellSystem/work/cine-animation-port'
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
MODE = os.environ.get('CSS_FACE_TRIAL_MODE','create')
if not WORK.is_relative_to(BASE) or MODE not in ('create','verify'):
    raise ValueError('Use a private workspace and create/verify mode')
CONTENT = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
SOURCE = '/Game/CSS/Locomotion/Faces/SK_Face_CWCompact/SK_Face_CWE4'
TARGET = '/Game/CSS/AnimLab/SK_Face_CWCompact/SK_Face_CWE5'
INPUT = BASE/'cw-performance-fitted-v1/facial-shapes.json'
COMBINED = os.environ.get('CSS_FACE_COMBINED_BASIS')
if COMBINED:
    basis = Path(COMBINED).resolve(strict=True)
    if not basis.is_relative_to(BASE):
        raise ValueError('Combined basis must be inside the animation workspace')
    INPUT = basis/'facial-shapes.json'
    TARGET = '/Game/CSS/AnimLab/SK_Face_CWCompact/SK_Face_CWE7'


def disk(package):
    return CONTENT/(package.removeprefix('/Game/')+'.uasset')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


audit_path = BASE/'cw-expression-shared-v3/verify-result.json'
audit = json.loads(audit_path.read_text())
fitted_path = INPUT.with_name('report.json')
fitted = json.loads(fitted_path.read_text())
expected_added = fitted.get('added_shapes') if COMBINED else 59
fit_verified = fitted.get('passed') if COMBINED else fitted.get('complete')
if COMBINED and (not isinstance(expected_added, int) or not 1 <= expected_added <= 128 or
        fitted.get('total_shapes', 129) > 128 or fitted.get('fitted_correspondence_error_cm', 1) > .0002 or
        len(fitted.get('results', [])) != 5 or
        any(not row.get('passed') or max(row['training_error_cm'], row['max_holdout_error_cm']) > .015
            for row in fitted['results'])):
    raise ValueError('Require a bounded combined fit for all five performances')
if (not audit.get('complete') or audit['target'] != SOURCE or
        not fit_verified or fitted.get('added_shapes') != expected_added or
        digest(INPUT) != fitted['sha256'] or digest(disk(SOURCE)) != audit['target_sha256']):
    raise ValueError('Require the unchanged verified face and fitted controls')
for mode in ('create','verify'):
    if json.loads((audit_path.parent/(mode+'-exit.json')).read_text())['exit_code'] != 0:
        raise ValueError('Baseline editor job did not finish successfully')
protected = {str(p):digest(p) for p in (audit_path,fitted_path,INPUT,disk(SOURCE))}
report_path = WORK/(MODE+'-result.json')
if report_path.exists() or (MODE=='create' and disk(TARGET).exists()):
    raise FileExistsError('Output already exists')
source = unreal.load_asset(SOURCE)
if not isinstance(source,unreal.SkeletalMesh):
    raise ValueError('Missing verified CW face')
if MODE=='create':
    target = unreal.EditorAssetLibrary.duplicate_asset(SOURCE,TARGET)
else:
    target = unreal.load_asset(TARGET)
    protected[str(disk(TARGET))] = digest(disk(TARGET))
if not isinstance(target,unreal.SkeletalMesh):
    raise ValueError('Missing performance face')
before = {str(m.get_name()) for m in source.get_editor_property('morph_targets')}
payload = json.loads(INPUT.read_text())
added = {row['name'] for row in payload['morph_targets']}
if len(before)!=90 or len(added)!=expected_added or before & added:
    raise ValueError('Unexpected morph set')
report = dict(complete=False,source=SOURCE,target=TARGET,mode=MODE,installed=False)
report_path.write_text(json.dumps(report,indent=2)+'\n')
if MODE=='create':
    report['import'] = unreal.CSSFacialLibrary.add_expression_shapes(target,str(INPUT))
    if not report['import'].startswith('PASS:'):
        raise ValueError(report['import'])
for key,result in (
        ('comparison',unreal.CSSFacialLibrary.verify_expression_shapes(source,target,str(INPUT))),
        ('preservation',unreal.CSSFacialLibrary.verify_expression_preservation(source,target))):
    report[key] = result
    if not result.startswith('PASS:'):
        raise ValueError(result)
if unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(source)!=unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(target):
    raise ValueError('Bone binding changed')
for field in ('skeleton','physics_asset','shadow_physics_asset','post_process_anim_blueprint','materials'):
    if source.get_editor_property(field)!=target.get_editor_property(field):
        raise ValueError('Source binding changed: '+field)
slots = source.get_editor_property('materials')
if len(slots)!=70 or any(not s.material_interface for s in slots):
    raise ValueError('All 70 baseline material bindings must resolve')
actual = {str(m.get_name()) for m in target.get_editor_property('morph_targets')}
if actual != before|added:
    raise ValueError('Morph names changed')
if MODE=='create' and not unreal.EditorAssetLibrary.save_loaded_asset(target,False):
    raise ValueError('Could not save candidate')
if any(digest(Path(path))!=checksum for path,checksum in protected.items()):
    raise ValueError('Protected input changed')
report.update(complete=True,protected=protected,sources_unchanged=True,
    prior_morphs=len(before),added_morphs=len(added),total_morphs=len(actual),
    material_slots=len(slots),target_sha256=digest(disk(TARGET)),
    scope='Editor shape and preservation checks only. Cooked readback, performance mappings and live acceptance remain pending.')
report_path.write_text(json.dumps(report,indent=2)+'\n')
print('CW_PERFORMANCE_FACE_PASS',MODE,report['comparison'],flush=True)
