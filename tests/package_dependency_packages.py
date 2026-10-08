"""Exercise dependency discovery with real metadata PAKs and optional migrated outfits."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zlib


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repak', 'test-binary', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--migrated', type=Path)
    parser.add_argument('--baseline', type=Path)
    args = parser.parse_args()
    if bool(args.migrated) != bool(args.baseline):
        raise ValueError('Migrated and baseline inputs must be provided together')
    root = args.output.resolve()
    root.mkdir()
    cases = root / 'cases'
    cases.mkdir()
    fixtures = root / 'fixtures'
    fixtures.mkdir()
    expectations = []

    def requirement(identity, contract='layout.v1', name='Shared Assets'):
        return dict(id=identity, contract=contract, name=name)

    def png_chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))

    image = (b'\x89PNG\r\n\x1a\n' + png_chunk(b'IHDR', struct.pack('>IIBBBBB', 128, 128, 8, 6, 0, 0, 0))
             + png_chunk(b'IDAT', zlib.compress((b'\0' + b'\0\0\0\xff' * 128) * 128)) + png_chunk(b'IEND', b''))

    def package(label, identity, *, provides=None, dependencies=None, outfit=False, invalid_catalog=False, clip_id=None):
        directory = fixtures / label
        directory.mkdir()
        stem = 'CSS_' + label + '_P'
        containers = {}
        for suffix in ('.utoc', '.ucas'):
            file = directory / (stem + suffix)
            file.write_bytes(b'Synthetic dependency test container; not an Unreal asset.\n' + label.encode())
            containers[suffix] = dict(file=file.name, bytes=file.stat().st_size, sha256=digest(file))
        name = identity
        manifest = dict(format='CSS.Package', format_version=1, game='MortalShell2', engine='5.6',
                        id=identity, name=name, version='0.1.0', author='test', containers=containers)
        if provides is not None:
            manifest['provides'] = provides
        if dependencies is not None:
            manifest['dependencies'] = dependencies
        folder = root / 'metadata' / label / 'MortalShell2/Content/CSS/Packages' / identity
        folder.mkdir(parents=True)
        if outfit:
            manifest['thumbnail'] = dict(file='thumbnail.png', width=128, height=128,
                                         sha256=hashlib.sha256(image).hexdigest())
            (folder / 'thumbnail.png').write_bytes(image)
            manifest['catalog'] = dict(schema=1, outfits=[dict(id=identity, name=name, author='test',
                thumbnail='thumbnail.png', shells=['CharacterId.Player.Shell.Genessa'],
                variants=[dict(id='default', name='Default', mesh='/Game/CSS/test/Outfit/SK_Body.SK_Body')])])
        else:
            manifest['kind'] = 'animation_library'
            manifest['catalog'] = dict(schema=1, animation_library=dict(id=identity, name=name, author='test',
                version='0.1.0', entries=[dict(id=clip_id or identity + '.idle', name='Idle', category='idle', slot='idle',
                playback='loop', assets=[dict(asset='/Game/CSS/test/Shared/Locomotion/AN_Idle.AN_Idle',
                skeleton='/Game/CSS/Shared/SKEL_Base.SKEL_Base')])]))
            if invalid_catalog:
                manifest['catalog']['animation_library']['entries'][0]['playback'] = 'invalid'
        (folder / 'manifest.json').write_text(json.dumps(manifest))
        subprocess.run([str(args.repak.resolve()), 'pack', str(root / 'metadata' / label),
                        str(directory / (stem + '.pak')), '--version', 'V8B'], check=True, stdout=subprocess.DEVNULL)
        return list(directory.iterdir())

    def case(name, groups, outfits, libraries, loaded, rejected, **extra):
        directory = cases / name
        directory.mkdir()
        for index, files in enumerate(groups):
            target = directory / str(index)
            target.mkdir()
            for file in files:
                (target / file.name).symlink_to(file.resolve(strict=True))
        expectations.append(dict(name=name, outfits=outfits, libraries=libraries,
                                 loaded=loaded, rejected=rejected, **extra))

    provider = package('provider', 'test.shared', provides=['layout.v1'])
    consumer = package('consumer', 'test.outfit', outfit=True, dependencies=[requirement('test.shared')])
    unrelated = package('unrelated', 'test.unrelated', outfit=True)
    old = package('old', 'test.shared')
    wrong = package('wrong', 'test.shared', provides=['layout.v2'])
    broken = package('broken', 'test.shared', provides=['layout.v1'], invalid_catalog=True)
    case('healthy', [consumer, provider, unrelated], ['test.outfit', 'test.unrelated'], ['test.shared'], 3, 0)
    updated = package('compatible_update', 'test.shared', provides=['layout.v1', 'layout.additional'])
    case('compatible-update', [consumer, updated], ['test.outfit'], ['test.shared'], 2, 0)
    case('missing', [consumer, unrelated], ['test.unrelated'], [], 1, 1, reasons=['Install Shared Assets'])
    case('missing-alone', [consumer], [], [], 0, 1, empty_message='Install Shared Assets')
    case('wrong-layout', [consumer, wrong], [], ['test.shared'], 1, 1, reasons=['Update Shared Assets'])
    case('legacy-provider', [consumer, old], [], ['test.shared'], 1, 1, reasons=['Update Shared Assets'])
    case('duplicate-provider', [consumer, provider, old], [], [], 0, 3, reasons=['Remove duplicate copies'])
    collision = package('collision', 'test.collision', clip_id='test.shared.idle')
    case('conflicting-provider', [consumer, provider, collision], [], [], 0, 3,
         reasons=['Repair Shared Assets', 'Conflicting animation library'])
    case('broken-provider', [consumer, broken, unrelated], ['test.unrelated'], [], 1, 2,
         reasons=['Invalid shared animation playback', 'Install Shared Assets'])
    a = package('chain_a', 'test.a', provides=['layout.v1'], dependencies=[requirement('test.b')])
    b = package('chain_b', 'test.b', provides=['layout.v1'], dependencies=[requirement('test.c')])
    case('transitive', [a, b], [], [], 0, 2, reasons=['Install Shared Assets', 'Repair Shared Assets'])
    case('transitive-reversed', [b, a], [], [], 0, 2)
    ca = package('cycle_a', 'test.a', provides=['layout.v1'], dependencies=[requirement('test.b')])
    cb = package('cycle_b', 'test.b', provides=['layout.v1'], dependencies=[requirement('test.a')])
    case('complete-cycle', [ca, cb], [], ['test.a', 'test.b'], 2, 0)
    for label, arguments in [
        ('self', dict(dependencies=[requirement('test.invalid')])),
        ('duplicate-requirement', dict(dependencies=[requirement('test.shared')] * 2)),
        ('duplicate-contract', dict(provides=['layout.v1'] * 2)),
        ('wrong-fields', dict(dependencies=[dict(id='test.shared', contract='layout.v1', name='Shared', extra=True)])),
        ('bad-label', dict(dependencies=[requirement('test.shared', name='Shared\nAssets')])),
    ]:
        invalid = package(label, 'test.invalid', **arguments)
        case(label, [invalid, unrelated], ['test.unrelated'], [], 1, 1)

    multiple = package('multiple', 'test.multiple')
    extra = root / 'metadata/multiple/MortalShell2/Content/CSS/Packages/test.shared'
    extra.mkdir()
    extra.joinpath('manifest.json').write_bytes(
        (root / 'metadata/provider/MortalShell2/Content/CSS/Packages/test.shared/manifest.json').read_bytes())
    pak = fixtures / 'multiple/CSS_multiple_P.pak'
    pak.unlink()
    subprocess.run([str(args.repak.resolve()), 'pack', str(root / 'metadata/multiple'), str(pak),
                    '--version', 'V8B'], check=True, stdout=subprocess.DEVNULL)
    case('multiple-manifests', [multiple, unrelated], ['test.unrelated'], [], 1, 1,
         reasons=['must contain one manifest'])

    protected = {}
    if args.migrated:
        migrated = args.migrated.resolve(strict=True)
        baseline = json.loads(args.baseline.read_text())
        targets = ['CSS_EveStellarBlade_eins0fx_P', 'CSS_UnholyGenessa_eins0fx_P', 'CSS_CommanderWhite_eins0fx_P']
        ids = ['eins0fx.seduxtress', 'eins0fx.unholygenessa', 'eins0fx.commanderwhite']
        current = [[migrated / (stem + suffix) for suffix in ('.pak', '.utoc', '.ucas')] for stem in targets]
        shared = [migrated / ('CSS_SharedAssets_P' + suffix) for suffix in ('.pak', '.utoc', '.ucas')]
        originals = {}
        for row in baseline['packages']:
            if row['container'] not in targets + ['CSS_LocomotionSharedAssets_P']:
                continue
            files = []
            for details in row['files'].values():
                file = Path(details['path'])
                if digest(file) != details['sha256']:
                    raise ValueError('Installed baseline changed: ' + str(file))
                protected[str(file)] = details['sha256']
                files.append(file)
            originals[row['container']] = files
        legacy_library = originals['CSS_LocomotionSharedAssets_P']
        case('migrated-clean', [*current, shared], ids, ['css.locomotion'], 4, 0, face_matches=9)
        case('migrated-missing-shared', current, [], [], 0, 3, empty_message='Install CSS Shared Assets')
        case('migrated-old-shared', [*current, legacy_library], [], ['css.locomotion'], 1, 3,
             reasons=['Update CSS Shared Assets'])
        case('migrated-both-shared', [*current, shared, legacy_library], [], [], 0, 5,
             reasons=['Remove duplicate copies of CSS Shared Assets'])
        case('shared-alone', [shared], [], ['css.locomotion'], 1, 0, face_matches=0)
        legacy_outfits = [originals[stem] for stem in targets]
        case('legacy-unchanged', [*legacy_outfits, legacy_library], ids, ['css.locomotion'], 4, 0, face_matches=7)
        case('legacy-with-new-library', [*legacy_outfits, shared], ids, ['css.locomotion'], 4, 0, face_matches=0)
        for index in range(3):
            mixed = copy.copy(current)
            mixed[index] = legacy_outfits[index]
            case('partial-' + str(index), [*mixed, shared], ids, ['css.locomotion'], 4, 0,
                 face_matches=9 - [6, 2, 1][index])
    expected = root / 'expectations.json'
    expected.write_text(json.dumps(expectations, indent=2) + '\n')
    command = [str(args.test_binary.resolve()), str(cases), str(expected)]
    with (root / 'validation.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    for path, checksum in protected.items():
        if digest(Path(path)) != checksum:
            raise ValueError('Protected baseline changed during test')
    (root / 'validation.json').write_text(json.dumps(dict(
        passed=result.returncode == 0, scenarios=len(expectations), command=command,
        returncode=result.returncode, baseline=protected,
        scope='Native package/catalog dependency handling and facial revision selection; no runtime asset playback.'
    ), indent=2) + '\n')
    result.check_returncode()
    print(f'{len(expectations)} dependency and upgrade scenarios passed')


if __name__ == '__main__':
    main()
