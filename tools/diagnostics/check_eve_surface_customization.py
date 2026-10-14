"""Compare six repaired Eve outfits with their accepted customization targets.

Python 3.14. This checks decoded geometry and metadata, not live rendering.
"""
import argparse
from collections import Counter
import copy
import json
from pathlib import Path

from check_import_cooked_mesh import sha


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read(path):
    return json.loads(path.read_text())


def mesh(exports):
    matches = [e for e in exports if e['Type'] == 'SkeletalMesh']
    require(len(matches) == 1, 'Expected one skeletal mesh')
    return matches[0]


def slot_triangles(item):
    result = []
    for lod in item['LODModels']:
        counts = Counter()
        for section in lod['Sections']:
            slot = section['MaterialIndex']
            require(0 <= slot < len(item['SkeletalMaterials']), 'Render section has invalid material slot')
            if not section['bDisabled']:
                counts[slot] += section['NumTriangles']
        result.append(dict(sorted(counts.items())))
    return result


def compare(custom, previous, current):
    require(previous['SkeletalMaterials'] == current['SkeletalMaterials'], 'Material slots changed')
    before, after = slot_triangles(previous), slot_triangles(current)
    require(before == after, 'Visible triangle coverage changed by material slot')
    controls = custom['controls']
    ids = [c['id'] for c in controls]
    require(len(ids) == len(set(ids)), 'Duplicate control IDs')
    targets = set()
    shapes = []
    for control in controls:
        targets.update(control.get('sections', []))
        targets.update(control.get('occludes_sections', []))
        targets.update(b['slot'] for b in control.get('bindings', []))
        if control['kind'] == 'shape':
            shapes.append(control['morph'])
    for surface in custom.get('surfaces', []):
        targets.update(surface['slots'])
    require(all(type(s) is int and 0 <= s < len(current['SkeletalMaterials']) for s in targets),
            'Customization targets an absent material slot')
    morphs = current['OfflineMeshDigest']['morphs']
    require(all(name in morphs and sum(lod['deltas'] for lod in morphs[name]) > 0 for name in shapes),
            'Shape control targets a missing or empty morph')
    require(all(set(p['values']) <= set(ids) for p in custom.get('palettes', [])),
            'Palette targets an absent control')
    return dict(materials=len(current['SkeletalMaterials']), lods=len(after),
                visible_triangles_by_slot=after, addressed_slots=sorted(targets),
                shape_morphs=shapes, controls=len(controls), palettes=len(custom.get('palettes', [])))


def negative_controls(custom, previous, current):
    cases = []
    changed = copy.deepcopy(current)
    changed['SkeletalMaterials'][0]['MaterialSlotName'] += '_wrong'
    cases.append(('material_identity', custom, changed, 'Material slots changed'))
    changed = copy.deepcopy(current)
    changed['LODModels'][0]['Sections'][0]['NumTriangles'] += 1
    cases.append(('triangle_coverage', custom, changed, 'Visible triangle coverage changed'))
    changed_custom = copy.deepcopy(custom)
    toggle = next(c for c in changed_custom['controls'] if c['kind'] == 'toggle')
    toggle['sections'].append(len(current['SkeletalMaterials']))
    cases.append(('absent_slot', changed_custom, current, 'Customization targets an absent'))
    changed = copy.deepcopy(current)
    name = next(c['morph'] for c in custom['controls'] if c['kind'] == 'shape')
    del changed['OfflineMeshDigest']['morphs'][name]
    cases.append(('missing_morph', custom, changed, 'Shape control targets a missing'))
    for label, controls, candidate, expected in cases:
        try:
            compare(controls, previous, candidate)
        except ValueError as error:
            require(expected in str(error), 'Negative control failed for an unrelated reason: '+label)
        else:
            raise ValueError('Negative control was accepted: '+label)
    return [c[0] for c in cases]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'Output already exists')
    out = args.candidate.resolve(strict=True)
    proof, plan, inventory = [read(p) for p in (out/'verified.json', out/'plan.json', args.inventory)]
    require(proof['passed'] and proof['plan_sha256'] == sha(out/'plan.json'), 'Unverified candidate')
    owner = 'CSS_EveStellarBlade_eins0fx_P'
    for row in inventory['packages'][owner]['files'].values():
        require(plan['protected'].get(row['path']) == row['sha256'], 'Inventory differs from accepted Eve baseline')
    baseline_path = Path(inventory['raw_readback']['path'])
    require(sha(baseline_path) == inventory['raw_readback']['sha256'], 'Baseline readback changed')
    old_manifest, = (out/'metadata-original'/owner).rglob('manifest.json')
    new_manifest, = (out/'metadata'/owner).rglob('manifest.json')
    paths = [Path(__file__), args.inventory, baseline_path, old_manifest, new_manifest,
             out/'verified.json', out/'plan.json', out/'meshes.json']
    inputs = {str(p.resolve()): sha(p) for p in paths}
    for name, digest in proof['files'].items():
        require(sha(out/'pack'/name) == digest, 'Candidate package changed: '+name)
    before, after = read(old_manifest), read(new_manifest)
    require(before['catalog'] == after['catalog'], 'Accepted catalog or customization changed')
    baseline, candidate = read(baseline_path), read(out/'meshes.json')
    variants = {v['id']: v for o in after['catalog']['outfits'] for v in o['variants']}
    rows, negatives, visited = [], [], set()
    for variant in inventory['variants']:
        if variant['outfit'] != 'eins0fx.seduxtress' or variant['variant'] == 'peony':
            continue
        custom = variants[variant['variant']]['customize']
        require(variants[variant['variant']]['mesh'].rsplit('.', 1)[0] == variant['meshes']['normal'],
                'Normal mesh selection changed')
        for kind, package in variant['meshes'].items():
            require(package in candidate and package not in visited, 'Missing or duplicate repaired mesh')
            visited.add(package)
            old, new = mesh(baseline[package]), mesh(candidate[package])
            row = compare(custom, old, new)
            rows.append(dict(variant=variant['variant'], kind=kind, package=package, **row))
            negatives.append(dict(package=package, rejected=negative_controls(custom, old, new)))
    require(len(rows) == 12 and visited == set(candidate), 'Incomplete repaired-mesh coverage')
    require(all(sha(Path(p)) == h for p, h in inputs.items()), 'Input changed during audit')
    args.output.write_text(json.dumps(dict(passed=True, rows=rows, negative_controls=negatives,
        inputs=inputs, files=proof['files'], catalog_unchanged=True,
        scope='Six outfits, normal/facial meshes: accepted slot identities and visible triangle coverage per LOD; '
              'control slots and nonempty shape targets; unchanged palettes and controls. '
              'Does not prove material parameter execution, posed fit, cloth motion or game rendering.'), indent=2)+'\n')
    print('Customization targets preserved across 12 meshes; 48 negative controls rejected')


if __name__ == '__main__':
    main()
