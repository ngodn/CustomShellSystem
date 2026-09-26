"""Bound discontinuities in a garment morph without changing its base fit or topology."""
import argparse,json,sys
from pathlib import Path
import bpy
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
from holiday_candidate import coords,digest
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);assert not a.output.exists()
body=bpy.data.objects['Eve Body'];before=digest(body)
o=bpy.data.objects['Eve Christmas - Dress'];base=coords(o.data.shape_keys.key_blocks[0].data)
key=o.data.shape_keys.key_blocks['PBMBreastsSize'];original=coords(key.data)-base;shift=original.copy()
edges=np.asarray([e.vertices[:] for e in o.data.edges]);i,j=edges.T
length=np.linalg.norm(base[i]-base[j],axis=1)
valid=length>1e-6;i,j,length=i[valid],j[valid],length[valid]
limit=length*1.0
history=[]
for iteration in range(400):
 difference=shift[i]-shift[j];amount=np.linalg.norm(difference,axis=1)
 excess=amount-limit
 active=excess>1e-6
 if iteration%40==0:history.append(dict(iteration=iteration,violations=int(active.sum()),maximum_gradient=float(np.max(amount/length))))
 if not active.any():break
 correction=difference[active]*(excess[active]/np.maximum(amount[active],1e-12))[:,None]*.5
 move=np.zeros_like(shift);degree=np.zeros(len(shift),dtype=np.float32)
 np.add.at(move,i[active],-correction);np.add.at(move,j[active],correction)
 np.add.at(degree,i[active],1);np.add.at(degree,j[active],1)
 used=degree>0;shift[used]+=move[used]/degree[used,None]
key.data.foreach_set('co',(base+shift).ravel())
assert np.array_equal(coords(o.data.shape_keys.key_blocks[0].data),base)
assert digest(body)==before
old_edges=np.linalg.norm((base+original)[i]-(base+original)[j],axis=1)/length
new_edges=np.linalg.norm((base+shift)[i]-(base+shift)[j],axis=1)/length
report=dict(body_unchanged=True,base_unchanged=True,shape=key.name,iterations=iteration+1,history=history,
 max_edge_stretch_before=float(old_edges.max()),max_edge_stretch_after=float(new_edges.max()),
 max_offset_adjustment_mm=float(np.linalg.norm(shift-original,axis=1).max()*1000),
 scope='Continuity candidate only. Does not prove morph skin clearance or aesthetic acceptance.')
bpy.ops.wm.save_as_mainfile(filepath=str(a.output));a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
print('MORPH_CONTINUITY_SAVED')
