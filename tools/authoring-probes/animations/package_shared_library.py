"""Package an audited animation-only cook for private verification, without installing it."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, PACKAGE_ROOT, asset_info
from css_paths import package_path, windows_destination


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('--dependency', type=Path, required=True,
                        help='Verified outfit trio supplying Shared/SKEL_Base')
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    if json.loads((work / 'cook-exit.json').read_text())['exit_code'] != 0:
        raise ValueError('Cook did not exit successfully')
    verification = json.loads((work / 'verify-result.json').read_text())
    if verification['complete'] is not True:
        raise ValueError('Fresh editor readback is incomplete')
    catalog = json.loads((work / 'catalog.json').read_text())
    library = catalog['animation_library']
    packages = (work / 'cook.txt').read_text().splitlines()
    expected = {entry['assets'][0]['asset'].split('.')[0] for entry in library['entries']}
    verified = {entry['asset'].split('.')[0] for entry in verification['results']}
    if len(packages) != len(set(packages)) or set(packages) != expected or expected != verified:
        raise ValueError('Cook, catalog and fresh readback disagree')
    dependency = args.dependency.resolve(strict=True)
    if dependency.suffix != '.utoc':
        raise ValueError('Dependency must be a .utoc with .ucas companion')
    dependency_files = [dependency, dependency.with_suffix('.ucas')]
    dependency_hashes = {str(path): digest(path) for path in dependency_files}
    out = work / 'pack'
    out.mkdir(exist_ok=False)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out)
    stage = out / 'stage'
    payloads = {}
    for package in packages:
        package_path(package)
        if not package.startswith('/Game/CSS/Locomotion/'):
            raise ValueError(f'Unexpected package: {package}')
        relative = package.removeprefix('/Game/')
        for suffix in ('.uasset', '.uexp'):
            source = work / 'cooked/CSSAuthoring/Content' / (relative + suffix)
            destination = stage / 'MortalShell2/Content' / (relative + suffix)
            windows_destination('C:/CSS', str(destination.relative_to(stage)))
            if suffix == '.uasset':
                info = asset_info(source.read_bytes())
                if info['package'] != package or not any(e['class'] == 'AnimSequence' for e in info['exports']):
                    raise ValueError(f'Not the expected cooked sequence: {package}')
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
            payloads[str(destination.relative_to(stage))] = digest(source)
    stem = 'CSS_LocomotionSharedAssets_P'
    trio = out / stem
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', stage, trio / (stem + '.utoc'),
                  '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio / (stem + '.utoc'))
    mount = out / 'containers'
    converter.base_containers(GAME, mount)
    for path in dependency_files + list(trio.iterdir()):
        (mount / path.name).symlink_to(path)
    converter.run(converter.retoc, 'to-legacy', mount, out / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/Locomotion/')
    for relative, checksum in payloads.items():
        decoded = out / 'readback' / relative
        if not decoded.is_file():
            raise ValueError(f'Missing round-trip file: {relative}')
        if decoded.suffix == '.uexp' and digest(decoded) != checksum:
            raise ValueError(f'Round-trip changed export bytes: {relative}')
        if decoded.suffix == '.uasset':
            asset_info(decoded.read_bytes())  # Reject unresolved imports, not just missing files.
    manifest = dict(format='CSS.Package', format_version=1, game='MortalShell2', engine='5.6',
                    kind='animation_library', catalog=catalog,
                    **{key: library[key] for key in ('id', 'name', 'author', 'version')})
    manifest['containers'] = {suffix: dict(file=stem + suffix, bytes=(trio / (stem + suffix)).stat().st_size,
                                         sha256=digest(trio / (stem + suffix))) for suffix in ('.utoc', '.ucas')}
    metadata = out / 'metadata'
    path = metadata / PACKAGE_ROOT / library['id'] / 'manifest.json'
    path.parent.mkdir(parents=True)
    path.write_text(json.dumps(manifest, indent=2) + '\n')
    converter.run(DEFAULT_REPAK, 'pack', metadata, trio / (stem + '.pak'), '--version', 'V8B')
    converter.run(DEFAULT_REPAK, 'unpack', trio / (stem + '.pak'), '--output', out / 'metadata-readback')
    if json.loads((out / 'metadata-readback' / PACKAGE_ROOT / library['id'] / 'manifest.json').read_text()) != manifest:
        raise ValueError('Manifest did not round-trip exactly')
    if any(digest(Path(path)) != checksum for path, checksum in dependency_hashes.items()):
        raise ValueError('Dependency changed during verification')
    report = dict(passed=True, packages=len(packages), exact_export_payloads=len(packages),
                  dependencies=dependency_hashes,
                  required_skeleton='/Game/CSS/Shared/SKEL_Base.SKEL_Base',
                  files={path.name: digest(path) for path in trio.iterdir()},
                  scope='Private structural package check. Decoded motion and live playback remain pending.')
    (out / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
