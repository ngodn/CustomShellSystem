"""UE 5.6.1: derive isolated facial candidates from the six current Eve meshes."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
CONTENT = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
if not WORK.is_relative_to(ROOT/'CustomShellSystem/work/cine-animation-port'):
    raise ValueError('Use the animation work directory')
INPUT = WORK/'facial-shapes.json'
sources = ['/Game/CSS/SeduXtress/SK_BlackPearl2',
           *[f'/Game/CSS/EveTest/SK_{name}' for name in ['PFit14','SFit16','BFit1','KCloth4','ACloth3']]]
selected=os.environ.get('CSS_FACE_SOURCE')
if selected:
    sources=[path for path in sources if path.rsplit('/',1)[-1]==selected]
    if not sources:
        raise ValueError('Unknown facial source')


def disk_path(package):
    return CONTENT/(package.removeprefix('/Game/')+'.uasset')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


protected = {str(disk_path(path)): digest(disk_path(path)) for path in sources}
results = []
report_path=WORK/('ue-import-'+(selected or 'all')+'.json')
report_path.write_text(json.dumps(dict(protected=protected,results=[],complete=False),indent=2)+'\n')
try:
    for source_path in sources:
        target_path = '/Game/CSS/AnimLab/SK_Face_'+source_path.rsplit('/',1)[-1].removeprefix('SK_')
        if unreal.EditorAssetLibrary.does_asset_exist(target_path):
            raise ValueError(f'Candidate already exists: {target_path}')
        source = unreal.load_asset(source_path)
        if not source:
            raise ValueError(f'Missing source: {source_path}')
        before = json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(source))
        fields = ['skeleton','physics_asset','shadow_physics_asset','post_process_anim_blueprint']
        bindings = {name: source.get_editor_property(name) for name in fields}
        original_morphs = {str(m.get_name()) for m in source.get_editor_property('morph_targets')}
        target = unreal.EditorAssetLibrary.duplicate_asset(source_path,target_path)
        if not target:
            raise ValueError('Mesh duplication failed')
        result = unreal.CSSFacialLibrary.add_expression_shapes(target,str(INPUT))
        if not result.startswith('PASS:'):
            raise ValueError(f'{target_path}: {result}')
        actual = {str(m.get_name()) for m in target.get_editor_property('morph_targets')}
        wanted = {entry['name'] for entry in json.loads(INPUT.read_text())['morph_targets']}
        if actual != original_morphs | wanted:
            raise ValueError('Existing or facial morph names lost')
        if before != json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(target)):
            raise ValueError('Mesh bone binding changed')
        if any(target.get_editor_property(name)!=obj for name,obj in bindings.items()):
            raise ValueError('Skeleton, physics or post-process binding changed')
        if target.get_editor_property('materials') != source.get_editor_property('materials'):
            raise ValueError('Material slots changed')
        if not unreal.EditorAssetLibrary.save_loaded_asset(target,False):
            raise ValueError('Saving facial candidate failed')
        results.append(dict(source=source_path,candidate=target_path,result=result,
                            prior_morphs=len(original_morphs),total_morphs=len(actual),
                            sha256=digest(disk_path(target_path))))
        report_path.write_text(json.dumps(dict(protected=protected,results=results,complete=False),indent=2)+'\n')
        print('FACIAL_IMPORT_PASS',target_path,flush=True)
finally:
    unchanged = all(digest(Path(path))==sha for path,sha in protected.items())
    report_path.write_text(json.dumps(dict(
        results=results,protected=protected,sources_unchanged=unchanged,installed=False,
        complete=len(results)==len(sources)),indent=2)+'\n')
    if not unchanged:
        raise ValueError('A protected source changed')
