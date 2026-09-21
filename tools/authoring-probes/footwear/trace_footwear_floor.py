"""Trace beneath replayed shoe points using the verified scene-query profile. Python 3.14.

No input, collision setting or actor transform is changed. Hit positions are
reconstructed from trace Time because reflected net-quantized vectors decode
as empty objects in the current bridge. Distance supplies an independent check.
"""
import argparse
import json
import math
import sys
from pathlib import Path
CSS=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(CSS/'tools'))
from css_live_snapshot import Probe
from check_inventory_camera import checked_get,checked_call
w=CSS/'work/grip-grounding-v1'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pose',type=Path,default=w/'heel-support-pose-v2/pose.json')
parser.add_argument('--live',type=Path,default=w/'footwear-followup-v1/live-floor-v2.json')
parser.add_argument('--output',type=Path,default=w/'footwear-followup-v1/floor-traces-v3.json')
args=parser.parse_args()
assert all(path.resolve().is_relative_to(CSS/'work') for path in (args.pose,args.live,args.output))
pose=json.loads(args.pose.read_text())
live=json.loads(args.live.read_text())
assert pose['player']==live['player'] and pose['mesh_transform']==live['mesh_transform_before']
out=args.output;assert not out.exists()
report=dict(scope='Four local scene traces in one stationary pose, not all terrain or locomotion acceptance.',rows=[],failures=[])
with out.with_suffix('.requests.jsonl').open('x') as f:
 p=Probe(f);player=p.send('player');assert player==pose['player'];mesh=checked_get(p,player['pawn'],'Mesh')
 before=checked_call(p,mesh,'K2_GetComponentToWorld')['ReturnValue']
 assert before==pose['mesh_transform'], 'Character moved since the captured pose'
 mesh_profile=checked_call(p,mesh,'GetCollisionProfileName')['ReturnValue'];assert mesh_profile==live['collision_profile']
 config=p.send('find',path='/Script/Engine.Default__CollisionProfile')
 profiles=checked_get(p,config,'Profiles');profile='BlockAll'
 assert sum(row['Name']==profile for row in profiles)==1
 target=p.send('find',path='/Script/Engine.Default__KismetSystemLibrary');assert target
 signature=p.send('describe',target=target,function='LineTraceSingleByProfile')
 expected={'WorldContextObject':8,'Start':24,'End':24,'ProfileName':8,'bTraceComplex':1,'ActorsToIgnore':16,'DrawDebugType':1,'OutHit':None,'bIgnoreSelf':1,'TraceColor':16,'TraceHitColor':16,'DrawTime':4,'ReturnValue':1}
 assert set(signature)==set(expected)
 for n,size in expected.items():
  if size is not None:assert signature[n]['size']==size,(n,signature[n])
 report.update(signature=signature,profile=profile,player=player)
 for side,points in pose['rows'].items():
  for kind in ('shoe','support'):
   point=dict(zip('XYZ',points[kind+'_low_world_cm']));start=dict(point,Z=point['Z']+20);end=dict(point,Z=point['Z']-50)
   hit=p.send('call',target=target,function='LineTraceSingleByProfile',args=dict(WorldContextObject=player['pawn'],Start=start,End=end,ProfileName=profile,bTraceComplex=True,DrawDebugType=0,bIgnoreSelf=True,TraceColor=dict(R=0,G=0,B=0,A=0),TraceHitColor=dict(R=0,G=0,B=0,A=0),DrawTime=0))
   row=dict(side=side,kind=kind,point=point,start=start,end=end,result=hit)
   h=hit['OutHit']
   if not hit['ReturnValue'] or not h['bBlockingHit'] or h['bStartPenetrating']:report['failures'].append(side+kind+': invalid floor hit')
   else:
    assert math.isfinite(h['Time']) and 0<=h['Time']<=1
    z=start['Z']-70*h['Time'];assert abs(h['Distance']-70*h['Time'])<.01
    row.update(hit_z_cm=z,clearance_cm=point['Z']-z)
   report['rows'].append(row)
 after=checked_call(p,mesh,'K2_GetComponentToWorld')['ReturnValue'];report.update(before=before,after=after)
 if after!=before or p.send('player')!=player:report['failures'].append('Character changed during traces')
 out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(rows=[{k:r[k] for k in ('side','kind','clearance_cm') if k in r} for r in report['rows']],failures=report['failures'])))
 raise SystemExit(bool(report['failures']))
