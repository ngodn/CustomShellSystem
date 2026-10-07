"""Replace verified animation compression copies while preserving the shared catalog."""
import argparse
import copy
import errno
import json
from pathlib import Path
import shutil

from package_corrected_dance_trial import (
    ROOT, STEM, GAME, PACKAGE_ROOT, DEFAULT_REPAK, Converter, asset_info, read, sha,
)
from css_facial_resources import encode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base', type=Path)
    parser.add_argument('precision', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    base, precision = args.base.resolve(strict=True), args.precision.resolve(strict=True)
    output = args.output.resolve()
    roots = (ROOT / 'work/cine-animation-port', Path('/mnt/eins0fxE/CSS-authoring-scratch'))
    if output.exists() or not any(output.is_relative_to(root) for root in roots):
        raise ValueError('Use new private animation storage')
    protected = {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        checksum = sha(path)
        if (expected is not None and checksum != expected) or protected.setdefault(str(path), checksum) != checksum:
            raise ValueError('Verified input changed: ' + str(path))
        return path

    baseline = read(protect(base / 'verification.json'))
    probe = read(protect(precision / 'pack/verification.json'))
    authoring = read(protect(precision / 'verify-result.json'))
    if not baseline.get('passed') or not probe.get('passed') or not authoring.get('complete'):
        raise ValueError('Require verified source packages and authored copies')
    for path, checksum in authoring['protected'].items():
        protect(Path(path), checksum)
    for name, checksum in baseline['files'].items():
        protect(base / STEM / name, checksum)
    for name, checksum in probe['files'].items():
        protect(precision / 'pack' / STEM / name, checksum)
    relative = Path(PACKAGE_ROOT) / 'css.locomotion/manifest.json'
    original = read(protect(base / 'metadata' / relative))
    manifest = copy.deepcopy(original)
    entries = manifest['catalog']['animation_library']['entries']
    replacements = {}
    for index, row in enumerate(authoring['results']):
        report = read(protect(precision / 'comparisons' / f'{index:02d}.json'))
        if (not report.get('passed') or report['asset'] != row['asset'] or
                report['frames'] != row['keys'] or not row['result'].startswith('PASS:')):
            raise ValueError('Require full cooked-key verification for each exact authored copy')
        for path, checksum in report['inputs'].items():
            protect(Path(path), checksum)
        matches = [entry for entry in entries if entry['id'] == row['id']]
        if len(matches) != 1 or len(matches[0]['assets']) != 1:
            raise ValueError('Ambiguous animation replacement')
        asset = matches[0]['assets'][0]
        source = asset['asset'].split('.')[0]
        target = row['asset'].split('.')[0]
        if source == target or source in replacements or not target.startswith('/Game/CSS/Locomotion/Studies/'):
            raise ValueError('Expected new isolated compression copies')
        replacements[source] = target
        asset['asset'] = row['asset']
    if not replacements:
        raise ValueError('No verified replacements')
    restored = copy.deepcopy(manifest)
    original_entries = {entry['id']: entry for entry in original['catalog']['animation_library']['entries']}
    for entry in restored['catalog']['animation_library']['entries']:
        entry['assets'] = original_entries[entry['id']]['assets']
    if restored != original:
        raise ValueError('Unrelated catalog data changed')
    payloads, removed = {}, set()
    for header in sorted((base / 'stage').rglob('*.uasset')):
        package = asset_info(header.read_bytes())['package']
        if package in replacements:
            removed.add(package)
            continue
        for source in (header, header.with_suffix('.uexp')):
            payloads[source.relative_to(base / 'stage')] = protect(source)
    if removed != set(replacements) or len(payloads) // 2 != baseline['exports'] - len(replacements):
        raise ValueError('Source payload set changed')
    unchanged = len(payloads) // 2
    additions = set()
    for header in sorted((precision / 'pack/stage').rglob('*.uasset')):
        info = asset_info(header.read_bytes())
        if info['package'] not in replacements.values() and not any(
                item['class'] == 'AnimBoneCompressionSettings' for item in info['exports']):
            raise ValueError('Unexpected precision payload')
        additions.add(info['package'])
        for source in (header, header.with_suffix('.uexp')):
            key = source.relative_to(precision / 'pack/stage')
            if key in payloads or (source.suffix == '.uexp' and sha(source) != sha(precision / 'pack/readback' / key)):
                raise ValueError('Conflicting or unverified precision export')
            payloads[key] = protect(source)
    if not set(replacements.values()) < additions:
        raise ValueError('Missing sequence or owned compression settings')
    output.mkdir(parents=True)
    for key, source in payloads.items():
        target = output / 'stage' / key
        target.parent.mkdir(parents=True, exist_ok=True)
        try:
            target.hardlink_to(source)
        except OSError as error:
            if error.errno != errno.EXDEV:
                raise
            shutil.copy2(source, target)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    trio = output / STEM
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', output / 'stage', trio / (STEM + '.utoc'), '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio / (STEM + '.utoc'))
    mounts = output / 'containers'
    converter.base_containers(GAME, mounts)
    for path, checksum in baseline['dependencies'].items():
        source = protect(Path(path), checksum)
        (mounts / source.name).symlink_to(source)
    for source in trio.iterdir():
        (mounts / source.name).symlink_to(source)
    converter.run(converter.retoc, 'to-legacy', mounts, output / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/Locomotion/')
    for key, source in payloads.items():
        result = output / 'readback' / key
        if source.suffix == '.uexp':
            if sha(source) != sha(result):
                raise ValueError('Changed export: ' + str(key))
        elif asset_info(source.read_bytes())['package'] != asset_info(result.read_bytes())['package']:
            raise ValueError('Changed package identity: ' + str(key))
    manifest['containers'] = {suffix: dict(file=STEM + suffix, bytes=(trio / (STEM + suffix)).stat().st_size,
        sha256=sha(trio / (STEM + suffix))) for suffix in ('.utoc', '.ucas')}
    # Copy the external face resources verbatim. Only the two body bindings and
    # new container checksums differ from the input manifest.
    for source in (base / 'metadata').rglob('*'):
        if source.is_file():
            protect(source)
            target = output / 'metadata' / source.relative_to(base / 'metadata')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    target = output / 'metadata' / relative
    target.write_bytes(encode(manifest))
    converter.run(DEFAULT_REPAK, 'pack', output / 'metadata', trio / (STEM + '.pak'), '--version', 'V8B')
    converter.run(DEFAULT_REPAK, 'unpack', trio / (STEM + '.pak'), '--output', output / 'metadata-readback')
    for source in (output / 'metadata').rglob('*'):
        if source.is_file() and sha(source) != sha(output / 'metadata-readback' / source.relative_to(output / 'metadata')):
            raise ValueError('Metadata round-trip changed')
    for path, checksum in protected.items():
        if sha(Path(path)) != checksum:
            raise ValueError('Protected input changed: ' + path)
    result = dict(passed=True, entries=len(entries), exports=len(payloads) // 2,
                  unchanged_existing_exports=unchanged, replacements=replacements,
                  protected=protected, dependencies=baseline['dependencies'],
                  files={p.name: sha(p) for p in trio.iterdir()}, installed=False,
                  scope='Compression-only private candidate. Full temporal and live acceptance remain pending.')
    (output / 'verification.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PRECISION_TRIAL', result['entries'], result['exports'], flush=True)


if __name__ == '__main__':
    main()
