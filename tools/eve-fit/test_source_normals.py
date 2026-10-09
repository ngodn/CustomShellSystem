"""Exercise retained-normal guards on a real lining library in Blender 5.2.2."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import bpy

root = Path(__file__).resolve().parents[3]
exporter = root / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work'
sys.path.insert(0, str(exporter))
from export_variant_clean import export_corner_normals, fitted_mesh

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
if args.output.exists():
    raise FileExistsError(args.output)
before = hashlib.sha256(args.source.read_bytes()).hexdigest()
results = {}


def load():
    bpy.ops.wm.open_mainfile(filepath=str(args.source.resolve()))
    return bpy.data.objects['Eve Skin Suit - Footwear Lining']


def rejects(label, obj, mesh, reason):
    try:
        export_corner_normals(obj, mesh)
    except ValueError as error:
        if reason not in str(error):
            raise
        results[label] = str(error)
    else:
        raise AssertionError(label + ' unexpectedly accepted')


obj = load()
export_corner_normals(obj, obj.data)
evaluated = fitted_mesh(obj)[0]
export_corner_normals(obj, evaluated)
results['saved_and_evaluated'] = 'accepted'
obj.location.x += 2
obj.rotation_euler.z = 0.3
export_corner_normals(obj, obj.data)
results['object_transform'] = 'accepted: normals remain local'

obj = load()
obj.data.vertices[0].co.x += 0.001
rejects('moved_vertex', obj, obj.data, 'geometry changed')

obj = load()
obj.data.shape_keys.key_blocks['FBMBodyTone'].value = 1
rejects('changed_evaluated_shape', obj, fitted_mesh(obj)[0], 'geometry changed')

obj = load()
obj.data.attributes['CSS_ExportNormal'].data[0].vector.x += 0.1
rejects('modified_normal', obj, obj.data, 'normal data changed')

obj = load()
obj.data.attributes.remove(obj.data.attributes['CSS_ExportNormal'])
rejects('missing_attribute', obj, obj.data, 'missing or malformed')

obj = load()
obj.data.attributes.remove(obj.data.attributes['CSS_ExportNormal'])
obj.data.attributes.new('CSS_ExportNormal', 'FLOAT_VECTOR', 'POINT')
rejects('wrong_domain', obj, obj.data, 'missing or malformed')

obj = load()
del obj['CSS_export_normal_geometry']
rejects('missing_geometry_guard', obj, obj.data, 'geometry changed')

obj = load()
obj.data.attributes.remove(obj.data.attributes['CSS_ExportNormal'])
del obj['CSS_export_normal_geometry']
del obj['CSS_export_normal_data']
actual = export_corner_normals(obj, obj.data)
assert [tuple(v.vector) for v in actual] == [tuple(v.vector) for v in obj.data.corner_normals]
results['ordinary_mesh_fallback'] = 'unchanged'

assert hashlib.sha256(args.source.read_bytes()).hexdigest() == before
args.output.parent.mkdir(parents=True, exist_ok=True)
report = dict(passed=True, results=results, source_sha256=before,
              exporter_sha256=hashlib.sha256((exporter / 'export_variant_clean.py').read_bytes()).hexdigest(),
              script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              blender=bpy.app.version_string, original_unchanged=True)
args.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
