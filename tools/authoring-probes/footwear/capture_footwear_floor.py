"""Capture a stationary accepted-rig foot pose and movement floor state. Python 3.14."""
import argparse
import json
import sys
import time
from pathlib import Path
CSS=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(CSS/'tools'))
from css import processes
from css_live_snapshot import Probe
from check_inventory_camera import checked_get,checked_call
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--profile',choices=('v44','short1'),default='v44')
args=parser.parse_args();out=args.output.resolve()
assert out.is_relative_to(CSS/'work') and not out.exists() and len(processes())==1
with out.with_suffix('.requests.jsonl').open('x') as f:
 p=Probe(f);player=p.send('player');assert player.get('pawn');pawn=player['pawn']
 mesh=checked_get(p,pawn,'Mesh');move=checked_get(p,pawn,'CharacterMovement');cap=checked_get(p,pawn,'CapsuleComponent')
 data=dict(player=player,mesh=mesh,movement=move,capsule=cap,started_ns=time.time_ns())
 data['asset']=checked_get(p,mesh,'SkeletalMesh')
 expected={'v44':'/Game/CSSAuthoring/DiagnosticReferences/SK_B2PhysicsBound_V1.SK_B2PhysicsBound_V1',
           'short1':'/Game/CSS/SeduXtress/SK_BlackPearl2.SK_BlackPearl2'}[args.profile]
 assert data['asset']['name']=='SkeletalMesh '+expected
 data['profile']=args.profile
 data['movement_values']={n:checked_get(p,move,n) for n in ('CurrentFloor','MovementMode','Velocity')}
 data['capsule_values']={n:checked_get(p,cap,n) for n in ('CapsuleHalfHeight','CapsuleRadius','RelativeScale3D')}
 data['capsule_transform']=checked_call(p,cap,'K2_GetComponentToWorld')['ReturnValue']
 data['collision_profile']=checked_call(p,mesh,'GetCollisionProfileName')['ReturnValue']
 data['morphs']={n:checked_call(p,mesh,'GetMorphTarget',MorphTargetName=n)['ReturnValue'] for n in ('FBMBodyTone','PBMBreastsSize','PBMGlutesSize','PBMHipSize','PBMThighsTone','PBMWaistWidth')}
 data['mesh_transform_before']=checked_call(p,mesh,'K2_GetComponentToWorld')['ReturnValue']
 data['pose']=checked_call(p,mesh,'SnapshotPose')
 data['mesh_transform_after']=checked_call(p,mesh,'K2_GetComponentToWorld')['ReturnValue']
 data['floor_after']=checked_get(p,move,'CurrentFloor')
 data['velocity_after']=checked_get(p,move,'Velocity')
 data['player_after']=p.send('player');assert data['player_after']==player
 data['ended_ns']=time.time_ns();out.write_text(json.dumps(data,indent=2)+'\n')
 assert data['mesh_transform_before']==data['mesh_transform_after']
 assert data['movement_values']['Velocity']==dict(X=0,Y=0,Z=0)
 assert data['velocity_after']==dict(X=0,Y=0,Z=0)
 print(json.dumps(dict(output=str(out),morphs=data['morphs'],floor=data['floor_after']['FloorDist'])))
