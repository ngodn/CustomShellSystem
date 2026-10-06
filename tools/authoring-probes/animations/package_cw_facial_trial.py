"""Add the verified Commander White face to the complete shared trial."""
import argparse
import copy
import json
from pathlib import Path
import shutil

from package_corrected_dance_trial import (
    ROOT, STEM, GAME, PACKAGE_ROOT, DEFAULT_REPAK, Converter, asset_info, read, sha,
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    output = parser.parse_args().output.resolve()
    work = ROOT / 'work/cine-animation-port'
    if output.exists() or not any(output.is_relative_to(parent) for parent in
            (work, Path('/mnt/eins0fxE/CSS-authoring-scratch'))):
        raise ValueError('Use new private animation storage')
    if shutil.disk_usage(output.parent).free < 2 * 1024**3:
        raise ValueError('Need 2 GiB for packaging and independent readback')
    base = work / 'native-locomotion-trial-v1'
    face = work / 'cw-expression-cook-v3'
    protected = {}

    def protect(path, checksum=None):
        actual = sha(path)
        if checksum is not None and actual != checksum:
            raise ValueError('Verified input changed: ' + str(path))
        protected[str(path)] = actual

    baseline = read(base / 'verification.json')
    evidence = read(face / 'facial-catalog-evidence.json')
    decoded = read(face / 'cooked-readback-v1.json')
    rig = read(face / 'facial-rig.json')
    if (not baseline.get('passed') or not evidence.get('passed') or
            not decoded.get('passed') or not decoded.get('source_deltas_checked') or
            len(decoded['results']) != 1 or rig['id'] != 'css.commanderwhite.face' or
            len(rig['adapters']) != 1 or len(rig['morphs']) != 74 or len(rig['expressions']) != 9):
        raise ValueError('Require the accepted native trial and complete Commander White face')
    for filename in ('cook-exit.json', 'package-exit.json', 'readback-exit.json'):
        if read(face / filename)['exit_code'] != 0:
            raise ValueError('Incomplete facial cook, package or readback')
        protect(face / filename)
    for path in (base / 'verification.json', face / 'facial-catalog-evidence.json',
                 face / 'cooked-readback-v1.json'):
        protect(path)
    protect(face / 'facial-rig.json', evidence['rig_sha256'])
    for path, checksum in evidence['protected'].items():
        protect(Path(path), checksum)
    for name, checksum in baseline['files'].items():
        protect(base / STEM / name, checksum)
    adapter = rig['adapters'][0]
    row = decoded['results'][0]
    if not row['passed'] or (row['source'], row['target']) != tuple(
            adapter[key].split('.')[0] for key in ('source_mesh', 'target_mesh')):
        raise ValueError('Facial binding differs from cooked evidence')
    dependencies = baseline['dependencies']
    matching = [Path(p) for p, checksum in dependencies.items() if p.endswith('.utoc') and
                checksum == adapter['source_toc_sha256'] and
                Path(p).with_suffix('.ucas').stat().st_size == adapter['source_bulk_bytes']]
    if len(matching) != 1:
        raise ValueError('Missing or ambiguous Commander White revision')
    manifest_path = base / 'metadata' / PACKAGE_ROOT / 'css.locomotion/manifest.json'
    protect(manifest_path)
    original = read(manifest_path)
    manifest = copy.deepcopy(original)
    library = manifest['catalog']['animation_library']
    if len(library['entries']) != 62 or len(library['adapters']) != 5 or len(library['faces']) != 1:
        raise ValueError('Base catalog dimensions changed')
    library['faces'].append(rig)
    control = copy.deepcopy(manifest)
    control['catalog']['animation_library']['faces'].pop()
    if control != original:
        raise ValueError('Existing catalog changed')
    payloads = {}
    for header in sorted((base / 'stage').rglob('*.uasset')):
        for source in (header, header.with_suffix('.uexp')):
            protect(source)
            payloads[source.relative_to(base / 'stage')] = source
    if len(payloads) != 190:
        raise ValueError('Expected all 95 previous exports')
    package = row['target']
    for suffix in ('.uasset', '.uexp'):
        relative = Path('MortalShell2/Content') / (package.removeprefix('/Game/') + suffix)
        source = face / 'cooked/CSSAuthoring/Content' / (package.removeprefix('/Game/') + suffix)
        protect(source, evidence['protected'][str(source)])
        if relative in payloads:
            raise ValueError('Duplicate facial package')
        payloads[relative] = source
    output.mkdir()
    for relative, source in payloads.items():
        target = output / 'stage' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    trio = output / STEM
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', output / 'stage', trio / (STEM + '.utoc'),
                  '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio / (STEM + '.utoc'))
    mounts = output / 'containers'
    converter.base_containers(GAME, mounts)
    for source, checksum in dependencies.items():
        source = Path(source)
        protect(source, checksum)
        (mounts / source.name).symlink_to(source)
    for source in trio.iterdir():
        (mounts / source.name).symlink_to(source)
    converter.run(converter.retoc, 'to-legacy', mounts, output / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/Locomotion/')
    for relative, source in payloads.items():
        result = output / 'readback' / relative
        if source.suffix == '.uexp':
            if sha(source) != sha(result):
                raise ValueError('Changed export: ' + str(relative))
        elif asset_info(source.read_bytes())['package'] != asset_info(result.read_bytes())['package']:
            raise ValueError('Changed package identity')
    manifest['containers'] = {suffix: dict(file=STEM + suffix,
        bytes=(trio / (STEM + suffix)).stat().st_size, sha256=sha(trio / (STEM + suffix)))
        for suffix in ('.utoc', '.ucas')}
    path = output / 'metadata' / PACKAGE_ROOT / 'css.locomotion/manifest.json'
    path.parent.mkdir(parents=True)
    path.write_text(json.dumps(manifest, indent=2) + '\n')
    converter.run(DEFAULT_REPAK, 'pack', output / 'metadata', trio / (STEM + '.pak'), '--version', 'V8B')
    converter.run(DEFAULT_REPAK, 'unpack', trio / (STEM + '.pak'), '--output', output / 'metadata-readback')
    if read(output / 'metadata-readback' / PACKAGE_ROOT / 'css.locomotion/manifest.json') != manifest:
        raise ValueError('Catalog round-trip changed')
    for source, checksum in protected.items():
        if sha(Path(source)) != checksum:
            raise ValueError('Input changed during packaging: ' + source)
    report = dict(passed=True, entries=62, exports=96, native_candidates=4,
        unchanged_existing_exports=95, adapters=5, facial_rigs=2, protected=protected,
        dependencies=dependencies, files={p.name: sha(p) for p in trio.iterdir()},
        installed=False, scope='Combined private trial with CW static expressions. Live acceptance and facial performance mapping remain.')
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print('CW_FACIAL_TRIAL: 62 identities, 96 exports, two facial rigs')


if __name__ == '__main__':
    main()
