"""Host Python 3.14: compare the full candidate export with V43."""
from collections import Counter
import json
from pathlib import Path

import os
OUT=Path(os.environ['CSS_ARM_REST_AUDIT_DIR']).resolve()
if OUT.parent != Path(__file__).resolve().parents[3] / 'work/grip-grounding-v1':
 raise ValueError('Audit directory must be a direct workspace grip-grounding child')
ROOT=OUT.parents[3]
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
baseline=MOD/'work/exports-nextgen/SK_SeduXtress_HandBindV43.mesh.json'
a=json.loads(baseline.read_text());b=json.loads((OUT/'full-candidate.mesh.json').read_text())
old=json.loads(baseline.with_suffix('.audit.json').read_text())
new=json.loads((OUT/'full-candidate.mesh.audit.json').read_text())
assert a['influences']==b['influences'] and a['materials']==b['materials']
assert old['counts']==new['counts']
assert Counter((tuple(w),tuple(c)) for w,c in zip(a['wedges'],a['colors'],strict=True))==Counter((tuple(w),tuple(c)) for w,c in zip(b['wedges'],b['colors'],strict=True))
for x,y in zip(old['parts'],new['parts'],strict=True):
 assert x['name']==y['name'] and x['baked_fit_keys']==y['baked_fit_keys'] and x['exported_shapes']==y['exported_shapes']

def triangles(mesh):
 return [tuple(mesh['wedges'][i][0] for i in f[:3])+(f[3],) for f in mesh['faces']]

ta,tb=triangles(a),triangles(b)
changed={i for i,(x,y) in enumerate(zip(ta,tb,strict=True)) if x!=y}
groups=[]
def boundary(tris):
 edges=Counter((t[j],t[(j+1)%3]) for t in tris for j in range(3))
 return {e:c-edges[(e[1],e[0])] for e,c in edges.items() if c>edges[(e[1],e[0])]}

while changed:
 first=min(changed)
 matches=[]
 for other in changed-{first}:
  pair={first,other}
  points_a={p for i in pair for p in ta[i][:3]};points_b={p for i in pair for p in tb[i][:3]}
  if points_a==points_b and len(points_a)==4 and boundary([ta[i] for i in pair])==boundary([tb[i] for i in pair]):matches.append(other)
 assert len(matches)==1,(first,matches)
 group={first,matches[0]};changed-=group
 before={p for i in group for p in ta[i][:3]};after={p for i in group for p in tb[i][:3]}
 assert before==after and len(before)==4
 assert len({ta[i][3] for i in group}|{tb[i][3] for i in group})==1
 assert boundary([ta[i] for i in group])==boundary([tb[i] for i in group])
 groups.append(dict(face_indices=sorted(group),quad_points=sorted(before)))
assert json.loads((OUT/'export.result.json').read_text())['exit_code']==0
result=dict(counts=new['counts'],influences_and_materials_exact=True,
 vertex_uv_color_associations_preserved=True,fit_values_and_exported_morph_names_exact=True,
 retessellated_quads=groups,standard_bind_policy=json.loads((OUT/'canonical-policy-validation.json').read_text()),
 scope=f'Full B2 Schema 1 export. {len(groups)} reposed quads choose the opposite diagonal; point sets, material and oriented quad boundaries remain unchanged. Not a cooked or deployed asset.')
(OUT/'full-export-validation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='retessellated_quads'}));print('Retessellated quads:',len(groups))
