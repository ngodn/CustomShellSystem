"""Read color-mask source boundaries from the accepted Eve blend. Blender 5.2 Python."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import bpy

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--blend', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
assert not args.output.exists()
before = hashlib.file_digest(args.blend.open('rb'), 'sha256').hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(args.blend.resolve()))
mesh = bpy.data.objects['Eve Body'].data
assert mesh.materials[2].name == 'Eve Body' and mesh.materials[5].name == 'Eve Genitals'
keys = mesh.shape_keys.key_blocks
basis = keys[0]
shapes = {name: [(a.co - b.co).length for a, b in zip(keys[name].data, basis.data)]
          for name in ('PBMAreolaeDepth', 'PBMNipplesDepth')}
mesh.calc_loop_triangles()
uv = mesh.uv_layers.active.data
triangles = [dict(material=t.material_index, vertices=list(t.vertices),
                  uv=[list(uv[i].uv) for i in t.loops])
             for t in mesh.loop_triangles if t.material_index in (2, 5)]
assert hashlib.file_digest(args.blend.open('rb'), 'sha256').hexdigest() == before
result = dict(blend=str(args.blend.resolve()), blend_sha256=before,
              object='Eve Body', uv_layer=mesh.uv_layers.active.name,
              shapes=shapes, triangles=triangles)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(result) + '\n')
print('Extracted', len(triangles), 'triangles; source blend unchanged')
