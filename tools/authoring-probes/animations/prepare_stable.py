"""Prepare source-matched two-bone leg trajectories for the fitted Eve rig."""
import json,sys,copy,math
from pathlib import Path
import numpy as np
sys.path.insert(0,'tools/authoring-probes/animations')
from review_source_tracks import rotation
ROOT=Path(__file__).resolve().parents[3]
os=__import__('os'); os.chdir(ROOT)
W=Path('work/anim14');B=Path('work/anim10/batch'); bind=json.loads((B/'target-bind.json').read_text());names=[b['name'] for b in bind]
sbind=json.loads(Path('work/anim4/source-bind.json').read_text());snames=[b['name'] for b in sbind]
source_paths={k:Path('work/anim9/batch')/(v+'.json') for k,v in json.loads(Path('work/anim9/batch/batch.json').read_text())}
source_paths.update({k:Path('work/anim4')/('Proto_'+v+'.json') for k,v in [('Walk','Walk'),('Jog','Run'),('Sprint','Sprint')]})
phase=json.loads(Path('work/anim12/phase.json').read_text())['clips']
def matrix(t):
 m=np.eye(4);m[:3,:3]=rotation(t['rotation']);m[:3,3]=t['translation'];return m
def worlds(locals,b):
 w=[]
 for m,x in zip(locals,b):w.append(w[x['parent']]@m if x['parent']>=0 else m)
 return w
trest=worlds([matrix(b) for b in bind],bind);srest=worlds([matrix(b) for b in sbind],sbind)
def swing(a,b):
 a=a/np.linalg.norm(a);b=b/np.linalg.norm(b);v=np.cross(a,b);c=float(np.dot(a,b));assert c>-.999
 k=np.array([[0,-v[2],v[1]],[v[2],0,-v[0]],[-v[1],v[0],0]])
 return np.eye(3)+k+k@k/(1+c)
def quat(m):
 # Eigenvector form avoids singular branches around half turns.
 k=np.array([[m[0,0]-m[1,1]-m[2,2],m[0,1]+m[1,0],m[0,2]+m[2,0],m[2,1]-m[1,2]],
 [m[0,1]+m[1,0],m[1,1]-m[0,0]-m[2,2],m[1,2]+m[2,1],m[0,2]-m[2,0]],
 [m[0,2]+m[2,0],m[1,2]+m[2,1],m[2,2]-m[0,0]-m[1,1],m[1,0]-m[0,1]],
 [m[2,1]-m[1,2],m[0,2]-m[2,0],m[1,0]-m[0,1],m.trace()]])/3
 q=np.linalg.eigh(k)[1][:,-1];assert np.max(np.abs(rotation(q)-m))<1e-6;return q
reports=[]
for label in phase:
 source=json.loads(source_paths[label].read_text());tracks={t['name']:t['keys'] for t in source['tracks']}
 doc=json.loads((B/(label.lower()+'-motion.json')).read_text());assert len(doc['frames'])==source['frames']
 errors=[];old_x=[];new_x=[]
 for frame,entry in enumerate(doc['frames']):
  pose=entry['pose']['Snapshot'];ts=pose['LocalTransforms'];locals=[matrix({'translation':[t['Translation'][k] for k in 'XYZ'],'rotation':[t['Rotation'][k] for k in 'XYZW']}) for t in ts[:len(bind)]]
  sw=worlds([matrix(tracks[b['name']][frame] if b['name'] in tracks else b) for b in sbind],sbind)
  for side in ['l','r']:
   ids=[names.index(n+'_'+side) for n in ['thigh','calf','foot']];src=[snames.index('Bip001-'+side.upper()+'-'+n) for n in ['Thigh','Calf','Foot']]
   tw=worlds(locals,bind);h,k,f=[tw[i][:3,3].copy() for i in ids];sh,sk,sf=[sw[i][:3,3] for i in src]
   a=np.linalg.norm(k-h);b=np.linalg.norm(f-k);sa=np.linalg.norm(sk-sh);sb=np.linalg.norm(sf-sk);ratio=(a+b)/(sa+sb)
   goal=h+(sf-sh)*ratio;delta=goal-h;d=np.linalg.norm(delta);limit=a+b-1e-4
   if d>limit:goal=h+delta*limit/d;d=limit
   axis=(goal-h)/d;pole=sk-sh;pole=pole-axis*np.dot(pole,axis);assert np.linalg.norm(pole)>1e-4;pole/=np.linalg.norm(pole)
   along=(a*a-b*b+d*d)/(2*d);knee=h+axis*along+pole*math.sqrt(max(0,a*a-along*along))
   desired=swing(k-h,knee-h)@tw[ids[0]][:3,:3];par=bind[ids[0]]['parent'];locals[ids[0]][:3,:3]=tw[par][:3,:3].T@desired
   tw=worlds(locals,bind);ck,cf=[tw[i][:3,3] for i in ids[1:]]
   desired=swing(cf-ck,goal-ck)@tw[ids[1]][:3,:3];locals[ids[1]][:3,:3]=tw[ids[0]][:3,:3].T@desired
   tw=worlds(locals,bind);desired=sw[src[2]][:3,:3]@srest[src[2]][:3,:3].T@trest[ids[2]][:3,:3]
   locals[ids[2]][:3,:3]=tw[ids[1]][:3,:3].T@desired
   tw=worlds(locals,bind);error=float(np.linalg.norm(tw[ids[2]][:3,3]-goal));assert error<.001
   errors.append(error);old_x.append(float(f[0]));new_x.append(float(goal[0]))
   for i in ids:ts[i]['Rotation']=dict(zip('XYZW',quat(locals[i][:3,:3]).tolist()))
 # Retain the validated whole-key phase alignment from P1.
 offset=phase[label]['start_frame'];fs=doc['frames'];count=len(fs)-1
 if offset:doc['frames']=[copy.deepcopy(fs[(i+offset)%count]) for i in range(count+1)]
 (W/(label+'-motion.json')).write_text(json.dumps(doc)+'\n')
 reports.append(dict(clip=label,frames=len(fs),max_ankle_error_cm=max(errors),old_lateral_span_cm=max(old_x)-min(old_x),new_lateral_span_cm=max(new_x)-min(new_x)))
(W/'trajectory-report.json').write_text(json.dumps(reports,indent=2)+'\n');print(json.dumps(reports))
