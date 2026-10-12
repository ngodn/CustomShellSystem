"""Rebuild accepted Eve payloads with one cooked rig and refresh shared compatibility metadata."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

CSS = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(CSS/'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK
from css_facial_resources import encode, externalize, resolve
from css_package import verify

EVE = 'CSS_EveStellarBlade_eins0fx_P'
SHARED = 'CSS_SharedAssets_P'
RIG = 'MortalShell2/Content/CSS/eins0fx/EveSB/CR_Body'


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2)+'\n')


def link_or_copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        if sha(source) != sha(target):
            raise ValueError('Existing staged file differs: '+str(target))
        return
    try:
        os.link(source, target)
    except OSError:
        subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('rig', 'baseline', 'output', 'dotnet', 'mappings'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--resume-metadata', action='store_true')
    args = parser.parse_args()
    rig, baseline = args.rig.resolve(strict=True), args.baseline.resolve(strict=True)
    out = args.output.resolve()
    if out.exists() != args.resume_metadata or not out.is_relative_to((CSS/'work').resolve()):
        raise ValueError('Expected a new private CSS work directory')
    proof = read(rig/'verified.json')
    cook = rig/'windows-cook1'
    if not proof['passed'] or read(cook/'exit.json') != dict(returncode=0, sources_unchanged=True):
        raise ValueError('Rig or Windows cook is unverified')
    previous = read(baseline/'assembly.json')
    if not previous['passed']:
        raise ValueError('Accepted package verification is missing')
    out.mkdir(exist_ok=args.resume_metadata)
    protected = {}

    def protect(path, expected=None):
        actual = sha(path)
        if expected is not None and actual != expected:
            raise ValueError('Input changed: '+str(path))
        protected[str(path)] = actual
        return actual

    def run(label, command):
        command = list(map(str, command))
        write(out/(label+'-command.json'), command)
        env = os.environ.copy()
        env['DOTNET_ROOT'] = str(args.dotnet.parent)
        with (out/(label+'.log')).open('w') as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        write(out/(label+'-exit.json'), dict(returncode=result.returncode))
        result.check_returncode()

    for path in (Path(__file__), rig/'verified.json', baseline/'assembly.json', args.mappings):
        protect(path)
    installed = GAME/'Content/Paks/~mods'
    for relative, expected in previous['files'].items():
        protect(installed/relative, expected)
    sources = {}
    for path, expected in previous['inputs'].items():
        if Path(path).is_file():
            sources.setdefault(expected, Path(path))
    payloads = dict(previous['payloads'][EVE])
    cooked = cook/'project/Saved/Cooked/Windows/CSSShared/Content/CSS/eins0fx/EveSB'
    replacements = {RIG+suffix: cooked/('CR_Body'+suffix) for suffix in ('.uasset', '.uexp')}
    for relative, expected in payloads.items():
        source = replacements.get(relative) or sources.get(expected)
        if source is None:
            raise ValueError('Accepted payload unavailable: '+relative)
        actual = protect(source, None if relative in replacements else expected)
        payloads[relative] = actual
        link_or_copy(source, out/'stage'/relative)
    changed = {name for name, digest in payloads.items() if previous['payloads'][EVE][name] != digest}
    if changed != set(replacements):
        raise ValueError('Unexpected payload replacement set')
    converter = Converter(CSS/'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out)
    (out/'pack'/EVE).mkdir(parents=True, exist_ok=args.resume_metadata)
    if args.resume_metadata:
        if read(out/'convert-exit.json')['returncode'] != 0:
            raise ValueError('Cannot resume an incomplete conversion')
    else:
        run('convert', [converter.retoc, 'to-zen', out/'stage', out/'pack'/EVE/(EVE+'.utoc'),
                        '--version', 'UE5_6', '--no-parallel'])
    run('container', [converter.retoc, 'verify', out/'pack'/EVE/(EVE+'.utoc')])
    manifests, paths = {}, {}
    for name in (EVE, SHARED):
        source = baseline/'metadata'/name
        metadata = out/'metadata'/name
        for path in source.rglob('*'):
            if path.is_file():
                target = metadata/path.relative_to(source)
                target.parent.mkdir(parents=True, exist_ok=True)
                protect(path)
                if path.name == 'manifest.json':
                    shutil.copy2(path, target)
                else:
                    link_or_copy(path, target)
        paths[name], = metadata.rglob('manifest.json')
        manifests[name] = read(paths[name])
    before = copy.deepcopy(manifests)
    for suffix in ('.utoc', '.ucas'):
        path = out/'pack'/EVE/(EVE+suffix)
        manifests[EVE]['containers'][suffix] = dict(file=path.name, bytes=path.stat().st_size, sha256=sha(path))
        link_or_copy(installed/SHARED/(SHARED+suffix), out/'pack'/SHARED/(SHARED+suffix))
    shared = manifests[SHARED]
    resources = {r['file']: (paths[SHARED].parent/r['file']).read_bytes() for r in shared['facial_resources']}
    inline = resolve(shared, resources)
    library = inline['catalog']['animation_library']
    updates = 0
    for adapters in [library['adapters'], *[face['adapters'] for face in library['faces']]]:
        for adapter in adapters:
            if adapter['source_outfit'] == manifests[EVE]['id']:
                adapter['source_toc_sha256'] = manifests[EVE]['containers']['.utoc']['sha256']
                adapter['source_bulk_bytes'] = manifests[EVE]['containers']['.ucas']['bytes']
                updates += 1
    manifests[SHARED], external = externalize(inline)
    if not updates or external != resources or manifests[EVE]['catalog'] != before[EVE]['catalog']:
        raise ValueError('Unexpected catalog or facial-resource changes')
    for name in (EVE, SHARED):
        paths[name].write_bytes(encode(manifests[name]))
        run(name+'-metadata', [DEFAULT_REPAK, 'pack', out/'metadata'/name,
                               out/'pack'/name/(name+'.pak'), '--version', 'V8B'])
        if name == EVE and verify(out/'pack'/name) != manifests[name]:
            raise ValueError('Packaged metadata differs')
    run('shared-metadata-readback', [DEFAULT_REPAK, 'unpack', out/'pack'/SHARED/(SHARED+'.pak'),
                                     '--output', out/'shared-metadata-readback'])
    for path in (out/'metadata'/SHARED).rglob('*'):
        if path.is_file() and sha(path) != sha(out/'shared-metadata-readback'/path.relative_to(out/'metadata'/SHARED)):
            raise ValueError('Shared metadata or facial resource differs')
    others = sorted({Path(name).parts[0] for name in previous['files']}-{EVE, SHARED})
    for name in others:
        folder = out/'pack'/name
        folder.mkdir()
        for suffix in ('.pak', '.utoc', '.ucas'):
            (folder/(name+suffix)).symlink_to(installed/name/(name+suffix))
    shutil.copy2(baseline/'variants.json', out/'variants.json')
    run('catalog', [CSS/'build/host/css_animation_library_tests', '--audited-peony-pack', out/'pack', out/'variants.json'])
    mount = out/'containers'
    converter.base_containers(GAME, mount)
    for name in (EVE, SHARED, *others):
        for path in (out/'pack'/name).iterdir():
            (mount/path.name).symlink_to(path.resolve())
    packages = baseline/'packages.txt'
    references = baseline/'references.txt'
    for path in (packages, references):
        protect(path)
    run('imports', [args.dotnet, CSS/'tools/AssetDependencyAudit/bin/Release/net10.0/AssetDependencyAudit.dll',
                    mount, args.mappings, packages, references, out/'imports.json'])
    if not read(out/'imports.json')['passed']:
        raise ValueError('Package imports did not resolve')
    (out/'rig-packages.txt').write_text('/Game/CSS/eins0fx/EveSB/CR_Body\n')
    run('decode', [args.dotnet, CSS/'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll',
                   mount, args.mappings, out/'rig-packages.txt', out/'decoded.json'])
    run('roundtrip', [converter.retoc, 'to-legacy', mount, out/'readback', '--version', 'UE5_6',
                      '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/EveSB/'])
    for relative, expected in payloads.items():
        if Path(relative).suffix != '.uasset' and sha(out/'readback'/relative) != expected:
            raise ValueError('Cooked export or bulk changed: '+relative)
    for path, expected in protected.items():
        if sha(Path(path)) != expected:
            raise ValueError('Protected input changed: '+path)
    write(out/'assembly.json', dict(passed=True, installed=False, inputs=protected,
        payloads=payloads, changed_payloads=sorted(changed), shared_adapter_updates=updates,
        files={str(p.relative_to(out/'pack')): sha(p) for p in (out/'pack').rglob('*') if p.is_file()},
        pending=['Independent cooked rig-default comparison', 'Native gameplay acceptance']))
    print('Eve rig candidate packaged; independent rig-default check and gameplay remain')


if __name__ == '__main__':
    main()
