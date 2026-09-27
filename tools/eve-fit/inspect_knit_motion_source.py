"""Inspect the author's Knitwear dependencies without saving the source blend."""
import hashlib
import json
import argparse
import sys
from pathlib import Path
import bpy

root = Path(__file__).resolve().parents[3]
source = root / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/reference/body-type-variant-EVE/eve_beta10.blend'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--outfit', choices=('knit', 'alice', 'aegis'), default='knit')
parser.add_argument('--output', type=Path, default=root / 'CustomShellSystem/work/eve26/knit-author-motion.json')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
output = args.output
assert not output.exists()
digest = hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(source))
terms = {'alice': ('alice',), 'aegis': ('aegis',), 'knit': ('sweater', 'knit')}[args.outfit]
selected = [o for o in bpy.data.objects if any(term in o.name.lower() for term in terms)]
assert selected
rows = []
for obj in selected:
    row = dict(name=obj.name, type=obj.type, modifiers=[], constraints=[])
    for mod in obj.modifiers:
        item = dict(name=mod.name, type=mod.type)
        for attr in ('object', 'target'):
            value = getattr(mod, attr, None)
            if value is not None and hasattr(value, 'name'):
                item[attr] = value.name
        if mod.type == 'CLOTH':
            item.update(pin_group=mod.settings.vertex_group_mass, quality=mod.settings.quality)
        row['modifiers'].append(item)
    row['constraints'] = [dict(name=c.name, type=c.type,
        target=getattr(getattr(c, 'target', None), 'name', None)) for c in obj.constraints]
    if obj.type == 'MESH':
        row.update(vertices=len(obj.data.vertices), faces=len(obj.data.polygons),
            groups=[g.name for g in obj.vertex_groups],
            shape_keys=[k.name for k in obj.data.shape_keys.key_blocks] if obj.data.shape_keys else [])
        row['shape_values'] = {k.name: k.value for k in obj.data.shape_keys.key_blocks} if obj.data.shape_keys else {}
        used = {g.group for v in obj.data.vertices for g in v.groups if g.weight > 0}
        row['weighted_groups'] = [g.name for g in obj.vertex_groups if g.index in used]
        if args.outfit == 'alice' and obj.name == 'Eve Midsummer Alice - Ribbon':
            row['points_world'] = [list(obj.matrix_world @ v.co) for v in obj.data.vertices]
            row['weights'] = [[v.index, obj.vertex_groups[g.group].name, g.weight]
                for v in obj.data.vertices for g in v.groups if g.weight > 0]
        maps = {g.index:g.name for g in obj.vertex_groups if g.name.startswith('dForce')}
        row['cloth_groups'] = {name:[] for name in maps.values()}
        for vertex in obj.data.vertices:
            for group in vertex.groups:
                if group.group in maps and group.weight > 0:
                    row['cloth_groups'][maps[group.group]].append([vertex.index, group.weight])
        row['cloth_group_ranges'] = {}
        for name, weights in row['cloth_groups'].items():
            coordinates = [obj.data.vertices[index].co for index, _ in weights]
            row['cloth_group_ranges'][name] = dict(count=len(weights),
                minimum_weight=min((weight for _, weight in weights), default=0),
                maximum_weight=max((weight for _, weight in weights), default=0),
                local_minimum=[min((v[axis] for v in coordinates), default=0) for axis in range(3)],
                local_maximum=[max((v[axis] for v in coordinates), default=0) for axis in range(3)])
        rigs = [m.object for m in obj.modifiers if m.type == 'ARMATURE' and m.object]
        row['rig_constraints'] = []
        for rig in rigs:
            for bone in rig.pose.bones:
                if bone.name not in row['weighted_groups']:
                    continue
                row['rig_constraints'].append(dict(bone=bone.name,
                    parent=bone.parent.name if bone.parent else None,
                    constraints=[dict(type=c.type, target=getattr(getattr(c, 'target', None), 'name', None),
                        subtarget=getattr(c, 'subtarget', None)) for c in bone.constraints]))
    rows.append(row)
assert hashlib.sha256(source.read_bytes()).hexdigest() == digest
output.write_text(json.dumps(dict(source_sha256=digest, source_unchanged=True, objects=rows,
    scope='Named outfit objects and directly weighted rig bones only. Does not prove absence of indirect driver or proxy dependencies.'), indent=2) + '\n')
print([(r['name'], r['type']) for r in rows])
