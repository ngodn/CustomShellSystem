"""Assemble a coordinated cloth trial from verified namespace packages, without installing it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css_convert import Converter, DEFAULT_REPAK, asset_info
from css import GAME
from css_facial_resources import encode, externalize, resolve

GENESSA = 'CSS_UnholyGenessa_eins0fx_P'
SHARED = 'CSS_SharedAssets_P'


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('cook', 'baseline', 'genessa', 'shared', 'output', 'dotnet', 'mappings'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--dependency-reader', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    assert not out.exists(), out
    assert read(args.cook / 'exit.json') == dict(exit_code=0, project_inputs_unchanged=True)
    baseline = read(args.baseline / 'receipt.json')
    assert baseline['passed'] and read(args.baseline / 'validation.json')['passed']
    out.mkdir()
    protected = {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        digest = sha(path)
        assert expected is None or digest == expected, path
        protected[str(path)] = digest

    def run(label, command):
        command = list(map(str, command))
        (out / (label + '-command.json')).write_text(json.dumps(command, indent=2))
        env = os.environ.copy()
        env['DOTNET_ROOT'] = str(args.dotnet.parent)
        with (out / (label + '.log')).open('x') as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        (out / (label + '-exit.json')).write_text(json.dumps(dict(exit_code=result.returncode)))
        result.check_returncode()

    pack = out / 'pack'
    pack.mkdir()
    cooked = args.cook / 'project/Saved/Cooked/Windows/CSSShared/Content'
    changed = [f'CSS/eins0fx/UnholyGenessa/{folder}SK_{v}W3{suffix}'
               for v in ('Eve', 'Genessa') for folder, suffix in (('', ''), ('Faces/', '_Face'))]
    additions = [f'CSS/eins0fx/Shared/Deformers/{kind}_ClothMorphBody' for kind in ('DG', 'MDC')]
    owners = dict(baseline['asset_owners'])
    payloads = {}
    for name, directory, packages in ((GENESSA, args.genessa, changed), (SHARED, args.shared, additions)):
        receipt = read(directory / 'verification.json')
        assert receipt['passed']
        protect(directory / 'verification.json')
        for suffix in ('.utoc', '.ucas'):
            protect(args.baseline / 'pack' / (name + suffix), receipt['files'][name + suffix])
        stage = out / 'stage' / name
        for relative, digest in receipt['payloads'].items():
            relative = Path(relative)
            assert not relative.is_absolute() and '..' not in relative.parts
            source = directory / 'stage' / relative
            protect(source, digest)
            target = stage / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)
        for package in packages:
            assert ('/Game/' + package in owners) == (name == GENESSA), package
            owners['/Game/' + package] = name
            for suffix in ('.uasset', '.uexp', '.ubulk'):
                source = cooked / (package + suffix)
                target = stage / 'MortalShell2/Content' / (package + suffix)
                if target.exists():
                    target.unlink()
                if suffix == '.ubulk' and not source.exists():
                    continue
                protect(source)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
        actual = {asset_info(f.read_bytes())['package'] for f in stage.rglob('*.uasset')}
        assert actual == {p for p, owner in owners.items() if owner == name}
        payloads[name] = {str(f.relative_to(stage)): sha(f) for f in stage.rglob('*') if f.is_file()}
        (out / name).mkdir()
        converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out / name)
        toc = pack / (name + '.utoc')
        converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
        converter.run(converter.retoc, 'verify', toc)

    metadata = out / 'metadata'
    for name in baseline['container_ids']:
        source_dir = args.baseline / 'metadata' / name
        for source in source_dir.rglob('*'):
            if source.is_file():
                protect(source)
        shutil.copytree(source_dir, metadata / name)
        manifest_path, = (metadata / name).rglob('manifest.json')
        manifest = read(manifest_path)
        for suffix in ('.utoc', '.ucas'):
            source = args.baseline / 'pack' / (name + suffix)
            if name not in (GENESSA, SHARED):
                protect(source, manifest['containers'][suffix]['sha256'])
                (pack / source.name).symlink_to(source.resolve())
            target = pack / (name + suffix)
            manifest['containers'][suffix] = dict(file=target.name, bytes=target.stat().st_size, sha256=sha(target))
        if name == SHARED:
            resources = {r['file']: (manifest_path.parent / r['file']).read_bytes()
                         for r in manifest['facial_resources']}
            inline = resolve(manifest, resources)
            library = inline['catalog']['animation_library']
            toc, bulk = pack / (GENESSA + '.utoc'), pack / (GENESSA + '.ucas')
            updated = 0
            for adapters in [library['adapters'], *[f['adapters'] for f in library['faces']]]:
                for adapter in adapters:
                    if adapter['source_outfit'] == 'eins0fx.unholygenessa':
                        adapter['source_toc_sha256'] = sha(toc)
                        adapter['source_bulk_bytes'] = bulk.stat().st_size
                        updated += 1
            assert updated > 0
            manifest, resources = externalize(inline)
            for resource, data in resources.items():
                (manifest_path.parent / resource).write_bytes(data)
        manifest_path.write_bytes(encode(manifest))
        run(name + '-metadata', [DEFAULT_REPAK, 'pack', metadata / name, pack / (name + '.pak'), '--version', 'V8B'])

    (out / 'combined').mkdir()
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out / 'combined')
    mount = out / 'containers'
    converter.base_containers(GAME, mount)
    for f in pack.iterdir():
        (mount / f.name).symlink_to(f.resolve())
    converter.run(converter.retoc, 'to-legacy', mount, out / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/')
    for name, files in payloads.items():
        for relative, digest in files.items():
            if Path(relative).suffix != '.uasset':
                assert sha(out / 'readback' / relative) == digest, relative
    (out / 'packages.txt').write_text('\n'.join(sorted(owners)) + '\n')
    references = sorted(set(baseline['references']) | {p + '.' + p.rsplit('/', 1)[1] for p in owners
                                                     if p.removeprefix('/Game/') in changed + additions})
    (out / 'references.txt').write_text('\n'.join(references) + '\n')
    run('imports', [args.dotnet, args.dependency_reader,
                    mount, args.mappings, out / 'packages.txt', out / 'references.txt', out / 'imports.json'])
    run('catalog', [ROOT / 'build/host/css_animation_library_tests', '--audited-namespace-pack', pack,
                    args.baseline / 'variants.json'])
    assert all(sha(Path(f)) == h for f, h in protected.items())
    report = dict(passed=True, installed=False, inputs=protected, payloads=payloads, asset_owners=owners,
                  changed_meshes=changed, added_graphs=additions, files={f.name: sha(f) for f in pack.iterdir()},
                  pending=['independent mesh comparison', 'gameplay', 'contact', 'performance'])
    (out / 'assembly.json').write_text(json.dumps(report, indent=2))
    print('Coordinated cloth trial assembled; not installed')


if __name__ == '__main__':
    main()
