"""Build V2 controls from cooked material recipes and the accepted Eve catalog.

Python 3.14. Produces metadata and a texture dependency plan, without installing.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from css_controls import validate, lint_convention, resource_info
from build_peony_v2_palettes import build as build_palettes

PARTS = {
    'jacket': ('Jacket', [19, 22, 23]),
    'corset': ('Corset', [20, 24, 25, 26, 27]),
    'choker': ('Choker', [21]), 'gloves': ('Gloves', [3, 14]),
    'underwear': ('Underwear', [10]), 'leg_panels': ('Thigh panels', [8]),
    'hip_bands': ('Hip bands', [9]), 'hip_straps': ('Hip straps', [11]),
    'boots': ('Boots', [4, 17, 30]),
}
COLOR_GROUPS = (
    ('jacket_color', 'Jacket color', 'BaseTint', [19, 20, 21, 22, 23]),
    ('corset_color', 'Corset color', 'BaseTint', [24]),
    ('lacing_color', 'Lacing and trim color', 'BaseTint', [25, 26, 27]),
    ('panel_color', 'Thigh panel color', 'BaseTint', [8]),
    ('band_color', 'Hip band color', 'BaseTint', [9]),
    ('strap_color', 'Hip strap color', 'BaseTint', [11]),
    ('underwear_color', 'Underwear color', 'BaseTint', [10]),
    ('glove_color', 'Glove color', 'BaseTint', [3]),
    ('calf_color', 'Boot shaft color', 'BaseTint', [4]),
    ('heel_color', 'Heel color', 'BaseTint', [30]),
    ('upper_pattern_color', 'Upper pattern color', 'PatternTint', list(range(19, 28))),
    ('lower_pattern_color', 'Lower pattern color', 'PatternTint', [8, 10]),
    ('strap_pattern_color', 'Strap pattern color', 'PatternTint', [9, 11]),
    ('boot_pattern_color', 'Boot pattern color', 'PatternTint', [4, 30]),
    ('glove_pattern_color', 'Glove pattern color', 'PatternTint', [3]),
)
PART_NAMES = {3: 'Gloves', 4: 'Boot shafts', 8: 'Thigh panels', 9: 'Hip bands',
              10: 'Underwear', 11: 'Hip straps', 19: 'Jacket', 20: 'Front fastenings',
              21: 'Choker', 22: 'Shoulder cords', 23: 'Shoulder trim', 24: 'Corset',
              25: 'Corset trim', 26: 'Corset lacing', 27: 'Corset edging', 30: 'Heels'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def srgb(value):
    return [12.92*x if x <= .0031308 else 1.055*x**(1/2.4)-.055 for x in value[:3]] + value[3:]


def build(materials, bindings, assembly, source, stockings, donor):
    recipe = build_palettes(materials, bindings)
    controls, palettes = recipe['controls'], recipe['palettes']
    rows = bindings['bindings']
    slots = {int(re.fullmatch(r'PeonyV2_part_(\d+)_Part\d+', r['name'])[1]): r['slot']
             for r in rows if r['owner'] == 'peony'}
    instances = {(r['palette'], r['slot']): r for r in materials['instances']}
    default = 'black_white_floral'
    dependency_plan = {}

    def toggle(identifier, label, sections, visible=True, occludes=()):
        row = dict(id='toggle_'+identifier, name=label, kind='toggle', group='outfit',
                   role='piece', default=[int(visible), 0, 0, 1], sections=sections)
        if occludes:
            row['occludes_sections'] = list(occludes)
        controls.append(row)

    def values(palette, parts, field, parameter):
        result = [instances[(palette, part)][field][parameter] for part in parts]
        if any(value != result[0] for value in result):
            raise ValueError(f'Linked control would flatten source settings: {palette}, {parameter}, {parts}')
        return result[0]

    def material_control(identifier, label, kind, role, parameter, parts, maximum=1, preserve=False):
        color = kind == 'color'
        field = 'colors' if color else 'scalars'
        def converted(palette):
            value = values(palette, parts, field, parameter)
            if color:
                result = srgb(value)
                if parameter == 'SourceGlowColor':
                    # The opaque SourceGlow graph consumes RGB only. Keep CSS's
                    # protected alpha fixed while preserving the source HDR color.
                    result[3] = values(default, parts, field, parameter)[3]
                return result
            return [value, 0, 0, 1]
        original = converted(default)
        controls.append(dict(id=identifier, name=label, kind=kind, role=role, group='outfit',
                             default=original, min=0, max=maximum,
                             bindings=[dict(slot=slots[p], parameter=parameter) for p in parts]))
        for palette in palettes:
            value = converted(palette['id'])
            if preserve:
                if value != original:
                    raise ValueError('Cannot preserve a manual setting with different palette baselines')
                targets = {slots[p] for p in parts}
                palette['parameters'] = [r for r in palette['parameters']
                    if not (r['slot'] in targets and r['parameter'] == parameter)]
            else:
                palette['values'][identifier] = value

    coverage = assembly['footwear_coverage']
    boot_skin = [row['slot'] for row in coverage['aliases']]
    stocking_start = assembly['stocking_offsets']['materials']
    legs = next(r for r in assembly['stocking_layers'] if r['region'] == 'legs')
    boot_stocking = stocking_start + legs['boot_covered_slot']
    for identifier, (name, parts) in PARTS.items():
        targets = [slots[p] for p in parts]
        if identifier == 'boots' and set(targets) != set(coverage['boot_material_slots']):
            raise ValueError('Boot ownership differs from the measured coverage')
        toggle(identifier, name, targets, occludes=boot_skin+[boot_stocking] if identifier == 'boots' else ())
    for layer in assembly['stocking_layers']:
        sections = [stocking_start + layer['material_slot']]
        if layer.get('boot_covered_slot') is not None:
            sections.append(boot_stocking)
        toggle('stockings' if layer['region'] == 'legs' else 'stockings_'+layer['region'],
               {'legs': 'Stockings', 'body': 'Sheer body layer', 'arms': 'Sheer sleeves', 'hands': 'Sheer gloves'}[layer['region']],
               sections, layer['default_visible'])

    for identifier, label, parameter, parts in COLOR_GROUPS:
        material_control(identifier, label, 'color', 'garment', parameter, parts)
    flow_parts = sorted(r['slot'] for r in materials['instances'] if r['palette'] == default and r['material_kind'].startswith('Flow'))
    material_control('glow_color', 'Trim glow color', 'color', 'glow', 'GlowColor', flow_parts)
    for identifier, label, parameter, maximum in (
        ('trim_glow', 'Trim glow', 'GlowStrength', 32),
        ('glow_coverage', 'Glow coverage', 'GlowFill', 1),
        ('flow_speed', 'Flow speed', 'FlowSpeed', 4)):
        material_control(identifier, label, 'glow' if identifier == 'trim_glow' else 'scalar',
                         'glow', parameter, flow_parts, maximum, preserve=True)
    material_control('hardware_glow_color', 'Glove and heel glow color', 'color', 'glow', 'SourceGlowColor', [14, 17], 16)
    material_control('hardware_glow', 'Glove and heel glow', 'intensity', 'glow', 'SourceGlowStrength', [14, 17], 4, preserve=True)
    stocking_slots = [r['slot'] for r in rows if r['owner'] == 'peony_stocking']
    for identifier, label, parameter, kind in (
        ('stocking_color', 'Stocking color', 'StockingTint', 'color'),
        ('stocking_opacity', 'Stocking opacity', 'StockingOpacity', 'opacity'),
        ('stocking_roughness', 'Stocking roughness', 'StockingRoughness', 'scalar')):
        value = srgb(stockings['colors'][parameter]) if kind == 'color' else [stockings['scalars'][parameter], 0, 0, 1]
        controls.append(dict(id=identifier, name=label, kind=kind, group='outfit', role='garment',
                             default=value, bindings=[dict(slot=s, parameter=parameter) for s in stocking_slots]))
        if kind == 'color':
            for palette in palettes:
                palette['values'][identifier] = value

    for source_control in source['source_controls']['TextureOptions']:
        part = source_control['MaterialIndex']
        parameter = {'Basecolor': 'BaseTexture', 'ORM': 'ORMTexture', 'ML_RS_Pattern': 'PatternTexture'}[source_control['ParamName']]
        sampler = 'COLOR' if parameter == 'BaseTexture' else 'MASKS'
        options = []
        for name, texture in zip(source_control['OptionNames'], source_control['resolved_textures'], strict=True):
            package = '/Game/CSS/eins0fx/EveSB/PeonyV2/Textures/T_V2_'+texture['sha256'][:20]+'_'+sampler
            path = package+'.'+package.rsplit('/', 1)[1]
            dependency_plan[path] = dict(source=texture['file'], sha256=texture['sha256'], sampler=sampler)
            if path not in [o['texture'] for o in options]:
                options.append(dict(name=name, texture=path))
        for palette in palettes:
            path = instances[(palette['id'], part)]['textures'][parameter]
            if path not in [o['texture'] for o in options]:
                options.append(dict(name=palette['name'], texture=path))
        if len(options) == 1:
            continue
        identifier = f'texture_{part}_{parameter.lower()}'
        def index(palette_id):
            path = instances[(palette_id, part)]['textures'][parameter]
            return next(i for i, option in enumerate(options) if option['texture'] == path)
        suffix = {'BaseTexture': 'finish', 'ORMTexture': 'shine', 'PatternTexture': 'pattern'}[parameter]
        controls.append(dict(id=identifier, name=PART_NAMES[part]+' '+suffix, kind='choice', role='pattern',
                             group='outfit', default=[index(default), 0, 0, 1], options=options,
                             bindings=[dict(slot=slots[part], parameter=parameter)]))
        for palette in palettes:
            palette['values'][identifier] = [index(palette['id']), 0, 0, 1]

    body_map = {old: new for new, old in enumerate(assembly['body_source_slots'])}
    retained = copy.deepcopy([c for c in donor['customize']['controls'] if c.get('group') == 'body'])
    hair = copy.deepcopy(next(c for c in donor['customize']['controls'] if c['id'] == 'hair'))
    hair.update(group='body', role='hair', sections=[r['slot'] for r in rows if r['name'] in
        ('MI_EVE_HR_01_TypeA.001', 'MI_EVE_HR_01_Tail_TypeA', 'CH_W_Sword_02_Blade_Edge.001')])
    if len(hair['sections']) != 3:
        raise ValueError('Hair and ponytail attachment slots changed')
    for control in retained:
        for binding in control.get('bindings', []):
            binding['slot'] = body_map[binding['slot']]
        for key in ('sections', 'occludes_sections'):
            if key in control:
                control[key] = [body_map[s] for s in control[key]]
    controls.extend([hair, *retained])
    retained_ids = {c['id'] for c in retained}
    recipe['surfaces'] = []
    for original in donor['customize']['surfaces']:
        layers = {k: v for k, v in original['layers'].items() if k in retained_ids}
        if not layers:
            continue
        targets = {body_map[s] for s in original['slots'] if s in body_map}
        targets.update(r['slot'] for r in coverage['aliases'] if r['original_slot'] in targets)
        if not targets:
            raise ValueError('A retained body dye surface lost its target')
        recipe['surfaces'].append(dict(original, slots=sorted(targets), layers=layers))
    return recipe, dependency_plan


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('materials', 'bindings', 'assembly', 'source', 'stockings', 'manifest', 'output'):
        parser.add_argument('--'+key, type=Path, required=True)
    args = parser.parse_args()
    paths = {k: p for k, p in vars(args).items() if k != 'output'}
    hashes = {str(p): digest(p) for p in paths.values()}
    data = {k: json.loads(p.read_text()) for k, p in paths.items()}
    manifest = data.pop('manifest')
    donor = next(v for v in manifest['catalog']['outfits'][0]['variants'] if v['id'] == 'skin_suit')
    recipe, dependencies = build(**data, donor=donor)
    print('Controls:', len(recipe['controls']))
    resources = validate(recipe)
    warnings = lint_convention(recipe)
    if warnings:
        raise ValueError('; '.join(warnings))
    for name in resources:
        if resource_info(args.manifest.parent/name) != manifest['resources'][name]:
            raise ValueError('Accepted dye resource changed: '+name)
    native_packages = set(data['materials']['packages'])
    missing = {path: row for path, row in dependencies.items() if path.split('.')[0] not in native_packages}
    if hashes != {str(p): digest(p) for p in paths.values()}:
        raise ValueError('An input changed during generation')
    args.output.mkdir(parents=True, exist_ok=True)
    def write(name, value):
        (args.output/name).write_text(json.dumps(value, indent=2, allow_nan=False)+'\n')
    write('customize.json', recipe)
    write('texture-dependencies.json', dict(all=dependencies, pending_import=missing))
    write('variant.json', dict(id='peony', name='Peony',
          mesh='/Game/CSS/eins0fx/EveSB/PeonyV2/Meshes/SK_PeonyV2.SK_PeonyV2', customize=recipe,
          animations=donor['animations'], ground_offset_toggle=dict(control='toggle_boots',
          shown_cm=data['assembly']['footwear_coverage']['boot_ground_offset_cm'], hidden_cm=0)))
    write('customize-receipt.json', dict(inputs=hashes, script_sha256=digest(Path(__file__)),
          controls=len(recipe['controls']), palettes=len(recipe['palettes']), resources=sorted(resources),
          pending_texture_imports=len(missing), output_sha256=digest(args.output/'customize.json'),
          limitation='Metadata candidate only. Pending texture imports, native binding checks, final package and runtime acceptance remain.'))


if __name__ == '__main__':
    main()
