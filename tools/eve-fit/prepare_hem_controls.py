"""Separate pelvis-driven lower hem without changing its baseline deformation."""
import copy
import json
import math
from pathlib import Path

work=Path(__file__).resolve().parents[2]/'work/eve26'
output=work/'holiday-hem.mesh.json';assert not output.exists()
m=json.loads((work/'holiday-follow.mesh.json').read_text());before=copy.deepcopy(m)
names={b['name']:i for i,b in enumerate(m['bones'])}
pelvis=names['CSS_Cloth_Skirt_F_01']
mapping=json.loads((work/'skirt-follow.json').read_text())['drivers']
panels={s:'CSS_Cloth_Skirt_'+s+'_04' for s in 'FBLR'}
for name in panels.values():assert name not in mapping;mapping[name]='pelvis'
rows=[];affected=set();counts={}
for v,b,w in m['influences']:
    if b!=pelvis:
        rows.append([v,b,w]);continue
    x,y,z=m['points'][v]
    alpha=min(1.,max(0.,(112-z)/9));alpha=alpha*alpha*(3-2*alpha)
    if alpha==0:rows.append([v,b,w]);continue
    dx=x-.00028577;dy=y+1.2014805
    sectors={'F':max(0.,-dy),'B':max(0.,dy),'L':max(0.,dx),'R':max(0.,-dx)}
    total=sum(sectors.values());assert total>0
    if alpha<1:rows.append([v,b,w*(1-alpha)])
    for side,value in sectors.items():
        if value>0:rows.append([v,names[panels[side]],w*alpha*value/total])
    affected.add(v)
for v,b,w in rows:counts[v]=counts.get(v,0)+1
assert max(counts.values())<=8,sorted([(v,n) for v,n in counts.items() if n>8])[:20]
original={};changed={}
for v,b,w in before['influences']:original[v]=original.get(v,0.)+w
for v,b,w in rows:changed[v]=changed.get(v,0.)+w
assert max(abs(original[v]-changed[v]) for v in original)<1e-12
m['influences']=rows;m['mesh_package']='/Game/CSS/EveTest/SK_HolidayHem'
assert all(m[k]==before[k] for k in m if k not in ('influences','mesh_package'))
output.write_text(json.dumps(m,separators=(',',':')))
(work/'skirt-hem.json').write_text(json.dumps({'drivers':mapping,'panels':panels,
    'changed_vertices':len(affected),'max_influences':max(counts.values()),
    'anchor_z_cm':112,'full_weight_z_cm':103,
    'scope':'Four existing bone carriers split only pelvis-driven garment influences below z112. Geometry, body weights, morphs and skeleton unchanged. Requires paired bind-compensated rig; no dynamics yet.'},indent=2)+'\n')
print('Hem controls:',len(affected),'vertices; max influences',max(counts.values()))
