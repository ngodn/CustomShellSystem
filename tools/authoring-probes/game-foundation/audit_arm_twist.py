import os
import bpy,json,hashlib,ast
from pathlib import Path
OUT=Path(os.environ['CSS_FOUNDATION_AUDIT_DIR']).resolve();ROOT=OUT.parents[3]
assert OUT.parent == Path(__file__).resolve().parents[4]/'CustomShellSystem/work/grip-grounding-v1'
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
source=MOD/'reference/body-type-variant-EVE/eve_beta10.blend';target=MOD/'work/CSS_SeduXtress_HandBindV43.blend'
paths=[source,target];before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
syntax=ast.parse((MOD/'tools/bind_eve_to_css_base_clean.py').read_text());mapping=next(ast.literal_eval(n.value) for n in syntax.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='vg_mapping' for t in n.targets))
bpy.ops.wm.open_mainfile(filepath=str(source));body=bpy.data.objects['Eve Body'];rig=next(m.object for m in body.modifiers if m.type=='ARMATURE')
rows=[]
for side in ('L','R'):
 for region in ('upper_arm','forearm'):
  for kind in ('bend','twist'):
   name=f'{region}.{kind}.twk.{side}';bone=rig.data.bones[name];group=body.vertex_groups.get(name);weights=[g.weight for v in body.data.vertices for g in v.groups if group and g.group==group.index and g.weight>.0001]
   pb=rig.pose.bones[name]
   rows.append(dict(source=name,target=mapping[name],weighted_vertices=len(weights),weight_sum=sum(weights),maximum_weight=max(weights,default=0),parent=bone.parent.name if bone.parent else None,head=list(rig.matrix_world@bone.head_local),tail=list(rig.matrix_world@bone.tail_local),constraints=[dict(type=c.type,influence=c.influence,subtarget=getattr(c,'subtarget',None)) for c in pb.constraints]))
bpy.ops.wm.open_mainfile(filepath=str(target));body=bpy.data.objects['Eve Body'];targets=[]
for name in [f'{region}_twist_{n:02}_{side}' for side in ('l','r') for region in ('upperarm','lowerarm') for n in (1,2)]:
 group=body.vertex_groups.get(name);weights=[g.weight for v in body.data.vertices for g in v.groups if group and g.group==group.index and g.weight>.0001]
 targets.append(dict(name=name,weighted_vertices=len(weights),weight_sum=sum(weights)))
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in before.items())
result=dict(source_weights=rows,target_twist_weights=targets,source_files_unchanged=True,hashes=before,scope='Read-only source/target binding audit. Collapsed twist weights are established; contribution to each visible defect is not yet isolated.')
(OUT/'arm-twist-weight-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
