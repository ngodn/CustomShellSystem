"""Check active roster mesh sections against cooked slots and catalog overrides."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('catalog', type=Path)
parser.add_argument('mesh_audit', type=Path)
parser.add_argument('override_shader_audit', type=Path)
args = parser.parse_args()
catalog = json.loads(args.catalog.read_text())
audit = json.loads(args.mesh_audit.read_text())
shaders = json.loads(args.override_shader_audit.read_text())
if not audit['passed'] or not shaders['passed']:
    raise SystemExit('Dependency or override shader audit failed')
verified = {f"{m['package']}.{m['export']}" for m in shaders['materials']}
meshes = {f"{m['package']}.{m['Name']}": m for m in audit['meshes']}
failures = []
checked = 0
for outfit in catalog['outfits']:
    for variant in outfit['variants']:
        mesh = meshes[variant['mesh']]
        materials = {m['index']: m['asset'] for m in mesh['materials']}
        for slot, asset in variant.get('materials', {}).items():
            if int(slot) not in materials or asset not in verified:
                failures.append(f"{variant['id']}: invalid or unverified material override {slot}: {asset}")
            materials[int(slot)] = asset
        active = {s['material'] for lod in mesh['lods'] for s in lod['sections']
                  if s['triangles'] and not s['disabled']}
        for slot in sorted(active):
            if not materials.get(slot):
                failures.append(f"{variant['id']}: active material slot {slot} has no material")
        checked += 1
print(json.dumps({'passed': not failures, 'variants': checked, 'failures': failures}, indent=2))
raise SystemExit(bool(failures))
