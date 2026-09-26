"""Extract the preserved body into a separate private collision-authoring mesh."""
import argparse
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--receipt', type=Path, required=True)
a = p.parse_args()
assert not a.output.exists() and not a.receipt.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
source = json.loads(a.source.read_text())
reference = json.loads((w/'body-collider.json').read_text())
audit = json.loads((w/'holiday.mesh.audit.json').read_text())['parts'][0]
assert audit['name'] == 'Eve Body'
ids = reference['source_vertices']
point_map = {old: new for new, old in enumerate(ids)}
assert len(ids) == len(point_map) == audit['points']
assert [source['points'][i] for i in ids] == reference['positions']
faces = source['faces'][:audit['faces']]
assert [[point_map[source['wedges'][w][0]] for w in f[:3]] for f in faces] == reference['indices']
wedge_ids = sorted({i for f in faces for i in f[:3]})
wedge_map = {old: new for new, old in enumerate(wedge_ids)}
materials = sorted({f[3] for f in faces})
material_map = {old: new for new, old in enumerate(materials)}
out = {k: source[k] for k in ('schema', 'bones', 'uv_channels')}
out.update(mesh_package='/Game/CSS/EveTest/SK_CBody', skeleton_package='/Game/CSS/EveTest/SKEL_CBody')
out['points'] = [source['points'][i] for i in ids]
out['materials'] = [source['materials'][i] for i in materials]
out['wedges'] = [[point_map[source['wedges'][i][0]], *source['wedges'][i][1:]] for i in wedge_ids]
out['faces'] = [[*(wedge_map[i] for i in f[:3]), material_map[f[3]], *f[4:]] for f in faces]
for key in ('normals', 'colors'):
    out[key] = [source[key][i] for i in wedge_ids]
out['influences'] = [[point_map[v], bone, weight] for v, bone, weight in source['influences'] if v in point_map]
out['morph_targets'] = [{'name': m['name'], 'deltas': [[point_map[v], *delta] for v, *delta in m['deltas'] if v in point_map]} for m in source['morph_targets']]
weights = [[] for _ in ids]
for v, bone, weight in out['influences']:
    weights[v].append([out['bones'][bone]['name'], weight])
assert weights == reference['weights']
a.output.write_text(json.dumps(out, separators=(',', ':'))+'\n')
report = {'source': str(a.source), 'source_sha256': hashlib.sha256(a.source.read_bytes()).hexdigest(),
          'output_sha256': hashlib.sha256(a.output.read_bytes()).hexdigest(),
          'points': len(ids), 'faces': len(faces), 'bones': len(out['bones']),
          'morphs': len(out['morph_targets']), 'body_geometry_and_weights_exact': True,
          'scope': 'Private body-only export for collision authoring. Original body, bone definitions and morph deltas retained; no production skeleton replacement or collider acceptance.'}
a.receipt.write_text(json.dumps(report, indent=2)+'\n')
print(report)
