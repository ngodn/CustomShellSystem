"""Blender 5.2 read-only outfit, material ownership and rig inventory."""
import hashlib
import json
import sys
from pathlib import Path
import bpy

output = Path(sys.argv[sys.argv.index('--') + 1])
assert not output.exists()
objects = []
for obj in bpy.data.objects:
    if obj.type not in ('MESH', 'ARMATURE'):
        continue
    entry = dict(name=obj.name, type=obj.type, parent=obj.parent.name if obj.parent else None,
                 matrix=[list(row) for row in obj.matrix_world],
                 modifiers=[dict(name=m.name, type=m.type,
                     target=getattr(m, 'object', None).name if getattr(m, 'object', None) else None)
                     for m in obj.modifiers])
    if obj.type == 'MESH':
        materials = [m.name if m else None for m in obj.data.materials]
        counts = [0] * len(materials)
        for face in obj.data.polygons:
            if face.material_index < len(counts):
                counts[face.material_index] += 1
        entry.update(vertices=len(obj.data.vertices), faces=len(obj.data.polygons),
                     materials=[dict(name=n, faces=c) for n, c in zip(materials, counts)],
                     shape_keys=[k.name for k in obj.data.shape_keys.key_blocks] if obj.data.shape_keys else [],
                     vertex_groups=[g.name for g in obj.vertex_groups],
                     unweighted_vertices=sum(not v.groups for v in obj.data.vertices))
    else:
        entry['bones'] = [dict(name=b.name, parent=b.parent.name if b.parent else None)
                          for b in obj.data.bones]
    objects.append(entry)
source = Path(bpy.data.filepath)
with source.open('rb') as stream:
    checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
output.write_text(json.dumps(dict(source=str(source), sha256=checksum,
    blender=bpy.app.version_string, objects=objects,
    scope='Read-only source inventory. No mesh, pose, material or file edits.'), indent=2) + '\n')
print('Saved source inventory:', output)
