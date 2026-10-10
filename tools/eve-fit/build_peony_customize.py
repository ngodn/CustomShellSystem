"""Build Peony's wardrobe controls from its final material-slot bindings.

Python 3.14. This fragment must be merged with the verified Eve body/hair
controls before packaging. It does not edit a manifest or install game files.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from css_controls import lint_convention, validate


PARTS = {
    'jacket': 'Jacket', 'corset': 'Corset', 'choker': 'Choker',
    'gloves': 'Gloves', 'arm_ornaments': 'Arm ornaments',
    'hip_accessories': 'Hip accessories', 'underwear': 'Underwear',
    'leg_panels': 'Leg panels', 'stockings': 'Stockings', 'boots': 'Boots',
}
FAMILIES = {'Upper', 'Arm', 'Lower_1', 'Lower_2', 'Panels', 'Stockings'}
FABRIC = {'Upper', 'Lower_2', 'Panels'}
COLOR_FIELDS = (
    ('garment_color', 'Fabric color', 'garment', 'FabricColor', 'fabric', FABRIC),
    ('trim_color', 'Trim color', 'accent', 'TrimColor', 'trim', FAMILIES - {'Stockings'}),
    ('metal_color', 'Metal color', 'metal', 'HardwareColor', 'hardware', FAMILIES - {'Stockings'}),
    ('stocking_color', 'Stocking color', 'garment', 'StockingColor', 'stockings', {'Stockings'}),
    ('glow_color', 'Trim glow color', 'glow', 'GlowColor', 'accent', FABRIC),
)


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def rgba(value: str) -> list[float]:
    if len(value) != 6:
        raise ValueError('Expected a six-digit sRGB color')
    return [int(value[i:i + 2], 16) / 255 for i in (0, 2, 4)] + [1.0]


def build(mesh: dict, plan: dict, palettes: dict, footwear: dict) -> tuple[dict, dict]:
    rows = plan['bindings']
    if [r['slot'] for r in rows] != list(range(len(mesh['materials']))):
        raise ValueError('Bindings must describe every material slot in order')
    if [r['name'] for r in rows] != mesh['materials']:
        raise ValueError('Mesh and binding material names differ')
    counts = Counter(f[3] for f in mesh['faces'])
    empty = [r['name'] for r in rows if counts[r['slot']] == 0]
    if set(empty) != {'Peony_leg_panels_Arm', 'Peony_leg_panels_Lower_1'}:
        raise ValueError('Unexpected empty slots after merging panel trim into cloth')
    owners = {key: [] for key in PARTS}
    families = {key: [] for key in FAMILIES}
    for row in rows:
        name, slot = row['name'], row['slot']
        if not counts[slot]:
            continue
        if not name.startswith('Peony_') or name.startswith('Peony_covered_feet_'):
            continue
        matches = [key for key in PARTS if name.startswith('Peony_' + key + '_')]
        if len(matches) != 1:
            raise ValueError('Unowned Peony section: ' + name)
        owners[matches[0]].append(slot)
        if name == 'Peony_boots_Lining':
            continue
        prefix = '/Game/CSS/eins0fx/EveSB/Peony/Materials/Palettes/MI_peony_monochrome_'
        package, separator, object_name = row['material'].partition('.')
        if not separator or not package.startswith(prefix) or object_name != package.rsplit('/', 1)[1]:
            raise ValueError('Unexpected Peony material: ' + row['material'])
        family = package.removeprefix(prefix)
        if family not in families:
            raise ValueError('Unknown Peony material family: ' + family)
        families[family].append(slot)
    if any(not slots for slots in owners.values()) or any(not slots for slots in families.values()):
        raise ValueError('Missing wardrobe part or material family')
    boot, = footwear['controls']
    if set(owners['boots']) != set(boot['sections']):
        raise ValueError('Boot sections no longer match the exact-face coverage recipe')
    covered = set(boot['occludes_sections'])
    if covered & {i for slots in owners.values() for i in slots}:
        raise ValueError('Boot visibility would occlude another garment')
    if {i for i, face in enumerate(mesh['faces']) if face[3] in covered} != set(footwear['covered_original_faces']):
        raise ValueError('Covered-foot face identities changed')
    controls = []
    for key, label in PARTS.items():
        control = dict(id='toggle_' + key, name=label, kind='toggle', group='outfit',
                       role='piece', default=[1, 0, 0, 1], sections=owners[key])
        if key == 'boots':
            control['occludes_sections'] = sorted(covered)
        controls.append(control)

    def bindings(parameter, targets):
        return [dict(slot=slot, parameter=parameter) for slot in
                sorted(i for family in targets for i in families[family])]

    first = palettes['palettes'][0]
    if first['id'] != 'peony_monochrome' or len(palettes['palettes']) != 5:
        raise ValueError('Expected Monochrome first and five authored palettes')
    for identifier, label, role, parameter, key, targets in COLOR_FIELDS:
        controls.append(dict(id=identifier, name=label, kind='color', role=role,
                             group='outfit', default=rgba(first[key]),
                             bindings=bindings(parameter, targets)))
    scalars = (
        ('trim_glow', 'Trim glow', 'glow', 'glow', 'GlowStrength', palettes['trim_glow']['strength'], 32, .1, FABRIC),
        ('glow_coverage', 'Glow coverage', 'scalar', 'glow', 'GlowFill', palettes['trim_glow']['coverage'], 1, .01, FABRIC),
        ('flow_speed', 'Flow speed', 'scalar', 'glow', 'FlowSpeed', palettes['trim_glow']['flow_speed'], 4, .01, FABRIC),
        ('stocking_opacity', 'Stocking opacity', 'opacity', 'garment', 'StockingOpacity', palettes['stocking_opacity'], 1, .01, {'Stockings'}),
        ('fabric_roughness', 'Fabric roughness', 'scalar', 'garment', 'FabricRoughness', .42, 1, .01, FABRIC),
        ('trim_roughness', 'Trim roughness', 'scalar', 'accent', 'TrimRoughness', .24, 1, .01, FABRIC),
        ('stocking_roughness', 'Stocking roughness', 'scalar', 'garment', 'StockingRoughness', .42, 1, .01, {'Stockings'}),
    )
    for identifier, label, kind, role, parameter, default, maximum, step, targets in scalars:
        controls.append(dict(id=identifier, name=label, kind=kind, role=role, group='outfit',
                             default=[default, 0, 0, 1], min=0, max=maximum, step=step,
                             bindings=bindings(parameter, targets)))
    templates = [dict(id=p['id'], name=p['name'], values={
        identifier: rgba(p[key]) for identifier, _, _, _, key, _ in COLOR_FIELDS
    }) for p in palettes['palettes']]
    recipe = dict(schema=1, controls=controls, surfaces=[], palettes=templates)
    if validate(recipe):
        raise ValueError('Wardrobe fragment unexpectedly needs dye resources')
    warnings = lint_convention(recipe)
    if warnings:
        raise ValueError('; '.join(warnings))
    return recipe, dict(section_owners=owners, material_families=families,
                        empty_merged_slots=empty,
                        covered_foot_sections=sorted(covered),
                        covered_foot_faces=len(footwear['covered_original_faces']))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('mesh', 'bindings', 'palettes', 'footwear', 'output'):
        parser.add_argument('--' + key, required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    paths = [args.mesh, args.bindings, args.palettes, args.footwear]
    hashes = {str(p.resolve()): sha(p) for p in paths}
    recipe, ownership = build(*(json.loads(p.read_text()) for p in paths))
    if hashes != {str(p.resolve()): sha(p) for p in paths}:
        raise ValueError('An input changed during generation')
    args.output.mkdir(parents=True)
    target = args.output / 'wardrobe-customize.json'
    target.write_text(json.dumps(recipe, indent=2, allow_nan=False) + '\n')
    receipt = dict(inputs=hashes, builder_sha256=sha(Path(__file__)),
                   output_sha256=sha(target), controls=len(recipe['controls']),
                   palettes=len(recipe['palettes']), ownership=ownership,
                   color_space='sRGB; CSS converts vectors to linear before material writes',
                   limits=['Wardrobe fragment only; merge verified body, hair and physics controls.',
                           'Boot-dependent ground height is not implemented by this fragment.',
                           'Native parameter readback, full catalog, cook and gameplay checks remain.'])
    (args.output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(dict(controls=receipt['controls'], palettes=receipt['palettes'], output=str(target))))


if __name__ == '__main__':
    main()
