"""Build private shared metadata with one additional, verified outfit facial rig."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'tools'))
from css_facial_resources import encode, externalize, resolve


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--resources', type=Path, required=True)
    parser.add_argument('--containers', type=Path, required=True)
    parser.add_argument('--rig', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--repak', type=Path, default=ROOT/'build/repak/release/repak')
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    protected = {}

    def protect(path, expected=None):
        digest = sha(path)
        if expected is not None and digest != expected:
            raise ValueError('Input changed: '+str(path))
        protected[str(path.resolve())] = digest

    original = read(args.manifest)
    protect(args.manifest)
    protect(args.evidence)
    evidence = read(args.evidence)
    protect(args.rig, evidence['rig_sha256'])
    for name, digest in evidence['inputs'].items():
        protect(Path(name), digest)
    resources = {}
    for descriptor in original['facial_resources']:
        name = descriptor['file']
        if Path(name).name != name:
            raise ValueError('Facial resource must be a leaf filename')
        path = args.resources/name
        protect(path, descriptor['sha256'])
        resources[name] = path.read_bytes()
    inline = resolve(original, resources)
    baseline = copy.deepcopy(inline)
    rig = read(args.rig)
    library = inline['catalog']['animation_library']
    if any(face['id'] == rig['id'] for face in library['faces']):
        raise ValueError('Rig is already present')
    library['faces'].append(rig)
    packed, resources = externalize(inline)
    # This intermediate trial keeps existing payload identities until their
    # namespace migration. It must not masquerade as a consolidated release.
    containers = []
    for suffix, entry in original['containers'].items():
        if suffix not in ('.utoc', '.ucas') or Path(entry['file']).name != entry['file']:
            raise ValueError('Unexpected container')
        path = args.containers/entry['file']
        protect(path, entry['sha256'])
        if path.stat().st_size != entry['bytes']:
            raise ValueError('Container size differs')
        containers.append(path.resolve())
    if len(containers) != 2 or len({p.stem for p in containers}) != 1:
        raise ValueError('Expected one IoStore pair')
    args.output.mkdir(parents=True)
    relative = Path('MortalShell2/Content/CSS/Packages')/original['id']
    metadata = args.output/'metadata'/relative
    metadata.mkdir(parents=True)
    (metadata/'manifest.json').write_bytes(encode(packed))
    for name, data in resources.items():
        (metadata/name).write_bytes(data)
    mount = args.output/'pack'
    mount.mkdir()
    for path in containers:
        (mount/path.name).symlink_to(path)
    pak = mount/(containers[0].stem+'.pak')
    commands = [[str(args.repak), 'pack', str(args.output/'metadata'), str(pak), '--version', 'V8B'],
                [str(args.repak), 'unpack', str(pak), '--output', str(args.output/'readback')]]
    (args.output/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
    for command in commands:
        subprocess.run(command, check=True)
    folder = args.output/'readback'/relative
    decoded = resolve(read(folder/'manifest.json'), {name: (folder/name).read_bytes() for name in resources})
    if decoded != inline:
        raise ValueError('Packaged metadata differs')
    recovered = copy.deepcopy(decoded)
    if recovered['catalog']['animation_library']['faces'].pop() != rig or recovered != baseline:
        raise ValueError('Existing catalog changed')
    for name, digest in protected.items():
        if sha(Path(name)) != digest:
            raise ValueError('Input changed during packaging: '+name)
    report = dict(passed=True, installed=False, rig=rig['id'], entries=len(library['entries']),
                  faces=len(library['faces']), original_catalog_unchanged=True,
                  protected=protected, pak_sha256=sha(pak), manifest_bytes=(metadata/'manifest.json').stat().st_size,
                  facial_resources=packed['facial_resources'],
                  scope='Private metadata integration. Outfit-owned facial assets and matching namespace-aware core are separate dependencies. Not a release or completed namespace migration.')
    (args.output/'verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({key: report[key] for key in ('entries', 'faces', 'manifest_bytes', 'original_catalog_unchanged')}))


if __name__ == '__main__':
    main()
