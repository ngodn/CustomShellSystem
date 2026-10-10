"""Build a private seven-outfit Eve/shared trial without installing anything.

Uses installed metadata to preserve the latest CW/Genessa adapter revisions,
verified legacy payloads for the existing packages, and Peony's Windows cook.
Fit and live acceptance remain separate from this package verification.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

CSS = Path(__file__).resolve().parents[2]
EVE = CSS.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
WORK = CSS / 'work/three-outfit-rig-physics'
NAMES = ('CSS_EveStellarBlade_eins0fx_P', 'CSS_SharedAssets_P')
OTHERS = ('CSS_CommanderWhite_eins0fx_P', 'CSS_UnholyGenessa_eins0fx_P')
sys.path.insert(0, str(CSS / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, PACKAGE_ROOT, asset_info
from css_facial_resources import encode, externalize, resolve
from css_package import verify


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text())


def write(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n')


def strings(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for key, child in value.items():
            if key != 'aliases':
                yield from strings(child)
    elif isinstance(value, list):
        for child in value:
            yield from strings(child)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('output', 'dotnet', 'mappings'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    shutil.copy2(__file__, out / Path(__file__).name)
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

    cook = EVE / 'work/peony-windows4'
    cook_proof = read(cook / 'readback1/receipt.json')
    if not cook_proof['passed'] or not read(cook / 'readback1/validation.json')['passed']:
        raise ValueError('Peony cook checks are incomplete')
    protect(cook / 'readback1/receipt.json')
    protect(cook / 'readback1/validation.json')
    for path, expected in cook_proof['files'].items():
        protect(Path(path), expected)
    groups = read(cook / 'plan.json')['groups']
    catalog_dir = EVE / 'work/peony-catalog4'
    catalog_proof = read(catalog_dir / 'assembly-receipt.json')
    protect(catalog_dir / 'assembly-receipt.json')
    for name, expected in catalog_proof['outputs'].items():
        protect(catalog_dir / name, expected)

    manifests, manifest_paths, payloads, owners = {}, {}, {}, {}
    original = GAME / 'Content/Paks/~mods'
    for name in (*NAMES, *OTHERS):
        for suffix in ('.pak', '.utoc', '.ucas'):
            protect(original / name / (name + suffix))
        metadata = out / 'metadata' / name
        metadata.parent.mkdir(parents=True, exist_ok=True)
        run(name + '-unpack', [DEFAULT_REPAK, 'unpack', original / name / (name + '.pak'), '--output', metadata])
        path, = metadata.rglob('manifest.json')
        manifest = read(path)
        for suffix, info in manifest['containers'].items():
            source = original / name / (name + suffix)
            if sha(source) != info['sha256'] or source.stat().st_size != info['bytes']:
                raise ValueError('Installed metadata does not match its container: ' + name)
        manifests[name], manifest_paths[name] = manifest, path

    baselines = (WORK / 'eve-final-container1', WORK / 'shared-cloth-native2')
    for name, baseline, group in zip(NAMES, baselines, ('cook_outfit', 'cook_shared'), strict=True):
        proof = read(baseline / 'verification.json')
        protect(baseline / 'verification.json')
        if not proof['passed']:
            raise ValueError('Unverified baseline: ' + name)
        for suffix in ('.utoc', '.ucas'):
            protect(original / name / (name + suffix), proof['files'][name + suffix])
        stage = out / 'stage' / name
        staged = {}
        for relative, expected in proof['payloads'].items():
            relative = Path(relative)
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('Unsafe staged payload path')
            source = baseline / 'stage' / relative
            protect(source, expected)
            target = stage / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)
            staged[str(relative)] = expected
        for package in groups[group]:
            for relative, expected in cook_proof['payloads'].items():
                if str(Path(relative).with_suffix('')) != 'MortalShell2/Content/' + package[6:]:
                    continue
                source = cook / 'readback1/stage' / relative
                protect(source, expected)
                target = stage / relative
                if target.exists():
                    raise ValueError('Peony unexpectedly replaces an existing asset: ' + package)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
                staged[relative] = expected
        for path in stage.rglob('*.uasset'):
            package = asset_info(path.read_bytes())['package']
            if package in owners:
                raise ValueError('Duplicate asset owner: ' + package)
            owners[package] = name
        if sum(owner == name for owner in owners.values()) != proof['packages'] + len(groups[group]):
            raise ValueError('Asset ownership/count changed')
        payloads[name] = staged
        trio = out / 'pack' / name
        trio.mkdir(parents=True)
        converter = Converter(CSS / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out)
        run(name + '-convert', [converter.retoc, 'to-zen', stage, trio / (name + '.utoc'), '--version', 'UE5_6', '--no-parallel'])
        run(name + '-verify', [converter.retoc, 'verify', trio / (name + '.utoc')])
        for suffix in ('.utoc', '.ucas'):
            path = trio / (name + suffix)
            manifests[name]['containers'][suffix] = dict(file=path.name, bytes=path.stat().st_size, sha256=sha(path))

    eve, shared = (manifests[n] for n in NAMES)
    catalog = read(catalog_dir / 'eve.css.json')
    if catalog['outfits'][0]['variants'][:-1] != eve['catalog']['outfits'][0]['variants']:
        raise ValueError('Existing six outfits differ from the accepted catalog')
    eve['catalog'] = catalog
    for name in eve['resources']:
        if sha(catalog_dir / name) != sha(manifest_paths[NAMES[0]].parent / name):
            raise ValueError('Existing dye resource changed')
    resource_data = {r['file']: (manifest_paths[NAMES[1]].parent / r['file']).read_bytes()
                     for r in shared['facial_resources']}
    inline = resolve(shared, resource_data)
    library = inline['catalog']['animation_library']
    before = copy.deepcopy(library)
    revision = eve['containers']
    for rows in [library['adapters'], *[f['adapters'] for f in library['faces']]]:
        for adapter in rows:
            if adapter['source_outfit'] == eve['id']:
                adapter['source_toc_sha256'] = revision['.utoc']['sha256']
                adapter['source_bulk_bytes'] = revision['.ucas']['bytes']
    face, = [f for f in library['faces'] if f['id'] == 'css.eve.face']
    peony = copy.deepcopy(face['adapters'][0])
    peony.update(source_mesh='/Game/CSS/eins0fx/EveSB/Peony/Meshes/SK_Peony.SK_Peony',
                 target_mesh='/Game/CSS/eins0fx/EveSB/Faces/Peony/SK_Facial1.SK_Facial1')
    face['adapters'].append(peony)
    if library['entries'] != before['entries'] or library['faces'][1:] != before['faces'][1:]:
        raise ValueError('Unrelated animation/facial content changed')
    if [a for a in library['adapters'] if a['source_outfit'] != eve['id']] != [
            a for a in before['adapters'] if a['source_outfit'] != eve['id']]:
        raise ValueError('CW/Genessa adapters changed')
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

    fixtures = read(WORK / 'catalog-namespace3/variants.json')
    for fixture in fixtures:
        matching = next(f for f in library['faces'] if any(a['source_outfit'] == fixture['outfit'] for a in f['adapters']))
        fixture['skeleton'] = matching['adapters'][0]['skeleton']
    extra = copy.deepcopy(next(f for f in fixtures if f['outfit'] == eve['id'] and f['variant'] == 'skin_suit'))
    extra.update(variant='peony', animations=catalog['outfits'][0]['variants'][-1]['animations'])
    fixtures.append(extra)
    write(out / 'variants.json', fixtures)
    run('catalog', [CSS / 'build/host/css_animation_library_tests', '--audited-peony-pack', out / 'pack', out / 'variants.json'])

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
    # Aliases identify historical choices, not assets to load. Native selector
    # checks above verify their resolution. Blueprint packages export generated
    # classes, so do not invent Package.Package object references for every asset.
    packages = set(owners) | {reference.split('.')[0] for reference in references}
    (out / 'packages.txt').write_text('\n'.join(sorted(packages)) + '\n')
    (out / 'references.txt').write_text('\n'.join(sorted(references)) + '\n')
    run('imports', [args.dotnet, CSS / 'tools/AssetDependencyAudit/bin/Release/net10.0/AssetDependencyAudit.dll',
                    mount, args.mappings, out / 'packages.txt', out / 'references.txt', out / 'imports.json'])
    if not read(out / 'imports.json')['passed']:
        raise ValueError('Unresolved package imports')
    if any(sha(Path(path)) != expected for path, expected in protected.items()):
        raise ValueError('Inputs changed during assembly')
    write(out / 'assembly.json', dict(passed=True, installed=False, inputs=protected, asset_owners=owners,
        payloads=payloads, files={str(p.relative_to(out / 'pack')): sha(p) for p in (out / 'pack').rglob('*') if p.is_file()},
        pending=['Hip accessory gameplay-pose comparison and final fit', 'Live materials, cloth and facial validation',
                 'Full gameplay regression and performance checks']))
    print('Seven-outfit Eve/shared private trial assembled and verified; not installed')


if __name__ == '__main__':
    main()
