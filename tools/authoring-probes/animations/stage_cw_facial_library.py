"""UE 5.6.1: stage the verified compact Commander White expression mesh."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT=Path(__file__).resolve().parents[4]
BASE=ROOT/'CustomShellSystem/work/cine-animation-port'
WORK=Path(os.environ['CSS_ANIM_WORK']).resolve()
MODE=os.environ.get('CSS_FACE_STAGE_MODE','create')
if not WORK.is_relative_to(BASE) or MODE not in ('create','verify'):
    raise ValueError('Use a private workspace and create/verify mode')
INPUT=BASE/'cw-expression-compact-v2/facial-shapes.json'
AUDIT=BASE/'cw-expression-native-v4/verify-result.json'
SOURCE='/Game/CSS/CommanderWhite/Dev/SK_CommanderC4'
CANDIDATE='/Game/CSS/AnimLab/SK_Face_CWCompact/SK_Face_CWE3'
TARGET='/Game/CSS/Locomotion/Faces/SK_Face_CWCompact/SK_Face_CWE3'
CONTENT=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'


def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def disk(package):return CONTENT/(package.removeprefix('/Game/')+'.uasset')


audit=json.loads(AUDIT.read_text())
if (not audit.get('complete') or audit.get('target')!=CANDIDATE or
        json.loads((AUDIT.parent/'verify-exit.json').read_text())['exit_code']!=0 or
        json.loads((AUDIT.parent/'create-exit.json').read_text())['exit_code']!=0):
    raise ValueError('Fresh native verification and successful creation are required')
protected=dict(audit['protected'])
protected.update({str(AUDIT):digest(AUDIT),str(INPUT):digest(INPUT),
                  str(disk(CANDIDATE)):audit['target_sha256']})
if any(digest(Path(p))!=sha for p,sha in protected.items()):
    raise ValueError('Verified input changed')
report=WORK/(MODE+'-result.json')
if report.exists():raise FileExistsError(report)
if MODE=='create' and disk(TARGET).exists():raise FileExistsError(TARGET)
if MODE=='verify':protected[str(disk(TARGET))]=digest(disk(TARGET))
source=unreal.load_asset(SOURCE);candidate=unreal.load_asset(CANDIDATE)
target=(unreal.EditorAssetLibrary.duplicate_asset(CANDIDATE,TARGET)
        if MODE=='create' else unreal.load_asset(TARGET))
if not source or not candidate or not isinstance(target,unreal.SkeletalMesh):
    raise ValueError('Missing facial staging asset')
comparisons=[]
for original in (source,candidate):
    check=unreal.CSSFacialLibrary.verify_expression_shapes(original,target,str(INPUT))
    if not check.startswith('PASS:'):raise ValueError(check)
    comparisons.append(check)
    if unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(original)!=unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(target):
        raise ValueError('Facial staging changed bone binding')
    for field in ('skeleton','physics_asset','shadow_physics_asset','post_process_anim_blueprint','materials'):
        if original.get_editor_property(field)!=target.get_editor_property(field):
            raise ValueError('Facial staging changed '+field)
actual={str(m.get_name()) for m in target.get_editor_property('morph_targets')}
expected={str(m.get_name()) for m in candidate.get_editor_property('morph_targets')}
if actual!=expected or len(actual)!=90:raise ValueError('Facial staging morph set differs')
if MODE=='create' and not unreal.EditorAssetLibrary.save_loaded_asset(target,False):
    raise ValueError('Could not save staged face')
if any(digest(Path(p))!=sha for p,sha in protected.items()):
    raise ValueError('Staging changed a protected input')
mapping=unreal.CSSFacialLibrary.expression_vertex_map(target)
if mapping.startswith('ERROR:'):raise ValueError(mapping)
vertex_map=json.loads(mapping)
(WORK/(MODE+'-vertex-map.json')).write_text(json.dumps(vertex_map,separators=(',',':'))+'\n')
report.write_text(json.dumps(dict(complete=True,source=SOURCE,candidate=CANDIDATE,target=TARGET,
    comparisons=comparisons,total_morphs=len(actual),protected=protected,
    target_sha256=digest(disk(TARGET)),installed=False,
    scope='Shared namespace staging only. Independent cook/readback and gameplay pending.'),indent=2)+'\n')
print('CW_SHARED_FACE_PASS',MODE,flush=True)
