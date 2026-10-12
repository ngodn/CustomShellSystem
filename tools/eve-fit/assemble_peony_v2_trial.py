"""Assemble the verified V2 character and controls with the accepted six Eve outfits.

Python 3.14. Keeps installed CW/Genessa metadata and shared animation choices.
Creates a private candidate only; astral compatibility and gameplay are separate gates.
"""
import argparse
import copy
import os
from pathlib import Path
import shutil
import subprocess
import sys

from assemble_peony_trial import CSS, WORK, NAMES, OTHERS, read, write, sha, strings

sys.path.insert(0, str(CSS / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info
from css_facial_resources import encode, externalize, resolve
from css_package import verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('output', 'dotnet', 'mappings', 'character', 'options', 'customize', 'catalog-test'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    protected = {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        actual = sha(path)
        if expected is not None and actual != expected:
            raise ValueError('Changed input: ' + str(path))
        protected[str(path)] = actual
        return actual

    def run(label, command):
        command = list(map(str, command))
        write(out / (label + '-command.json'), command)
        env = os.environ.copy()
        env['DOTNET_ROOT'] = str(args.dotnet.parent)
        with (out / (label + '.log')).open('x') as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        write(out / (label + '-exit.json'), dict(returncode=result.returncode))
        result.check_returncode()

    for source in (Path(__file__), Path(__file__).with_name('assemble_peony_trial.py')):
        protect(source)
        shutil.copy2(source, out / source.name)
    protect(args.catalog_test)
    protect(args.mappings)
    customize = args.customize.resolve(strict=True)
    material_proof = read(customize / 'control-materials-complete.json')
    protect(customize / 'control-materials-complete.json')
    if not material_proof['passed'] or material_proof['pending_texture_imports']:
        raise ValueError('Customization materials are incomplete')
    for path, expected in material_proof['inputs'].items():
        protect(CSS / path, expected)
    variant = read(customize / 'variant.json')
    protect(customize / 'variant.json')
    if variant['id'] != 'peony' or variant['mesh'] != '/Game/CSS/eins0fx/EveSB/PeonyV2/Meshes/SK_PeonyV2.SK_PeonyV2':
        raise ValueError('Unexpected V2 identity')
    if variant['customize'] != read(customize / 'customize.json'):
        raise ValueError('Variant does not contain the checked customization')

    installed = read(WORK / 'peony-refit2/assembly.json')
    protect(WORK / 'peony-refit2/assembly.json')
    if not installed['passed']:
        raise ValueError('Missing installed baseline proof')
    original = GAME / 'Content/Paks/~mods'
    manifests, manifest_paths = {}, {}
    for name in (*NAMES, *OTHERS):
        for suffix in ('.pak', '.utoc', '.ucas'):
            relative = name + '/' + name + suffix
            protect(original / relative, installed['files'][relative])
        metadata = out / 'metadata' / name
        metadata.parent.mkdir(parents=True, exist_ok=True)
        run(name + '-unpack', [DEFAULT_REPAK, 'unpack', original / name / (name + '.pak'), '--output', metadata])
        path, = metadata.rglob('manifest.json')
        manifest = read(path)
        for suffix, info in manifest['containers'].items():
            source = original / name / (name + suffix)
            if sha(source) != info['sha256'] or source.stat().st_size != info['bytes']:
                raise ValueError('Installed metadata/container mismatch: ' + name)
        manifests[name], manifest_paths[name] = manifest, path

    payloads = {name: {} for name in NAMES}
    owners = {}

    def stage(name, relative, source, expected):
        relative = Path(relative)
        if relative.is_absolute() or '..' in relative.parts or relative.parts[:2] != ('MortalShell2', 'Content'):
            raise ValueError('Unsafe payload path')
        protect(source, expected)
        target = out / 'stage' / name / relative
        if target.exists():
            raise ValueError('Duplicate staged payload: ' + str(relative))
        target.parent.mkdir(parents=True, exist_ok=True)
        # These inputs are immutable and the converter only reads them.
        try:
            os.link(source, target)
        except OSError:
            subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)
        payloads[name][str(relative)] = expected
        if relative.suffix == '.uasset':
            package = asset_info(source.read_bytes())['package']
            if package in owners:
                raise ValueError('Duplicate package owner: ' + package)
            owners[package] = name

    for name, baseline in zip(NAMES, ('eve-final-container1', 'shared-cloth-native2'), strict=True):
        baseline = WORK / baseline
        proof = read(baseline / 'verification.json')
        protect(baseline / 'verification.json')
        if not proof['passed']:
            raise ValueError('Unverified baseline')
        for relative, expected in proof['payloads'].items():
            if installed['payloads'][name].get(relative) != expected:
                raise ValueError('Installed accepted payload differs: ' + relative)
            stage(name, relative, baseline / 'stage' / relative, expected)

    for cook in (args.character, args.options):
        proof = read(cook / 'cooked-readback1/verified.json')
        protect(cook / 'cooked-readback1/verified.json')
        if not proof['passed']:
            raise ValueError('Unverified V2 cook')
        source_root = cook / 'windows-cook1/project/Saved/Cooked/Windows/CSSShared/Content'
        for relative, expected in proof['payloads'].items():
            local = Path(relative).relative_to('MortalShell2/Content')
            if str(local).startswith('CSS/eins0fx/EveSB/PeonyV2/'):
                name = NAMES[0]
            elif str(local.with_suffix('')) == 'CSS/eins0fx/Shared/Materials/MF_TrimFlow':
                name = NAMES[1]
            else:
                raise ValueError('Cook attempted to replace an accepted dependency: ' + str(local))
            stage(name, relative, source_root / local, expected)

    converter = Converter(CSS / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out)
    for name in NAMES:
        trio = out / 'pack' / name
        trio.mkdir(parents=True)
        run(name + '-convert', [converter.retoc, 'to-zen', out / 'stage' / name, trio / (name + '.utoc'),
                               '--version', 'UE5_6', '--no-parallel'])
        run(name + '-verify', [converter.retoc, 'verify', trio / (name + '.utoc')])
        for suffix in ('.utoc', '.ucas'):
            path = trio / (name + suffix)
            manifests[name]['containers'][suffix] = dict(file=path.name, bytes=path.stat().st_size, sha256=sha(path))

    eve, shared = (manifests[name] for name in NAMES)
    original_catalog = copy.deepcopy(eve['catalog'])
    variants = eve['catalog']['outfits'][0]['variants']
    if len(variants) != 7 or variants[-1]['id'] != 'peony':
        raise ValueError('Installed seven-outfit catalog changed')
    variants[-1] = variant
    for resource in eve['resources']:
        protect(manifest_paths[NAMES[0]].parent / resource)
    recipe_proof = read(customize / 'customize-receipt.json')
    protect(customize / 'customize-receipt.json')
    for resource in recipe_proof['resources']:
        target = manifest_paths[NAMES[0]].parent / resource
        if resource not in eve['resources'] or sha(target) != eve['resources'][resource]['sha256']:
            raise ValueError('V2 dye resource is not the accepted Eve resource: ' + resource)
    if variants[:-1] != original_catalog['outfits'][0]['variants'][:-1]:
        raise ValueError('Existing six variants changed')

    resource_data = {r['file']: (manifest_paths[NAMES[1]].parent / r['file']).read_bytes()
                     for r in shared['facial_resources']}
    inline = resolve(shared, resource_data)
    library = inline['catalog']['animation_library']
    before = copy.deepcopy(library)
    for rows in [library['adapters'], *[face['adapters'] for face in library['faces']]]:
        for adapter in rows:
            if adapter['source_outfit'] == eve['id']:
                adapter['source_toc_sha256'] = eve['containers']['.utoc']['sha256']
                adapter['source_bulk_bytes'] = eve['containers']['.ucas']['bytes']
    face, = [f for f in library['faces'] if f['id'] == 'css.eve.face']
    old_mesh = original_catalog['outfits'][0]['variants'][-1]['mesh']
    adapter, = [a for a in face['adapters'] if a['source_mesh'] == old_mesh]
    adapter.update(source_mesh=variant['mesh'],
                   target_mesh='/Game/CSS/eins0fx/EveSB/PeonyV2/Meshes/SK_PeonyV2_Face.SK_PeonyV2_Face')
    if library['entries'] != before['entries'] or library['faces'][1:] != before['faces'][1:]:
        raise ValueError('Unrelated animation/facial entries changed')
    if [a for a in library['adapters'] if a['source_outfit'] != eve['id']] != [
            a for a in before['adapters'] if a['source_outfit'] != eve['id']]:
        raise ValueError('Other outfit adapters changed')
    manifests[NAMES[1]], resources = externalize(inline)
    if resources != resource_data:
        raise ValueError('Facial performance curves changed')
    for name in NAMES:
        manifest_paths[name].write_bytes(encode(manifests[name]))
        run(name + '-metadata', [DEFAULT_REPAK, 'pack', out / 'metadata' / name,
                                out / 'pack' / name / (name + '.pak'), '--version', 'V8B'])
    if verify(out / 'pack' / NAMES[0]) != eve:
        raise ValueError('Packaged Eve catalog differs')
    for name in OTHERS:
        directory = out / 'pack' / name
        directory.mkdir()
        for suffix in ('.pak', '.utoc', '.ucas'):
            (directory / (name + suffix)).symlink_to(original / name / (name + suffix))
    fixtures = read(WORK / 'peony-refit2/variants.json')
    fixture, = [f for f in fixtures if f['outfit'] == eve['id'] and f['variant'] == 'peony']
    fixture['animations'] = variant['animations']
    write(out / 'variants.json', fixtures)
    run('catalog', [args.catalog_test, '--audited-peony-pack', out / 'pack', out / 'variants.json'])

    mount = out / 'containers'
    converter.base_containers(GAME, mount)
    for name in (*NAMES, *OTHERS):
        for path in (out / 'pack' / name).iterdir():
            (mount / path.name).symlink_to(path.resolve())
    run('roundtrip', [converter.retoc, 'to-legacy', mount, out / 'readback', '--version', 'UE5_6',
                     '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/EveSB/', '-f', '/CSS/eins0fx/Shared/'])
    for files in payloads.values():
        for relative, expected in files.items():
            if Path(relative).suffix != '.uasset' and sha(out / 'readback' / relative) != expected:
                raise ValueError('Roundtrip changed payload: ' + relative)
    references = {s for m in manifests.values() for s in strings(m['catalog']) if s.startswith('/Game/')}
    packages = set(owners) | {reference.split('.')[0] for reference in references}
    (out / 'packages.txt').write_text('\n'.join(sorted(packages)) + '\n')
    (out / 'references.txt').write_text('\n'.join(sorted(references)) + '\n')
    run('imports', [args.dotnet, CSS / 'tools/AssetDependencyAudit/bin/Release/net10.0/AssetDependencyAudit.dll',
                    mount, args.mappings, out / 'packages.txt', out / 'references.txt', out / 'imports.json'])
    if not read(out / 'imports.json')['passed']:
        raise ValueError('Unresolved package imports')
    for path, expected in protected.items():
        if sha(Path(path)) != expected:
            raise ValueError('Input changed during assembly: ' + path)
    write(out / 'assembly.json', dict(passed=True, installed=False, inputs=protected, asset_owners=owners,
        payloads=payloads, files={str(p.relative_to(out / 'pack')): sha(p)
                                for p in (out / 'pack').rglob('*') if p.is_file()},
        pending=['V2 astral material compatibility', 'Native visual and garment fit acceptance',
                 'Controls, profiles, cloth, gameplay and performance regression']))
    print('Peony V2 Eve/shared package verified; not installed')


if __name__ == '__main__':
    main()
