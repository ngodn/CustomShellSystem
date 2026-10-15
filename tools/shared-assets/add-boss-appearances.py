"""Add accepted boss adaptations to shared assets without recooking their payloads.

Python 3.14. The baseline and trial remain immutable. This produces a verified
candidate only; mounted assets must be installed through a separate restart.
"""
import argparse
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME, sha
from css_convert import Converter, DEFAULT_REPAK, asset_info, replace_references


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--boss-trial', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    baseline = args.baseline.resolve(strict=True)
    trial = args.boss_trial.resolve(strict=True)
    out = args.output.resolve()
    require(not out.exists() and out.is_relative_to((ROOT / 'build').resolve()),
            'Use a new directory beneath build')
    require(shutil.disk_usage(ROOT / 'build').free > 2 * 1024**3, 'Need 2 GiB free')
    spec_path = ROOT / 'packaging/boss-appearances.json'
    spec = read(spec_path)
    name = spec['container']
    require(name == 'CSS_SharedAssets_P' and spec['schema'] == 1, 'Unknown asset owner')
    rows = spec['characters']
    require(len(rows) == 4 and len({r['id'] for r in rows}) == 4, 'Expected four accepted bosses')
    proof = read(baseline / 'verified.json')
    require(proof['passed'] and read(trial / 'verified.json')['passed'], 'Unverified inputs')
    pins = {}

    def pin(path, expected=None):
        digest = sha(path)
        require(expected is None or digest == expected, 'Changed input: ' + str(path))
        pins[str(path)] = digest
        return digest

    for path in (spec_path, Path(__file__), baseline / 'verified.json',
                 trial / 'verified.json', trial / 'decoded.json', trial / 'payloads.json'):
        pin(path)
    base = baseline / 'pack' / name
    for suffix in ('.pak', '.utoc', '.ucas'):
        filename = name + suffix
        expected = proof['files'][name + '/' + filename]
        pin(base / filename, expected)
    trial_files = read(trial / 'package.json')['files']
    for filename, digest in trial_files.items():
        pin(trial / filename, digest)
    trial_payloads = {r['target']: r for r in read(trial / 'payloads.json')}
    mapping = {r['trial_' + field]: r[field] for r in rows for field in ('mesh', 'skeleton')}
    require(len(mapping) == 8 and all(len(a) == len(b) and b.startswith('/Game/CSS/eins0fx/Bosses/')
                                    for a, b in mapping.items()), 'Invalid permanent asset paths')
    out.mkdir(parents=True)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out)
    for tool in (converter.retoc, DEFAULT_REPAK):
        pin(tool)
    mount = out / 'containers'
    converter.base_containers(GAME, mount)
    for suffix in ('.utoc', '.ucas'):
        (mount / (name + suffix)).symlink_to(base / (name + suffix))
    stage = out / 'stage'
    converter.run(converter.retoc, 'to-legacy', mount, stage, '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/Shared/')
    preserved = {str(p.relative_to(stage)): sha(p) for p in stage.rglob('*') if p.is_file()}
    require(sum(p.endswith('.uasset') for p in preserved) == 283, 'Shared package inventory changed')
    added = {}
    for old, new in mapping.items():
        prefix = 'MortalShell2/Content/' + old.removeprefix('/Game/')
        files = [key for key in trial_payloads if key.rsplit('.', 1)[0] == prefix]
        require(any(key.endswith('.uasset') for key in files), 'Missing trial asset: ' + old)
        for relative in files:
            source = trial / 'stage' / relative
            pin(source, trial_payloads[relative]['sha256'])
            original = source.read_bytes()
            edits = []
            data, count = replace_references(original, mapping, edits)
            undo = bytearray(data)
            for offset, value in edits:
                undo[offset:offset + len(value)] = value
            require(bytes(undo) == original, 'Relocation changed data outside package references')
            target = stage / ('MortalShell2/Content/' + new.removeprefix('/Game/') + source.suffix)
            require(not target.exists(), 'Duplicate asset provider')
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            added[str(target.relative_to(stage))] = dict(source=str(source), sha256=sha(target), replacements=count)
    pack = out / 'pack' / name
    pack.mkdir(parents=True)
    toc = pack / (name + '.utoc')
    converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', toc)
    metadata = out / 'metadata'
    converter.run(DEFAULT_REPAK, 'unpack', base / (name + '.pak'), '--output', metadata)
    manifest_path, = metadata.rglob('manifest.json')
    manifest = read(manifest_path)
    previous = copy.deepcopy(manifest)
    resources = {str(p.relative_to(metadata)): sha(p) for p in metadata.rglob('*') if p.is_file() and p != manifest_path}
    for suffix in ('.utoc', '.ucas'):
        p = toc.with_suffix(suffix)
        manifest['containers'][suffix] = dict(file=p.name, bytes=p.stat().st_size, sha256=sha(p))
    write(manifest_path, manifest)
    require({k:v for k,v in manifest.items() if k != 'containers'} ==
            {k:v for k,v in previous.items() if k != 'containers'}, 'Shared catalog changed')
    converter.run(DEFAULT_REPAK, 'pack', metadata, toc.with_suffix('.pak'), '--version', 'V8B')
    converter.run(DEFAULT_REPAK, 'unpack', toc.with_suffix('.pak'), '--output', out / 'metadata-readback')
    for p in metadata.rglob('*'):
        if p.is_file():
            require(sha(p) == sha(out / 'metadata-readback' / p.relative_to(metadata)), 'Metadata roundtrip changed')
    for relative, digest in resources.items():
        require(sha(metadata / relative) == digest, 'Facial resources changed')
    for suffix in ('.utoc', '.ucas'):
        link = mount / (name + suffix)
        link.unlink()
        link.symlink_to(toc.with_suffix(suffix))
    for owner in ('CSS_EveStellarBlade_eins0fx_P', 'CSS_UnholyGenessa_eins0fx_P', 'CSS_CommanderWhite_eins0fx_P'):
        for suffix in ('.utoc', '.ucas'):
            p = GAME / 'Content/Paks/~mods' / owner / (owner + suffix)
            pin(p)
            (mount / p.name).symlink_to(p)
    converter.run(converter.retoc, 'to-legacy', mount, out / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/Shared/', '-f', '/CSS/eins0fx/Bosses/')
    for relative, digest in preserved.items():
        require(sha(out / 'readback' / relative) == digest, 'Existing shared payload changed: ' + relative)
    for relative, row in added.items():
        path = out / 'readback' / relative
        if path.suffix == '.uasset':
            # IoStore reconstructs cooked headers. The editor-only IsAsset flag
            # is not retained; export bodies and decoded objects are checked below.
            before_info = asset_info((stage / relative).read_bytes())
            after_info = asset_info(path.read_bytes())
            for info in (before_info, after_info):
                for export in info['exports']:
                    export.pop('asset')
            require(before_info == after_info, 'Boss header identity changed: ' + relative)
        else:
            require(sha(path) == row['sha256'], 'Boss payload roundtrip changed: ' + relative)
    packages = sorted(mapping.values())
    (out / 'packages.txt').write_text('\n'.join(packages) + '\n')
    (out / 'references.txt').write_text('\n'.join(p + '.' + p.rsplit('/', 1)[1] for p in packages) + '\n')
    dotnet = Path('/home/eins0fx/.local/share/mise/installs/dotnet/10.0.401/dotnet')
    mappings = GAME / 'Binaries/Win64/ue4ss/MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241+1339-d7e7826d.usmap'
    for label, tool, extra in (
        ('decoded', 'AssetReadback', [out / 'decoded.json']),
        ('imports', 'AssetDependencyAudit', [out / 'references.txt', out / 'imports.json']),
    ):
        reader = ROOT / 'tools' / tool / 'bin/Release/net10.0' / (tool + '.dll')
        pin(reader)
        command = list(map(str, [dotnet, reader, mount, mappings, out / 'packages.txt', *extra]))
        write(out / (label + '-command.json'), command)
        with (out / (label + '.log')).open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        write(out / (label + '-exit.json'), dict(returncode=result.returncode))
        result.check_returncode()
    require(read(out / 'imports.json')['passed'], 'Boss imports failed')
    before, after = read(trial / 'decoded.json'), read(out / 'decoded.json')

    def normalize(value):
        if isinstance(value, str):
            for old, new in mapping.items():
                value = value.replace(new, old)
            return value
        if isinstance(value, list):
            return [normalize(v) for v in value]
        if isinstance(value, dict):
            return {k:normalize(v) for k,v in value.items()}
        return value

    for old, new in mapping.items():
        require(normalize(after[new]) == before[old], 'Decoded asset changed: ' + new)
    require(all(sha(Path(p)) == h for p,h in pins.items()), 'Inputs changed during assembly')
    write(out / 'verified.json', dict(passed=True, installed=False, inputs=pins,
        mapping=mapping, preserved_payloads=preserved, added_payloads=added,
        files={p.name:sha(p) for p in pack.iterdir()},
        shared_packages=283, boss_packages=8, decoded_assets_preserved=True,
        scope='Existing shared payloads and full decoded boss assets preserved; catalog and facial resources unchanged. Installation and live binding checks remain.'))
    print('Verified 283 existing shared packages and eight permanent boss packages', flush=True)


if __name__ == '__main__':
    main()
