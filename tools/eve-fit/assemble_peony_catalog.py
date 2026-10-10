"""Merge the Peony wardrobe with proven Eve body controls, Python 3.14.

Writes a private seven-variant catalog and resources. Container identities and
facial adapters must be regenerated after cooking; this is not a release package.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from css_controls import check_shapes, resource_info, validate


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify_body(source, audit, target, assembly):
    """Check ordered body/hair corners, weights and morphs before reusing dye UVs."""
    if source['bones'] != target['bones'] or source['uv_channels'] != target['uv_channels']:
        raise ValueError('Body skeleton or UV layout changed')
    selected = {p['name'] for p in assembly['body_parts']}
    point_map, face_ids = {}, []
    point_start = face_start = 0
    for part in audit['parts']:
        if part['name'] in selected:
            for old in range(point_start, point_start + part['points']):
                point_map[old] = len(point_map)
            face_ids.extend(range(face_start, face_start + part['faces']))
        point_start += part['points']
        face_start += part['faces']
    if len(point_map) != assembly['garment_offsets']['points']:
        raise ValueError('Body/hair point partition changed')
    for old, new in point_map.items():
        if source['points'][old] != target['points'][new]:
            raise ValueError('Body/hair neutral geometry changed')
    for new_index, old_index in enumerate(face_ids):
        old, new = source['faces'][old_index], target['faces'][new_index]
        old_name, new_name = source['materials'][old[3]], target['materials'][new[3]]
        if new_name == 'Peony_covered_feet_1':
            new_name = 'Eve Covered Feet'
        if old_name != new_name:
            raise ValueError('Unexpected body/hair material reassignment')
        for old_wedge, new_wedge in zip(old[:3], new[:3], strict=True):
            before, after = source['wedges'][old_wedge], target['wedges'][new_wedge]
            if after != [point_map[before[0]], *before[1:]]:
                raise ValueError('Body/hair corner topology or UV changed')
            if any(source[key][old_wedge] != target[key][new_wedge] for key in ('normals', 'colors')):
                raise ValueError('Body/hair corner attributes changed')
    expected = [[point_map[p], b, w] for p, b, w in source['influences'] if p in point_map]
    actual = [r for r in target['influences'] if r[0] < len(point_map)]
    if expected != actual:
        raise ValueError('Body/hair weights changed')
    shapes = {m['name']: m['deltas'] for m in target['morph_targets']}
    for morph in source['morph_targets']:
        expected = [[point_map[r[0]], *r[1:]] for r in morph['deltas'] if r[0] in point_map]
        actual = [r for r in shapes[morph['name']] if r[0] < len(point_map)]
        if expected != actual:
            raise ValueError('Body/hair morph changed: ' + morph['name'])
    return dict(points=len(point_map), faces=len(face_ids), morphs=len(source['morph_targets']),
                bones=len(source['bones']), ordered_corners_weights_morphs_equal=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('manifest', 'source-mesh', 'source-audit', 'mesh', 'assembly',
                 'bindings', 'wardrobe', 'footwear', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    paths = {key: value for key, value in vars(args).items() if key != 'output'}
    hashes = {str(p.resolve()): sha(p) for p in paths.values()}
    data = {key: json.loads(value.read_text()) for key, value in paths.items()}
    manifest, source, target = data['manifest'], data['source_mesh'], data['mesh']
    audit, assembly = data['source_audit'], data['assembly']
    if sha(args.source_mesh) != audit['output_sha256']:
        raise ValueError('Source interchange no longer matches its audit')
    for path in (args.source_mesh, args.source_audit):
        if assembly['inputs'][str(path.resolve())] != sha(path):
            raise ValueError('Assembly was made from a different body input')
    proof = verify_body(source, audit, target, assembly)
    catalog = copy.deepcopy(manifest['catalog'])
    outfit, = catalog['outfits']
    if manifest['id'] != 'eins0fx.seduxtress' or outfit['id'] != manifest['id']:
        raise ValueError('Expected the current Eve package')
    donor, = [v for v in outfit['variants'] if v['id'] == 'skin_suit']
    if any(v['id'] == 'peony' for v in outfit['variants']):
        raise ValueError('Peony already exists')
    names = target['materials']
    slot_map = {old: names.index(name) for old, name in enumerate(source['materials']) if name in names}
    retained = [copy.deepcopy(c) for c in donor['customize']['controls'] if c.get('group') == 'body']
    hair = copy.deepcopy(next(c for c in donor['customize']['controls'] if c['id'] == 'hair'))
    hair.update(group='body', role='hair', sections=[names.index(name) for name in
        ('MI_EVE_HR_01_TypeA.001', 'MI_EVE_HR_01_Tail_TypeA', 'CH_W_Sword_02_Blade_Edge.001')])
    retained.insert(0, hair)
    selected_ids = {c['id'] for c in retained}
    controls = copy.deepcopy(data['wardrobe'])
    if selected_ids & {c['id'] for c in controls['controls']}:
        raise ValueError('Wardrobe and body control identities overlap')
    controls['controls'].extend(retained)
    bindings = data['bindings']['bindings']
    if [(r['slot'], r['name']) for r in bindings] != list(enumerate(names)):
        raise ValueError('Bindings do not identify the target slots in order')
    aliases = {names.index('Peony_covered_feet_1'): names.index('Eve Covered Feet'),
               names.index('Peony_boots_Lining'): names.index('Eve Legs')}
    for new, original in aliases.items():
        if bindings[new]['material'] != bindings[original]['material']:
            raise ValueError('A dye alias uses a different material')
    for original in donor['customize']['surfaces']:
        layers = {k: v for k, v in original['layers'].items() if k in selected_ids}
        if not layers:
            continue
        surface = copy.deepcopy(original)
        slots = {slot_map[s] for s in original['slots'] if s in slot_map}
        slots.update(new for new, previous in aliases.items() if previous in slots)
        if not slots:
            raise ValueError('Body dye surface lost all destinations')
        surface.update(slots=sorted(slots), layers=layers)
        controls['surfaces'].append(surface)
    peony_resources = validate(controls)
    check_shapes(controls, {m['name'] for m in target['morph_targets']})
    lift = data['footwear']['required_boot_lift_cm']
    if not 0 < lift <= 10:
        raise ValueError('Measured boot lift exceeds CSS placement range')
    package = target['mesh_package']
    if not package.startswith('/Game/CSS/eins0fx/EveSB/Peony/Meshes/'):
        raise ValueError('Unexpected Peony mesh namespace')
    variant = dict(id='peony', name='Peony', mesh=package + '.' + package.rsplit('/', 1)[1],
                   customize=controls, animations=copy.deepcopy(donor['animations']),
                   ground_offset_toggle=dict(control='toggle_boots', shown_cm=lift, hidden_cm=0))
    outfit['variants'].append(variant)
    if outfit['variants'][:-1] != manifest['catalog']['outfits'][0]['variants']:
        raise ValueError('Existing Eve variants changed')
    resources = set(peony_resources)
    for old in outfit['variants'][:-1]:
        resources.update(validate(old['customize']))
    if outfit.get('customize'):
        resources.update(validate(outfit['customize']))
    if resources != manifest['resources'].keys():
        raise ValueError('Full catalog dye resources differ from the existing Eve package')
    for name in resources:
        if resource_info(args.manifest.parent / name) != manifest['resources'][name]:
            raise ValueError('Existing dye resource changed: ' + name)
    if hashes != {str(p.resolve()): sha(p) for p in paths.values()}:
        raise ValueError('An input changed during assembly')
    artwork = []
    if outfit.get('thumbnail'):
        if outfit['thumbnail'] != 'thumbnail.png':
            raise ValueError('Unexpected outfit artwork path')
        artwork.append(args.manifest.parent / 'thumbnail.png')
    hashes.update({str(p.resolve()): sha(p) for p in artwork})
    args.output.mkdir(parents=True)
    for name in resources:
        shutil.copy2(args.manifest.parent / name, args.output / name)
    for path in artwork:
        shutil.copy2(path, args.output / path.name)
        if sha(args.output / path.name) != hashes[str(path.resolve())]:
            raise ValueError('Copied artwork differs from the original')
    for filename, payload in [('eve.css.json', catalog), ('peony-variant.json', variant),
                              ('peony-customize.json', controls), ('resources.json', {
                                  name: manifest['resources'][name] for name in sorted(resources)})]:
        (args.output / filename).write_text(json.dumps(payload, indent=2) + '\n')
    receipt = dict(inputs=hashes, builder_sha256=sha(Path(__file__)), body_proof=proof,
        controls=len(controls['controls']), surfaces=len(controls['surfaces']), palettes=len(controls['palettes']),
        dye_resources=len(resources), peony_dye_resources=len(peony_resources),
        inherited_control_ids=sorted(selected_ids),
        slot_map=slot_map, dye_aliases=aliases, existing_six_variants_unchanged=True,
        ground_offset_toggle=variant['ground_offset_toggle'], installed=False,
        outputs={p.name: sha(p) for p in args.output.iterdir() if p.is_file()},
        pending=['Body/hair material and physics runtime validation on this new variant.',
                 'Final fit, cooking, new container revisions and facial adapter registration.',
                 'Astral material support, gameplay and measured performance.'])
    (args.output / 'assembly-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({k: receipt[k] for k in ('controls', 'surfaces', 'palettes', 'dye_resources', 'body_proof')}))


if __name__ == '__main__':
    main()
