"""Compare migrated cooked rigs to V44, with explicit package/symbol renaming."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    args = parser.parse_args()
    work = args.work.resolve()
    output = work / 'cooked-validation.json'
    if output.exists():
        raise FileExistsError(output)
    load = lambda p: json.loads(p.read_text())
    for label in ('cook', 'pack', 'verify', 'decode2', 'verify3'):
        assert load(work / (label + '-exit.json'))['exit_code'] == 0, label
    assets = load(work / 'assets.json')
    mapping = load(work / 'map.json')
    packages = (work / 'cook.txt').read_text().splitlines()
    assert len(packages) == len(set(packages)) == 7
    names = {old.rsplit('/', 1)[1]: new.rsplit('/', 1)[1]
             for old, new in mapping.items()
             if old.rsplit('/', 1)[1] != new.rsplit('/', 1)[1]}

    def normalized(value):
        if isinstance(value, dict):
            pairs = [(normalized(k), normalized(v)) for k, v in value.items()]
            result = dict(pairs)
            assert len(result) == len(pairs), 'Renamed dictionary key collision'
            return result
        if isinstance(value, list):
            return [normalized(v) for v in value]
        if isinstance(value, str):
            for old, new in sorted(mapping.items(), key=lambda p: -len(p[0])):
                value = value.replace(old, new)
            # Generated class/function names include the authored asset leaf.
            for old, new in sorted(names.items(), key=lambda p: -len(p[0])):
                value = value.replace(old, new)
        return value

    compared = []
    for old, new in mapping.items():
        if old in assets and new in assets:
            assert normalized(assets[old]) == assets[new], new
            compared.append(new)
    assert set(compared) == set(packages) - {'/Game/CSS/SeduXtress/SK_BlackPearl'}
    mesh = next(x for x in assets['/Game/CSS/SeduXtress/SK_BlackPearl'] if x['Type'] == 'SkeletalMesh')
    props = mesh['Properties']
    for field, package in {
        'Skeleton': '/Game/CSS/Shared/SKEL_Base',
        'PhysicsAsset': '/Game/CSS/SeduXtress/PA_Body',
        'PostProcessAnimBlueprint': '/Game/CSS/SeduXtress/ABP_Secondary',
    }.items():
        assert props[field]['ObjectPath'].rsplit('.', 1)[0] == package, field
    assert len(mesh['SkeletalMaterials']) == 30 and len(props['MorphTargets']) == 22
    material_packages = {new for old, new in mapping.items() if old.rsplit('/', 1)[1].startswith('MI_')}
    assert len(material_packages) == 30
    available = set(packages) | material_packages
    dependencies = set()
    for package in packages:
        text = json.dumps(assets[package])
        assert '/Game/CSSAuthoring/' not in text, package
        refs = {s.split('.', 1)[0] for s in re.findall(r'/Game/CSS/[^\s\"\x27]+', text)}
        assert refs <= available, (package, refs - available)
        dependencies.update(refs)
    protected = load(work / 'verified.json')['protected_hashes']
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
    result = dict(passed=True, compared_packages=sorted(compared), decoded_packages=len(assets),
                  runtime_packages=7, mesh_materials=30, mesh_morphs=22,
                  css_dependencies=sorted(dependencies), protected_files=len(protected),
                  scope='Full decoded equality for six rig assets after declared renaming; mesh bindings and CSS references only. Mesh geometry, moving dynamics and live behavior remain separate.')
    output.write_text(json.dumps(result, indent=2) + '\n')
    print('Cooked rig equality and mesh bindings pass')


if __name__ == '__main__':
    main()
