"""UE 5.6.1: strict schema guards plus saved legacy/sparse facial readback."""
import copy
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT=Path(__file__).resolve().parents[4]
WORK=Path(os.environ['CSS_ANIM_WORK']).resolve()
if not WORK.is_relative_to(ROOT/'CustomShellSystem/work/cine-animation-port'):
    raise ValueError('Use the private animation workspace')
content=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
cases=[('/Game/CSS/CommanderWhite/Dev/SK_CommanderC4','/Game/CSS/AnimLab/SK_Face_CWJ1',
        WORK.parent/'cw-facial-bake-v2/facial-shapes.json'),
       ('/Game/CSS/SeduXtress/SK_BlackPearl2','/Game/CSS/AnimLab/SK_Face_Performance_BlackPearl2',
        WORK.parent/'facial-performance-export-v2/facial-shapes.json')]
protected={}
results=[]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


for source_path,target_path,input_path in cases:
    for package in (source_path,target_path):
        file=content/(package.removeprefix('/Game/')+'.uasset')
        protected[str(file)]=digest(file)
    protected[str(input_path)]=digest(input_path)
    source=unreal.load_asset(source_path);target=unreal.load_asset(target_path)
    if not source or not target:
        raise ValueError('Missing fixture')
    valid=json.loads(input_path.read_text())
    negatives=[]
    for field,value in [('schema',1.5),('schema',2.5),('schema',str(valid['schema'])),
                        ('schema',True),('schema',None),('diagnostic_only','false'),
                        ('diagnostic_only',0),('diagnostic_only',None),('diagnostic_only',True)]:
        bad=copy.deepcopy(valid);bad[field]=value
        filename=f'{valid["schema"]}-{field}-{value}.json';path=WORK/filename
        path.write_text(json.dumps(bad,separators=(',',':'))+'\n')
        result=unreal.CSSFacialLibrary.add_expression_shapes(target,str(path))
        if not result.startswith('ERROR: invalid facial schema, counts or point correspondence'):
            raise ValueError('Wrong validation stage: '+result)
        negatives.append(dict(field=field,value=value,result=result))
    result=unreal.CSSFacialLibrary.verify_expression_shapes(source,target,str(input_path))
    if not result.startswith('PASS:'):
        raise ValueError(result)
    preservation=unreal.CSSFacialLibrary.verify_expression_preservation(source,target)
    if not preservation.startswith('PASS:'):
        raise ValueError(preservation)
    results.append(dict(schema=valid['schema'],source=source_path,target=target_path,
                        result=result,preservation=preservation,negative_cases=negatives))
if any(digest(Path(path))!=sha for path,sha in protected.items()):
    raise ValueError('Readback changed a source')
(WORK/'result.json').write_text(json.dumps(dict(complete=True,results=results,protected=protected,
    scope='Saved schema-1/schema-2 readback and strict parser rejection. No gameplay/cook acceptance.'),indent=2)+'\n')
print('FACIAL_SCHEMA_VERIFY_PASS',len(results),flush=True)
