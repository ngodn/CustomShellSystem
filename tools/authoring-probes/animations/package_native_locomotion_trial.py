"""Add only the four native locomotion choices to the shared animation trial."""
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
    if not output.is_relative_to(work) or output.exists():
        raise ValueError('Use a new private animation workspace')
    base = work / 'shared-dance-trial-v1'
    native = work / 'native-locomotion-cook-v1'
    if shutil.disk_usage(work).free < 800 * 1024**2:
        raise ValueError('Need 800 MiB for containers and independent readback')
    protected = {}

    def protect(path, checksum=None):
        actual = sha(path)
        if checksum is not None and actual != checksum:
            raise ValueError('Evidence changed: ' + str(path))
        protected[str(path)] = actual

    base_report = read(base / 'verification.json')
    native_report = read(native / 'pack/verification.json')
    for folder, report, stem in ((base, base_report, STEM),
                                 (native / 'pack', native_report, 'CSS_NativeLocomotionStudy_P')):
        if report['passed'] is not True:
            raise ValueError('Input package failed verification')
        protect(folder / 'verification.json')
        for name, checksum in report['files'].items():
            protect(folder / stem / name, checksum)
    for name in ('full-motion-verification.json', 'dependency-verification.json'):
        path = native / name
        report = read(path)
        if report['passed'] is not True:
            raise ValueError('Native motion or dependencies failed verification')
        protect(path)
        for source, checksum in report.get('inputs', {}).items():
            protect(Path(source), checksum)
    motion = read(native / 'full-motion-verification.json')
    if len(motion['clips']) != 20 or sum(row['frames'] for row in motion['clips']) != 754 or any(row['tracks'] != 1208 or not row['passed'] for row in motion['clips']):
        raise ValueError('Native full-clip evidence changed')
    payloads = {}
    for folder in (base, native / 'pack'):
        for header in sorted((folder / 'stage').rglob('*.uasset')):
            if not asset_info(header.read_bytes())['package'].startswith('/Game/CSS/Locomotion/'):
                raise ValueError('Unexpected shared asset namespace')
            for source in (header, header.with_suffix('.uexp')):
                protect(source)
                relative = source.relative_to(folder / 'stage')
                if relative in payloads:
                    if sha(payloads[relative]) != sha(source):
                        raise ValueError('Conflicting shared dependency: ' + str(relative))
                else:
                    payloads[relative] = source
    if len(payloads) != 190:
        raise ValueError('Expected 71 unchanged exports and 24 native sequences/carriers')
    manifest_path = base / 'metadata' / PACKAGE_ROOT / 'css.locomotion/manifest.json'
    protect(manifest_path)
    original = read(manifest_path)
    manifest = copy.deepcopy(original)
    library = manifest['catalog']['animation_library']
    if len(library['entries']) != 59 or len(library['adapters']) != 5:
        raise ValueError('Original trial dimensions changed')
    skeleton = '/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton'
    package = '/Game/CSS/Locomotion/NativeHuman/Studies/NL2/'
    entry = next(row for row in library['entries'] if row['id'] == 'css.eve.idle.default')
    entry['assets'].append(dict(asset=package+'AN_Idle.AN_Idle', skeleton=skeleton,
                               idle_blend_space=package+'BS_Idle.BS_Idle'))
    for gait in ('Walk', 'Jog', 'Sprint'):
        library['entries'].append(dict(id='css.eve.'+gait.lower(), name='Eve '+gait,
            category='locomotion', playback='loop', slot=gait.lower(), hide_weapons=False,
            assets=[dict(asset=package+'BS_'+gait+'.BS_'+gait, skeleton=skeleton)],
            aliases=[dict(slot=gait.lower(), id='eve',
                          asset='/Game/CSS/Eve/Anim/BS_S1_'+gait+'.BS_S1_'+gait)]))
    rollback = copy.deepcopy(manifest)
    rollback['catalog']['animation_library']['entries'] = rollback['catalog']['animation_library']['entries'][:-3]
    next(row for row in rollback['catalog']['animation_library']['entries']
         if row['id'] == entry['id'])['assets'].pop()
    if rollback != original:
        raise ValueError('Existing catalog changed beyond the four native choices')
    output.mkdir(parents=True)
    for relative, source in payloads.items():
        target = output / 'stage' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.hardlink_to(source)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    trio = output / STEM
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', output / 'stage', trio / (STEM + '.utoc'),
                  '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio / (STEM + '.utoc'))
    mounts = output / 'containers'
    converter.base_containers(GAME, mounts)
    for source, checksum in base_report['dependencies'].items():
        source = Path(source)
        protect(source, checksum)
        (mounts / source.name).symlink_to(source)
    for source in trio.iterdir():
        (mounts / source.name).symlink_to(source)
    converter.run(converter.retoc, 'to-legacy', mounts, output / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/Locomotion/')
    for relative, source in payloads.items():
        decoded = output / 'readback' / relative
        if source.suffix == '.uexp':
            if sha(source) != sha(decoded):
                raise ValueError('Cooked export changed: ' + str(relative))
        elif asset_info(source.read_bytes())['package'] != asset_info(decoded.read_bytes())['package']:
            raise ValueError('Package identity changed')
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
    report = dict(passed=True, entries=62, exports=95, native_candidates=4,
        unchanged_existing_exports=71, adapters=5, protected=protected,
        dependencies=base_report['dependencies'], files={p.name: sha(p) for p in trio.iterdir()},
        installed=False, scope='Private four-choice native locomotion candidate. Runtime, visible-mesh and live acceptance remain.')
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print('NATIVE_LOCOMOTION_TRIAL: 62 identities, 95 exports, four native choices')


if __name__ == '__main__':
    main()
