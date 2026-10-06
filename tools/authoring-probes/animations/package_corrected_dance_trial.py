"""Combine the verified 57-clip trial and L5X body dances without recooking assets."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, PACKAGE_ROOT, asset_info

STEM = 'CSS_LocomotionSharedAssets_P'


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    work = ROOT / 'work/cine-animation-port'
    output = args.output.resolve()
    if not output.is_relative_to(work) or output.exists():
        raise ValueError('Use a new private animation workspace')
    base = work / 'shared-v1/pack-with-face-motion-v2'
    extra = work / 'l5x-cook/pack'
    protected, dependencies = {}, {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        actual = sha(path)
        if expected is not None and actual != expected:
            raise ValueError('Evidence changed: ' + str(path))
        previous = protected.setdefault(str(path), actual)
        if previous != actual:
            raise ValueError('Conflicting evidence: ' + str(path))

    payloads = {}
    for folder in (base, extra):
        report = read(folder / 'verification.json')
        if report['passed'] is not True:
            raise ValueError('Source package verification failed')
        protect(folder / 'verification.json')
        for name, checksum in report['files'].items():
            protect(folder / STEM / name, checksum)
        for path, checksum in report['dependencies'].items():
            protect(Path(path), checksum)
            dependencies[path] = checksum
        headers = sorted((folder / 'stage').rglob('*.uasset'))
        if len(headers) != report['packages']:
            raise ValueError('Source package asset count changed')
        for header in headers:
            info = asset_info(header.read_bytes())
            if not info['package'].startswith('/Game/CSS/Locomotion/'):
                raise ValueError('Asset is outside the shared namespace')
            for path in (header, header.with_suffix('.uexp')):
                relative = path.relative_to(folder / 'stage')
                if relative in payloads:
                    raise ValueError('Duplicate asset in trial inputs')
                roundtrip = folder / 'readback' / relative
                if path.suffix == '.uexp' and sha(path) != sha(roundtrip):
                    raise ValueError('Original round-trip export differs')
                protect(path)
                protect(roundtrip)
                payloads[relative] = path

    manifest_path = base / 'metadata' / PACKAGE_ROOT / 'css.locomotion/manifest.json'
    protect(manifest_path)
    manifest = copy.deepcopy(read(manifest_path))
    library = manifest['catalog']['animation_library']
    if len(library['entries']) != 57 or len(library['adapters']) != 5:
        raise ValueError('Expected the three requested outfits and 57 existing clips')
    allowed_layers = {'/Game/CSS/Locomotion/Adapters/ABP_' + name + '.ABP_' + name + '_C'
                      for name in ('Eve', 'EveBikini', 'EveKnit', 'Genessa', 'CommanderWhite')}
    if {row['target_class'] for row in library['adapters']} != allowed_layers:
        raise ValueError('Unexpected outfit adapter')
    evaluation_path = work / 'l5x-adapter-v1/corrected-current-result.json'
    evaluation = read(evaluation_path)
    if not all(evaluation.get(key) is True for key in ('complete', 'passed', 'body_passed', 'fingers_passed')):
        raise ValueError('Dance adapter comparison failed')
    protect(evaluation_path)
    # Source mounts can be empty outside the editor sandbox; their immutable
    # cooked revisions are checked through the dependencies above.
    for path, checksum in evaluation['protected'].items():
        if Path(path).is_file():
            protect(Path(path), checksum)
    old_entries = copy.deepcopy(library['entries'])
    required_cases = {'inactive', 'active_start', 'active_seek', 'held_end', 'stopped',
                      'null_clip_guard', 'restored', 'zero_weight', 'full_clip'}
    for label, display, frames in (('DumbDumb', 'DumbDumb', 3943), ('IrisOut', 'Iris Out', 1001)):
        identity = 'css.eve.dance.' + label.lower()
        asset = '/Game/CSS/Locomotion/Studies/L5X/AN_' + label + '.AN_' + label
        path = work / ('l5x-cook/decoded-runtime-v1/' + label.lower() + '-verification.json')
        motion = read(path)
        if not motion['complete'] or not motion['passed'] or motion['frames'] != frames or motion['asset'] != asset:
            raise ValueError('Full cooked-motion comparison failed')
        protect(path)
        for source, checksum in motion['inputs'].items():
            protect(Path(source), checksum)
        rows = [row for row in evaluation['results'] if row['id'] == identity]
        if len(rows) != 9 or len({row['mesh'] for row in rows}) != 9:
            raise ValueError('Expected all nine required outfit meshes')
        for row in rows:
            if (row['performance'] != asset or row['finger_bones_checked'] != 38 or
                    {case['case'] for case in row['cases']} != required_cases or
                    not all(case['body_passed'] and case['fingers_passed'] for case in row['cases'])):
                raise ValueError('Incomplete dance playback/restoration coverage')
        library['entries'].append(dict(id=identity, name=display, category='dance', playback='once',
            hide_weapons=True, assets=[dict(asset=asset, skeleton='/Game/CSS/Shared/SKEL_Base.SKEL_Base')]))
    if len({row['id'] for row in library['entries']}) != 59 or library['entries'][:57] != old_entries:
        raise ValueError('Existing entries changed or new identities collide')
    output.mkdir(parents=True)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    for relative, source in payloads.items():
        target = output / 'stage' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.hardlink_to(source)
    trio = output / STEM
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', output / 'stage', trio / (STEM + '.utoc'), '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio / (STEM + '.utoc'))
    mounts = output / 'containers'
    converter.base_containers(GAME, mounts)
    for path in [Path(p) for p in dependencies] + list(trio.iterdir()):
        (mounts / path.name).symlink_to(path)
    converter.run(converter.retoc, 'to-legacy', mounts, output / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/Locomotion/')
    for relative, source in payloads.items():
        decoded = output / 'readback' / relative
        if source.suffix == '.uexp':
            if sha(decoded) != sha(source):
                raise ValueError('Merged export changed: ' + str(relative))
        else:
            asset_info(decoded.read_bytes())
    manifest['containers'] = {suffix: dict(file=STEM + suffix, bytes=(trio / (STEM + suffix)).stat().st_size,
        sha256=sha(trio / (STEM + suffix))) for suffix in ('.utoc', '.ucas')}
    path = output / 'metadata' / PACKAGE_ROOT / 'css.locomotion/manifest.json'
    path.parent.mkdir(parents=True)
    path.write_text(json.dumps(manifest, indent=2) + '\n')
    converter.run(DEFAULT_REPAK, 'pack', output / 'metadata', trio / (STEM + '.pak'), '--version', 'V8B')
    converter.run(DEFAULT_REPAK, 'unpack', trio / (STEM + '.pak'), '--output', output / 'metadata-readback')
    if read(output / 'metadata-readback' / PACKAGE_ROOT / 'css.locomotion/manifest.json') != manifest:
        raise ValueError('Merged catalog round-trip differs')
    for path, checksum in protected.items():
        if sha(Path(path)) != checksum:
            raise ValueError('Input changed while packaging: ' + path)
    (output / 'verification.json').write_text(json.dumps(dict(passed=True, sequences=59,
        adapters=5, facial_meshes=6, unchanged_existing_entries=57, exact_export_payloads=len(payloads)//2,
        protected=protected, dependencies=dependencies,
        compression_settings=read(extra / 'verification.json')['compression_settings'],
        files={p.name: sha(p) for p in trio.iterdir()}, installed=False,
        pending=['Unknown source indices 3269-3275', 'Remaining dance facial controls',
                 'Full visual and live acceptance', 'Default-human runtime integration'],
        scope='Private combined trial, not a release or proof of live compatibility.'), indent=2) + '\n')
    print('COMBINED_DANCE_TRIAL', len(library['entries']), len(payloads)//2, flush=True)


if __name__ == '__main__':
    main()
