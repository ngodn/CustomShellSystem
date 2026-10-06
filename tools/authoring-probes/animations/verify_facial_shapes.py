"""UE 5.6.1: read back facial candidates in a fresh editor process without saving."""
import hashlib
import json
import os
import re
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[4]
WORK=Path(os.environ['CSS_ANIM_WORK']).resolve()
if not WORK.is_relative_to(ROOT/'CustomShellSystem/work/cine-animation-port'):
    raise ValueError('Use the animation work directory')
CONTENT=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
sources=['/Game/CSS/SeduXtress/SK_BlackPearl2',
         *[f'/Game/CSS/EveTest/SK_{name}' for name in ['PFit14','SFit16','BFit1','KCloth4','ACloth3']]]
input_path=WORK/'facial-shapes.json'
revision=os.environ.get('CSS_FACE_REVISION', '')
if revision and not re.fullmatch(r'[A-Za-z0-9]{1,24}', revision):
    raise ValueError('Invalid facial candidate revision')
prefix='/Game/CSS/AnimLab/SK_Face_' + (revision+'_' if revision else '')
expression_names={shape['name'] for shape in json.loads(input_path.read_text())['morph_targets']}
records=[]
protected={str(input_path):hashlib.sha256(input_path.read_bytes()).hexdigest()}
(WORK/'ue-verification.json').write_text(json.dumps(dict(complete=False,results=[]),indent=2)+'\n')
for source_path in sources:
    target_path=prefix+source_path.rsplit('/',1)[-1].removeprefix('SK_')
    for package in (source_path,target_path):
        file=CONTENT/(package.removeprefix('/Game/')+'.uasset')
        protected[str(file)]=hashlib.sha256(file.read_bytes()).hexdigest()
    source=unreal.load_asset(source_path)
    target=unreal.load_asset(target_path)
    if not source or not target:
        raise ValueError('Missing comparison asset')
    if not records:
        if not unreal.CSSFacialLibrary.add_expression_shapes(source,str(input_path)).startswith('ERROR:'):
            raise ValueError('Importer accepted a protected source')
        if not unreal.CSSFacialLibrary.add_expression_shapes(target,str(input_path)).startswith('ERROR:'):
            raise ValueError('Importer accepted duplicate expression shapes')
    result=unreal.CSSFacialLibrary.verify_expression_shapes(source,target,str(input_path))
    if not result.startswith('PASS:'):
        raise ValueError(f'{target_path}: {result}')
    preservation=unreal.CSSFacialLibrary.verify_expression_preservation(source,target)
    if not preservation.startswith('PASS:'):
        raise ValueError(f'{target_path}: {preservation}')
    before=json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(source))
    if before!=json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(target)):
        raise ValueError('Bone binding changed')
    bindings=['skeleton','physics_asset','shadow_physics_asset','post_process_anim_blueprint','materials']
    if any(source.get_editor_property(name)!=target.get_editor_property(name) for name in bindings):
        raise ValueError('Material, physics or animation binding changed')
    old={str(m.get_name()) for m in source.get_editor_property('morph_targets')}
    actual={str(m.get_name()) for m in target.get_editor_property('morph_targets')}
    if actual!=old|expression_names:
        raise ValueError('Morph names changed')
    records.append(dict(source=source_path,candidate=target_path,result=result,preservation=preservation,
                        preserved_morphs=len(old),total_morphs=len(actual)))
    (WORK/'ue-verification.json').write_text(json.dumps(dict(
        results=records,complete=False),indent=2)+'\n')
    print('FACIAL_READBACK_PASS',target_path,flush=True)
if any(hashlib.sha256(Path(path).read_bytes()).hexdigest()!=sha for path,sha in protected.items()):
    raise ValueError('Readback modified an asset')
(WORK/'ue-verification.json').write_text(json.dumps(dict(
    complete=True,results=records,protected=protected,installed=False,
    scope='Fresh-process editor geometry, morphs, built sections, persistent cloth properties and bindings. Cooked/live playback unverified.'),indent=2)+'\n')
