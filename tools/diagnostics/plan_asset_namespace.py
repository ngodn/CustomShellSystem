"""Map the audited installation to author-owned packages without modifying assets."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
AUTHOR = '/Game/CSS/eins0fx/'
EVE = 'CSS_EveStellarBlade_eins0fx_P'
GENESSA = 'CSS_UnholyGenessa_eins0fx_P'
COMMANDER = 'CSS_CommanderWhite_eins0fx_P'
SHARED = 'CSS_SharedAssets_P'

# Most specific prefixes go first. Names inside a package are preserved.
ROUTES = (
    ('Shared/SKEL_Base', 'Shared/Skeletons/SKEL_Base', SHARED),
    ('UnholyGenessa/SKEL_Wings2', 'UnholyGenessa/Skeletons/SKEL_Wings2', GENESSA),
    ('Locomotion/Faces/SK_Face_Jali3V1_', 'EveSB/Faces/SK_Face_Jali3V1_', EVE),
    ('Locomotion/Faces/SK_Face_CWCompact/', 'CommanderWhite/Faces/', COMMANDER),
    ('Locomotion/Faces/Optical1/', 'CommanderWhite/Faces/Optical/', COMMANDER),
    ('Locomotion/Adapters/ABP_Eve', 'EveSB/Animations/Adapters/ABP_Eve', EVE),
    ('Locomotion/Adapters/ABP_EveBikini', 'EveSB/Animations/Adapters/ABP_EveBikini', EVE),
    ('Locomotion/Adapters/ABP_EveKnit', 'EveSB/Animations/Adapters/ABP_EveKnit', EVE),
    ('Locomotion/Adapters/ABP_Genessa', 'UnholyGenessa/Animations/Adapters/ABP_Genessa', GENESSA),
    ('Locomotion/Adapters/ABP_CommanderWhite', 'CommanderWhite/Animations/Adapters/ABP_CommanderWhite', COMMANDER),
    ('Locomotion/', 'Shared/Locomotion/', SHARED),
    ('SharedAssets/Astral/', 'Shared/Astral/', SHARED),
    ('Eve/Anim/', 'Shared/Locomotion/EveLegacy/', SHARED),
    ('CW1/Anim/', 'Shared/Locomotion/CommanderWhiteLegacy/', SHARED),
    ('SeduXtress/', 'EveSB/', EVE),
    ('EveTest/', 'EveSB/Fit/', EVE),
    ('EveHair/', 'EveSB/Hair/', EVE),
    ('UnholyGenessa/', 'UnholyGenessa/', GENESSA),
    ('CommanderWhite/Dev/', 'CommanderWhite/Meshes/', COMMANDER),
    ('CommanderWhite/', 'CommanderWhite/', COMMANDER),
    ('CWTest/', 'CommanderWhite/Rig/', COMMANDER),
)


def destination(package):
    if not package.startswith('/Game/CSS/'):
        raise ValueError(f'Unexpected package: {package}')
    relative = package.removeprefix('/Game/CSS/')
    for old, new, owner in ROUTES:
        if relative == old or (old.endswith(('/', '_')) and relative.startswith(old)):
            if old == 'Locomotion/' and relative.startswith('Locomotion/Faces/'):
                raise ValueError(f'Facial asset needs an explicit character owner: {package}')
            if old == 'Locomotion/' and relative.startswith('Locomotion/Adapters/'):
                raise ValueError(f'Playback adapter needs an explicit character owner: {package}')
            return AUTHOR + new + relative[len(old):], owner
    # Older cooked relocation used Z padding. Retain slot/hash identity to avoid
    # merging materials or textures with the same basename but different data.
    material = re.fullmatch(r'(C[WX]Material\d+)Z+/([^/]+)', relative)
    texture = re.fullmatch(r'(CWT(?:BRM|BC|N)[0-9a-f]{10})Z+/([^/]+)', relative)
    if material:
        return AUTHOR + 'CommanderWhite/Materials/' + '/'.join(material.groups()), COMMANDER
    if texture:
        return AUTHOR + 'CommanderWhite/Textures/' + '/'.join(texture.groups()), COMMANDER
    raise ValueError(f'No explicit ownership route: {package}')


def plan(baseline, conflicts):
    providers = defaultdict(list)
    for container in baseline['packages']:
        for export in container['exports']:
            providers[export['package']].append({
                'container': container['container'],
                'indexed_chunk_hash': export.get('indexed_chunk_hash') or export['index_row'].split()[2],
            })
    repaired = set()
    for sequence in conflicts['sequences']:
        if (sequence['old_tail'] != list(range(379, 388))
                or sequence['corrected_tail'] != list(range(386, 395))
                or not sequence['body_mapping_equal']
                or not sequence['all_other_decoded_fields_equal']):
            raise ValueError(f'Unrecognized conflict evidence: {sequence["package"]}')
        repaired.add(sequence['package'])
    if len(repaired) != 19:
        raise ValueError('Expected the nineteen independently decoded mapping repairs')

    rows, destinations, selected_conflicts = [], set(), set()
    for old, choices in sorted(providers.items()):
        new, owner = destination(old)
        if new.casefold() in destinations:
            raise ValueError(f'Colliding target package: {new}')
        destinations.add(new.casefold())
        if len({p['indexed_chunk_hash'] for p in choices}) > 1:
            if old not in repaired or {p['container'] for p in choices} != {EVE, SHARED}:
                raise ValueError(f'Unresolved provider conflict: {old}')
            selected = next(p for p in choices if p['container'] == EVE)
            reason = 'Eve copy has the verified 386-394 virtual-track mapping'
            selected_conflicts.add(old)
        else:
            selected = next((p for p in choices if p['container'] == owner), choices[0])
            reason = 'Unique export' if len(choices) == 1 else 'Providers agree in indexed chunk hash'
        rows.append(dict(old_package=old, new_package=new, target_container=owner,
                         source=selected, providers=choices, selection_reason=reason))
    if selected_conflicts != repaired:
        raise ValueError('Conflict evidence does not match this installation')
    return rows


def catalog_references(catalog, rows):
    assets = {row['old_package']: row for row in rows}
    references = []

    def visit(value, pointer=''):
        if isinstance(value, dict):
            for key, child in value.items():
                token = key.replace('~', '~0').replace('/', '~1')
                visit(child, pointer + '/' + token)
        elif isinstance(value, list):
            for index, child in enumerate(value):
                visit(child, pointer + '/' + str(index))
        elif isinstance(value, str) and value.startswith('/Game/CSS/'):
            package, dot, object_name = value.partition('.')
            if package not in assets:
                raise ValueError(f'Unmapped catalog reference at {pointer}: {value}')
            asset = assets[package]
            # Alias paths identify old saved selections. Keep them as aliases
            # rather than replacing them with the new physical package path.
            is_alias = '/aliases/' in pointer
            references.append(dict(
                pointer=pointer, old_value=value,
                canonical_value=asset['new_package'] + dot + object_name,
                target_container=asset['target_container'],
                action='preserve_legacy_alias' if is_alias else 'rebuild_reference'))

    visit(catalog)
    return references


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--conflicts', type=Path, required=True)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT / 'work'):
        raise ValueError('Use a new output file under the CSS work directory')
    inputs = {}
    data = {}
    for name, path in (('baseline', args.baseline), ('conflicts', args.conflicts),
                       ('catalog', args.catalog)):
        raw = path.read_bytes()
        inputs[name] = dict(path=str(path.resolve()), sha256=hashlib.sha256(raw).hexdigest())
        data[name] = json.loads(raw)
    rows = plan(data['baseline'], data['conflicts'])
    references = catalog_references(data['catalog'], rows)
    counts = dict(sorted(Counter(r['target_container'] for r in rows).items()))
    report = dict(schema=1, stage='planning_only', inputs=inputs, target_counts=counts,
                  source_export_count=sum(len(p['exports']) for p in data['baseline']['packages']),
                  unique_package_count=len(rows), assets=rows,
                  catalog_references=references,
                  preserved_entry_ids=[entry['id'] for entry in
                                       data['catalog']['catalog']['animation_library']['entries']],
                  pending=['Editable-source provenance', 'Hard and soft reference rebuild',
                           'Catalog dependencies and saved-path compatibility',
                           'Cooked readback and old/new install validation'])
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(unique_packages=len(rows), target_counts=counts), indent=2))


if __name__ == '__main__':
    main()
