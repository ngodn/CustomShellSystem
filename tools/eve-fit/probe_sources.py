import bpy,json,hashlib
from pathlib import Path
import numpy as np
root=Path('/home/eins0fx/development/mods/msII')
mod=root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
report={}
for label,path in [('fixed',mod/'gemini-work/CSS_SeduXtress_Variants_Fixed.blend'),('master',mod/'authoring/CSS_SeduXtress_Eve_Master.blend'),('original',mod/'reference/body-type-variant-EVE/eve_beta10.blend')]:
 if not path.exists():continue
 bpy.ops.wm.open_mainfile(filepath=str(path))
 rows={}
 for o in bpy.data.objects:
  if o.type!='MESH' or not (o.name.startswith('Eve Body') or o.name.startswith('Eve Christmas - Arms') or o.name.startswith('Eve Christmas - Dress')):continue
  coords=np.array([v.co[:] for v in o.data.vertices])
  keys=o.data.shape_keys
  rigs=[m.object for m in o.modifiers if m.type=='ARMATURE' and m.object]
  bones={b.name for r in rigs for b in r.data.bones}
  vg={g.index:g.name for g in o.vertex_groups}
  rows[o.name]={'count':len(coords),'bounds':[coords.min(0).tolist(),coords.max(0).tolist()], 'coord_hash':hashlib.sha256(coords.tobytes()).hexdigest(),'world':[list(r) for r in o.matrix_world], 'rigs':[(r.name,r.data.pose_position) for r in rigs], 'rig_weighted':sum(any(vg[w.group] in bones and w.weight>0.00001 for w in v.groups) for v in o.data.vertices),'active_shapes':[(k.name,k.value,k.vertex_group,k.mute) for k in keys.key_blocks if k.value] if keys else [],'basis_diff':float(np.max(np.abs(coords-np.array([v.co[:] for v in keys.key_blocks[0].data])))) if keys else 0,'groups':list(vg.values())[:20]}
 report[label]=rows
(root/'CustomShellSystem/work/eve26/fit-probe.json').write_text(json.dumps(report,indent=2))
print('FIT_PROBE_DONE')
