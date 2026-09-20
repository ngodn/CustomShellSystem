"""Read current collision asset references without changing character state."""
import argparse,json,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
OUT=args.output.resolve()
assert OUT.parent==ROOT/'work/grip-grounding-v1' and not OUT.exists()
OUT.mkdir()
sys.path.insert(0,str(ROOT/'tools'))
from css import GAME,atomic,wait_json
MOD=GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
def ask(request):
 rid=str(time.time_ns())
 with (OUT/'requests.jsonl').open('a') as log:log.write(json.dumps(dict(id=rid,request=request))+'\n')
 atomic(MOD/'request.json',dict(id=rid,action='css_probe',request=request))
 response=wait_json(MOD/'runtime/css-probe.json',lambda r:r.get('id')==rid,timeout=20)
 with (OUT/'requests.jsonl').open('a') as log:log.write(json.dumps(response)+'\n')
 assert response['ok'],response
 return response['result']
def get(target,prop):return ask(dict(op='get',target=target,property=prop))
def properties(target):
 rows=ask(dict(op='properties',target=target,inherited=True))
 return {r['name'] for r in rows}
player=ask(dict(op='player'));pawn=player['pawn'];assert pawn
mesh=get(pawn,'Mesh');names=properties(mesh)
refs={n:get(mesh,n) for n in ['PhysicsAssetOverride','SkeletalMesh','SkinnedAsset','SkeletalMeshAsset'] if n in names}
meshes={};seen={}
for n,obj in refs.items():
 if not obj or not isinstance(obj,dict) or 'SkeletalMesh ' not in obj.get('name',''):continue
 if obj['$object'] in seen:
  meshes[n]=dict(meshes[seen[obj['$object']]]);continue
 seen[obj['$object']]=n
 fields=properties(obj);meshes[n]=dict(object=obj)
 for prop in ('PhysicsAsset','ShadowPhysicsAsset','Skeleton'):
  if prop in fields:meshes[n][prop]=get(obj,prop)
end=ask(dict(op='player'));assert end['pawn']==pawn and end['revision']==player['revision']
report=dict(player=player,mesh=mesh,component_references=refs,mesh_assets=meshes,bCanBeDamaged=get(pawn,'bCanBeDamaged'),scope='Fresh reflected reference reads only; no collision mutation or hit/parry test.')
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
