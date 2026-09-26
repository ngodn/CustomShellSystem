"""Give hem regions independent controls while preserving each original driver mass."""
import json
from pathlib import Path

work=Path(__file__).resolve().parents[2]/'work/eve26'
output=work/'holiday-hem2.mesh.json';assert not output.exists()
m=json.loads((work/'holiday-hip-clean.mesh.json').read_text())
names={b['name']:i for i,b in enumerate(m['bones'])}
drivers=['pelvis','butt001','butt002','thigh_l','thigh_r']
mapping={};panels={s:[] for s in 'FBLR'};lookup={}
for side in panels:
    for index,driver in enumerate(drivers,1):
        carrier=f'CSS_Cloth_Skirt_{side}_{index:02}'
        mapping[carrier]=driver;panels[side].append(carrier)
        lookup[(names[driver],side)]=names[carrier]
for side,driver in [('L','thigh_twist_02_l'),('R','thigh_twist_02_r')]:
    carrier=f'CSS_Cloth_Skirt_{side}_06';mapping[carrier]=driver;panels[side].append(carrier)
    lookup[(names[driver],side)]=names[carrier]
owners={names[c]:names[d] for c,d in mapping.items()}
slots={'MI_CH_P_EVE_Christmas_01_01.001','MI_CH_P_EVE_Christmas_01_Decal.001',
       'MI_EVE_HR_Christmas_01_Fur.001','MI_EVE_HR_15_Emissive1.001','MI_CH_P_EVE_Christmas_01_03.001'}
ids={m['materials'].index(s) for s in slots}
vertices={m['wedges'][w][0] for f in m['faces'] if f[3] in ids for w in f[:3]}
original={}
for v,b,w in m['influences']:original.setdefault(v,{})[b]=w
rows=[];changed=0;merged=0.;worst_merge=0.;maximum=0
for v,source in original.items():
    x,y,z=m['points'][v];alpha=min(1.,max(0.,(112-z)/9));alpha=alpha*alpha*(3-2*alpha)
    row={}
    if v not in vertices or alpha==0:row=source.copy()
    else:
        changed+=1;dx=x-.00028577;dy=y+1.2014805
        regions={'F':max(0.,-dy),'B':max(0.,dy),'L':max(0.,dx),'R':max(0.,-dx)}
        total=sum(regions.values());assert total>0
        for bone,weight in source.items():
            allowed={s:w for s,w in regions.items() if (bone,s) in lookup and w>0}
            if not allowed:row[bone]=weight;continue
            if alpha<1:row[bone]=weight*(1-alpha)
            part=sum(allowed.values())
            for side,w in allowed.items():row[lookup[(bone,side)]]=weight*alpha*w/part
        amount=0.
        while len(row)>8:
            options=[]
            for bone,weight in row.items():
                owner=owners.get(bone,bone)
                peers=[b for b in row if b!=bone and owners.get(b,b)==owner]
                if peers:options.append((weight,bone,max(peers,key=lambda b:row[b])))
            assert options
            weight,bone,recipient=min(options)
            row[recipient]+=row.pop(bone);amount+=weight
        merged+=amount;worst_merge=max(worst_merge,amount)
        restored={}
        for bone,weight in row.items():
            owner=owners.get(bone,bone);restored[owner]=restored.get(owner,0.)+weight
        assert set(restored)==set(source)
        assert max(abs(restored[b]-source[b]) for b in source)<1e-12
    maximum=max(maximum,len(row));rows.extend([v,b,w] for b,w in row.items())
m['influences']=rows;m['mesh_package']='/Game/CSS/EveTest/SK_HolidayHem2'
output.write_text(json.dumps(m,separators=(',',':')))
(work/'skirt-hem2.json').write_text(json.dumps({'drivers':mapping,'panels':panels,
    'changed_vertices':changed,'max_influences':maximum,'total_merged_same_driver_mass':merged,
    'max_vertex_merged_same_driver_mass':worst_merge,
    'scope':'22 existing carriers share four regional displacements. Only lower garment influences split; original driver mass is exact. Eight-influence consolidation merges only same-driver copies, preserving baseline deformation but approximating the regional motion field. No dynamics/runtime acceptance.'},indent=2)+'\n')
print('Regional hem controls',len(mapping),'vertices',changed,'max influences',maximum,'max merged regional mass',worst_merge)
