"""Verify lining topology and exact position, weight and morph continuity at its seam."""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('candidate',type=Path)
a=p.parse_args()
base=Path(__file__).resolve().parents[2]/'work/eve26/skin-foot-sections/skin.mesh.json'
old=json.loads(base.read_text());path=a.candidate/'skin.mesh.json';data=json.loads(path.read_text())
audit=json.loads(path.with_suffix('.audit.json').read_text())
assert hashlib.sha256(path.read_bytes()).hexdigest()==audit['output_sha256']
count=audit['parts'][0]['faces']
selected=[f for f in old['faces'][:count] if f[3] in (20,21)]
used=sorted({old['wedges'][w][0] for f in selected for w in f[:3]})
outside={old['wedges'][w][0] for f in old['faces'][:count] if f[3] not in (20,21) for w in f[:3]}
seam=set(used)&outside;mapping={v:len(old['points'])+i for i,v in enumerate(used)}
assert data['points'][:len(old['points'])]==old['points']
assert data['faces'][:len(old['faces'])]==old['faces']
weights={}
for v,b,w in data['influences']:weights.setdefault(v,[]).append([b,w])
for v in seam:
    assert data['points'][v]==data['points'][mapping[v]]
    assert weights[v]==weights[mapping[v]]
for target in data['morph_targets']:
    deltas={v:xyz for v,*xyz in target['deltas']}
    for v in seam:assert deltas.get(v,[0,0,0])==deltas.get(mapping[v],[0,0,0])
points=np.asarray(data['points'],dtype=np.float32)
faces=np.asarray([[data['wedges'][w][0] for w in f[:3]] for f in data['faces'][len(old['faces']):]])
cross=np.cross(points[faces[:,1]]-points[faces[:,0]],points[faces[:,2]]-points[faces[:,0]])
squared=np.sum(cross*cross,axis=1);area=np.sqrt(squared)/2
report=dict(source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),triangles=len(faces),
    degenerate_triangles=int((squared<1e-12).sum()),minimum_area_cm2=float(area.min()),
    importer_check='float32 cross-product squared < 1e-12, CSSImportMeshCommandlet.cpp:243',
    seam_vertices=len(seam),seam_positions_weights_morphs_exact=True,original_geometry_unchanged=True,
    scope='Exact seam inputs imply identical linear skinning for the same pose. Interior folds, normals, visibility and actual runtime still require inspection.')
out=a.candidate/'geometry-check.json';assert not out.exists()
out.write_text(json.dumps(report,indent=2)+'\n');print(report)
assert report['degenerate_triangles']==0
