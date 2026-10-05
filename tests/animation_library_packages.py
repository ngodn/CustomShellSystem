"""Exercise native library discovery with real V8B metadata paks and synthetic containers."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repak', type=Path, required=True)
    parser.add_argument('--test-binary', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='css-library-packages-') as temporary:
        root = Path(temporary)
        paks = root / 'paks'
        paks.mkdir()
        cases = [('valid', 'valid'), ('conflict_a', 'collision'), ('conflict_b', 'collision'),
                 ('bad_asset', 'bad_asset'), ('corrupt_toc', 'corrupt_toc')]
        for label, clip in cases:
            package_id = f'test.{label}'
            stem = f'CSS_{label}_P'
            library = dict(id=package_id, name=label, author='test', version='0.1.0', entries=[dict(
                id=f'test.{clip}.idle', name='Idle', category='idle', playback='loop', slot='idle',
                assets=[dict(asset='/Game/CSS/Locomotion/Test/AN_Idle.AN_Idle',
                             skeleton='/Game/CSS/Shared/SKEL_Base.SKEL_Base')])])
            if label == 'bad_asset':
                library['entries'][0]['assets'][0]['asset'] = '/Game/Outside/AN_Idle.AN_Idle'
            containers = {}
            for suffix in ('.utoc', '.ucas'):
                path = paks / (stem + suffix)
                path.write_bytes(b'Metadata-only test. Not an Unreal asset container.\n')
                containers[suffix] = dict(file=path.name, bytes=path.stat().st_size,
                                          sha256=hashlib.sha256(path.read_bytes()).hexdigest())
            manifest = dict(format='CSS.Package', format_version=1, game='MortalShell2', engine='5.6',
                            kind='animation_library', id=package_id, name=label, author='test', version='0.1.0',
                            containers=containers, catalog=dict(schema=1, animation_library=library))
            staging = root / label
            metadata = staging / 'MortalShell2/Content/CSS/Packages' / package_id
            metadata.mkdir(parents=True)
            (metadata / 'manifest.json').write_text(json.dumps(manifest))
            subprocess.run([str(args.repak.resolve()), 'pack', str(staging), str(paks / (stem + '.pak')),
                            '--version', 'V8B'], check=True, stdout=subprocess.DEVNULL)
            if label == 'corrupt_toc':
                path = paks / (stem + '.utoc')
                content = bytearray(path.read_bytes())
                content[0] ^= 1
                path.write_bytes(content)
        subprocess.run([str(args.test_binary.resolve()), str(paks)], check=True)


if __name__ == '__main__':
    main()
