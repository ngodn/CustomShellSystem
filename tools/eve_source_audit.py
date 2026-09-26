"""Read-only Eve manifest audit. Python 3.14; findings are not gameplay acceptance."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path


def audit(path):
    data = json.loads(path.read_text())
    rows = []
    for outfit in data['catalog']['outfits']:
        for variant in outfit['variants']:
            custom = variant.get('customize', {})
            controls = custom.get('controls', [])
            ids = [c['id'] for c in controls]
            toggles = [c for c in controls if c['kind'] == 'toggle']
            overlaps = []
            for left, right in itertools.combinations(toggles, 2):
                shared = sorted(set(left.get('sections', [])) & set(right.get('sections', [])))
                if shared:
                    overlaps.append(dict(controls=[left['id'], right['id']], sections=shared))
            palettes = custom.get('palettes', [])
            unknown = {p['id']: sorted(set(p.get('values', {})) - set(ids)) for p in palettes}
            surfaces = custom.get('surfaces', [])
            dye_ids = {key for s in surfaces if s.get('slots') and s.get('parameter')
                       for key in s.get('layers', {})}
            garment_ids = {c['id'] for c in controls if c.get('kind') == 'color'
                           and c.get('group') == 'outfit'
                           and c['id'] not in ('hair_color', 'hair_accessory_color')}
            bound_garment_ids = {c['id'] for c in controls if c['id'] in garment_ids
                                 and (c['id'] in dye_ids or c.get('bindings'))}
            garment_palettes = [p['id'] for p in palettes
                                if bound_garment_ids.intersection(p.get('values', {}))]
            rows.append(dict(id=variant['id'], name=variant['name'], mesh=variant['mesh'],
                controls=len(controls), duplicate_control_ids=sorted({i for i in ids if ids.count(i) > 1}),
                palettes=[dict(id=p['id'], name=p['name']) for p in palettes],
                palette_count_meets_five=len(palettes) >= 5,
                garment_color_controls=sorted(garment_ids),
                unbound_garment_color_controls=sorted(garment_ids - bound_garment_ids),
                palettes_with_garment_bindings=garment_palettes,
                garment_palette_count_meets_five=len(garment_palettes) >= 5,
                original_restore_status='requires runtime check',
                unknown_palette_controls={k: v for k, v in unknown.items() if v},
                shared_toggle_sections=overlaps,
                animation_slots=sorted(variant.get('animations', {})),
                fit_status='unverified', cloth_status='unverified', gameplay_status='unverified'))
    return dict(source=str(path.resolve()), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                version=data.get('version'), variants=rows,
                scope='Manifest definitions only. Shared sections require mesh-level review; they are not automatically errors.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    result = audit(args.manifest)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x') as out:
        json.dump(result, out, indent=2)
        out.write('\n')
    for v in result['variants']:
        print(f"{v['id']}: {len(v['palettes_with_garment_bindings'])}/{len(v['palettes'])} palettes target clothing, {len(v['shared_toggle_sections'])} shared toggle mappings, "
              f"{len(v['unknown_palette_controls'])} palettes with unknown controls")
