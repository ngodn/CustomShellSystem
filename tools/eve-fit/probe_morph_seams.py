"""Read-only Holiday morph seam and connected-edge measurements."""
import argparse,bpy,json,sys
from pathlib import Path
import numpy as np
from mathutils.kdtree import KDTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
from holiday_candidate import coords
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
rows={}
for part in ('Dress','Arms','Legs','Panties'):
 o=bpy.data.objects['Eve Christmas - '+part];base=coords(o.data.shape_keys.key_blocks[0].data)
 tree=KDTree(len(base))
 for i,p in enumerate(base):tree.insert(p,i)
 tree.balance();pairs=[]
 for i,p in enumerate(base):
  for _,j,d in tree.find_range(p,.0001):
   if j>i:pairs.append((i,j,d))
 morphs={}
 for name in ('PBMBreastsSize','PBMHipSize','PBMGlutesSize','PBMWaistWidth'):
  points=coords(o.data.shape_keys.key_blocks[name].data)
  gaps=sorted([(float(np.linalg.norm(points[i]-points[j])),i,j,d) for i,j,d in pairs],reverse=True)
  morphs[name]=dict(max_gap_mm=gaps[0][0]*1000 if gaps else 0,worst=[dict(gap_mm=d*1000,vertices=[i,j],base_gap_mm=b*1000,position=base[i].tolist()) for d,i,j,b in gaps[:8]])
 edges=[]
 points=coords(o.data.shape_keys.key_blocks['PBMBreastsSize'].data)
 for e in o.data.edges:
  i,j=e.vertices;length=np.linalg.norm(base[i]-base[j]);b=np.linalg.norm(points[i]-points[j])
  if length>.0001 and b/length>2:edges.append(dict(vertices=[i,j],base_mm=float(length*1000),morphed_mm=float(b*1000),ratio=float(b/length),position=((base[i]+base[j])/2).tolist()))
 rows[part]=dict(near_duplicate_pairs=len(pairs),morphs=morphs,stretched_edges=sorted(edges,key=lambda e:e['ratio'],reverse=True)[:20])
a.output.write_text(json.dumps(rows,indent=2)+'\n')
print('SEAM_PROBE_DONE')
