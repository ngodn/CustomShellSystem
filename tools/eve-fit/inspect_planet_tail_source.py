"""Read the author's tail proxy, pins and bone constraints without saving the blend."""
import bpy
import hashlib
import json
from pathlib import Path
root = Path('/home/eins0fx/development/mods/msII')
source = root / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/reference/body-type-variant-EVE/eve_beta10.blend'
out = root / 'CustomShellSystem/work/eve26/planet-tail-author.json'
assert not out.exists()
before = hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(source))
rows = []
for obj in bpy.data.objects:
    if not obj.name.startswith('Eve Prototype Planet Diving Suit - Tail'):
        continue
    row = {'name': obj.name, 'type': obj.type, 'matrix': [list(r) for r in obj.matrix_world], 'modifiers': []}
    for mod in obj.modifiers:
        entry = {'name':mod.name, 'type':mod.type}
        if mod.type == 'ARMATURE':
            entry['object'] = mod.object.name if mod.object else None
        elif mod.type == 'CLOTH':
            entry['pin_group'] = mod.settings.vertex_group_mass
            entry['quality'] = mod.settings.quality
            entry['mass'] = mod.settings.mass
        row['modifiers'].append(entry)
    if obj.type == 'MESH':
        groups = {g.index:g.name for g in obj.vertex_groups}
        row['groups'] = list(groups.values())
        row['vertices'] = [{'co':list(v.co),'weights':{groups[g.group]:g.weight for g in v.groups}} for v in obj.data.vertices]
        row['faces'] = [list(p.vertices) for p in obj.data.polygons]
    if obj.type == 'ARMATURE':
        row['bones'] = [{'name':b.name,'head':list(b.head_local),'tail':list(b.tail_local),'parent':b.parent.name if b.parent else None} for b in obj.data.bones]
        row['constraints'] = [{'bone':b.name,'constraints':[{'name':c.name,'type':c.type,'target':getattr(getattr(c,'target',None),'name',None),'subtarget':getattr(c,'subtarget',None)} for c in b.constraints]} for b in obj.pose.bones if b.constraints]
    rows.append(row)
assert rows
assert hashlib.sha256(source.read_bytes()).hexdigest() == before
out.write_text(json.dumps({'source_sha256':before,'source_unchanged':True,'objects':rows},indent=2)+'\n')
print('PLANET_TAIL_AUTHOR_DONE',[(r['name'],r['type'],len(r.get('vertices',[]))) for r in rows])
