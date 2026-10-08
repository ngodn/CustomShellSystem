"""Rebuild a verified namespace trial with its final, unique IoStore identity."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
RETOC = ROOT / 'build/retoc-css-target/release/retoc'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text())


def write(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n')


def chunks(listing):
    rows, headers = {}, []
    for line in listing.splitlines():
        fields = line.split(maxsplit=5)
        if len(fields) != 6 or not re.fullmatch('[0-9a-f]{24}', fields[1]):
            raise ValueError('Unrecognized chunk listing: ' + line)
        if fields[3] == 'ContainerHeader':
            headers.append(fields)
            continue
        if fields[1] in rows:
            raise ValueError('Duplicate chunk identity')
        # Includes content hash, chunk type, size and package-store dependencies.
        rows[fields[1]] = fields[2:]
    if len(headers) != 1 or not rows:
        raise ValueError('Expected one container header and nonempty asset data')
    return rows


def identity(info):
    matches = re.findall(r'container_id: FIoContainerId\(([0-9a-f]+)\)', info)
    if len(matches) != 1:
        raise ValueError('Expected one container identity')
    return matches[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'output', 'dotnet', 'mappings'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--container', required=True,
                        choices=('CSS_EveStellarBlade_eins0fx_P', 'CSS_CommanderWhite_eins0fx_P'))
    args = parser.parse_args()
    source, output = args.source.resolve(strict=True), args.output.resolve()
    if output.exists():
        raise FileExistsError(output)
    receipt = read(source / 'verification.json')
    if receipt['passed'] is not True:
        raise ValueError('Source trial did not pass')
    toc_names = [name for name in receipt['files'] if name.endswith('.utoc')]
    if len(toc_names) != 1:
        raise ValueError('Expected one source container')
    original = source / toc_names[0]
    if original.stem == args.container:
        raise ValueError('Source already has requested identity')
    protected = {}
    for path in (source / 'verification.json', source / 'decoded.json', RETOC):
        protected[str(path)] = sha(path)
    for name, digest in receipt['files'].items():
        path = source / name
        if sha(path) != digest:
            raise ValueError('Changed source container: ' + name)
        protected[str(path)] = digest
    for relative, digest in receipt['payloads'].items():
        if Path(relative).is_absolute() or '..' in Path(relative).parts:
            raise ValueError('Invalid payload path')
        path = source / 'stage' / relative
        if sha(path) != digest:
            raise ValueError('Changed staged payload: ' + relative)
    required = original.with_suffix('.ucas').stat().st_size + 128 * 1024**2
    if shutil.disk_usage(output.parent).free < required:
        raise ValueError(f'Need {required} free bytes for a new container')
    output.mkdir()
    (output / 'stage').symlink_to(source / 'stage', target_is_directory=True)

    def run(label, command):
        command = list(map(str, command))
        write(output / (label + '-command.json'), command)
        with (output / (label + '.log')).open('x') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        write(output / (label + '-exit.json'), dict(returncode=result.returncode))
        result.check_returncode()
        return (output / (label + '.log')).read_text()

    target = output / (args.container + '.utoc')
    before = run('original-index', [RETOC, 'list', '--hash', '--size', '--store', original])
    old_id = identity(run('original-info', [RETOC, 'info', original]))
    run('assemble', [RETOC, 'to-zen', source / 'stage', target, '--version', 'UE5_6', '--no-parallel'])
    run('verify', [RETOC, 'verify', target])
    after = run('candidate-index', [RETOC, 'list', '--hash', '--size', '--store', target])
    new_id = identity(run('candidate-info', [RETOC, 'info', target]))
    if old_id == new_id or chunks(before) != chunks(after):
        raise ValueError('Container identity unchanged or asset chunks/store entries differ')
    mount = output / 'containers'
    mount.mkdir()
    for old in (source / 'containers').iterdir():
        if old.is_file() and old.stem != original.stem:
            (mount / old.name).symlink_to(old.resolve(strict=True))
    for suffix in ('.pak', '.utoc', '.ucas'):
        path = target.with_suffix(suffix)
        (mount / path.name).symlink_to(path)
    packages = output / 'packages.txt'
    packages.write_text('\n'.join(sorted(read(source / 'decoded.json'))) + '\n')
    run('readback', [args.dotnet, ROOT / 'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll',
                     mount, args.mappings, packages, output / 'decoded.json', '--shader-maps'])
    if read(source / 'decoded.json') != read(output / 'decoded.json'):
        raise ValueError('Independent decoded export comparison differs')
    for path, digest in protected.items():
        if sha(Path(path)) != digest:
            raise ValueError('Source changed during finalization: ' + path)
    write(output / 'verification.json', dict(
        passed=True, installed=False, packages=receipt['packages'],
        compared=len(read(source / 'decoded.json')), asset_chunks=len(chunks(after)),
        original_container_id=old_id, container_id=new_id, protected=protected,
        payloads=receipt['payloads'], files={target.with_suffix(s).name: sha(target.with_suffix(s))
                                            for s in ('.pak', '.utoc', '.ucas')},
        scope='Unique final container identity; unchanged asset chunks, store entries and decoded exports. No release metadata or live acceptance.'))
    print(f'Verified {receipt["packages"]} packages, unique container {new_id}')


if __name__ == '__main__':
    main()
