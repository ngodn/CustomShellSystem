"""Compare saved outfit-part geometry with Gemini's older export, read-only."""
import argparse,hashlib,json,sys
from pathlib import Path
import bpy
import numpy as np

root=Path(__file__).resolve().parents[3]
mod=root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work'
w=root/'CustomShellSystem/work/eve26'
sys.path.insert(0,str(mod))
from export_variant_clean import fitted_mesh,TO_UE
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--part',action='append',help='Exact Blender object/export part name; repeat for multiple parts')
parser.add_argument('--source',type=Path,default=mod/'CSS_SeduXtress_Variants_Fixed.blend')
parser.add_argument('--mesh',type=Path,default=mod/'exports/SK_Eve_SkinSuit.mesh.json')
parser.add_argument('--output',type=Path,default=w/'skin-source-comparison.json')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
source=args.source
before=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
old=json.loads(args.mesh.read_text())
audit=json.loads(args.mesh.with_suffix('.audit.json').read_text())
assert hashlib.sha256(args.mesh.read_bytes()).hexdigest()==audit['output_sha256']
names=args.part or ['Eve Skin Suit - Suit Complete']
parts={};offset=0
for part in audit['parts']:
    parts[part['name']]=(offset,part)
    offset+=part['points']
with bpy.data.libraries.load(str(source),link=False) as (_,data):data.objects=names
rows=[]
for obj in data.objects:
    assert obj
    bpy.context.scene.collection.objects.link(obj)
    modifiers=[dict(name=m.name,type=m.type) for m in obj.modifiers]
    mesh,deltas,active=fitted_mesh(obj);mesh.calc_loop_triangles()
    transform=TO_UE@obj.matrix_world;used=set()
    for triangle in mesh.loop_triangles:
        a,b,c=[transform@mesh.vertices[i].co for i in triangle.vertices]
        if (b-a).cross(c-a).length_squared>=1e-12:used.update(triangle.vertices)
    points=np.asarray([transform@mesh.vertices[i].co for i in sorted(used)])
    begin,part=parts[obj.name];assert len(points)==part['points'],(obj.name,len(points),part['points'])
    expected=np.asarray(old['points'][begin:begin+len(points)])
    distances=np.linalg.norm(points-expected,axis=1)
    rows.append(dict(name=obj.name,points=len(points),saved_modifiers=modifiers,baked_keys=active,
        exported_shape_names=list(deltas),old_export_comparison_cm=dict(maximum=float(distances.max()),
        median=float(np.median(distances)),p95=float(np.percentile(distances,95)))))
assert hashlib.sha256(source.read_bytes()).hexdigest()==before
out=args.output;assert not out.exists()
out.write_text(json.dumps(dict(source_sha256=before,source_unchanged=True,parts=rows,
    scope='Index-wise saved garment comparison. Topology correspondence, current export, fitting and runtime still require verification.'),indent=2)+'\n')
print(out.read_text())
