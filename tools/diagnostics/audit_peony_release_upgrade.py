"""Check the Peony release with published legacy outfits, without installing. Python 3.14."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK

EVE = 'CSS_EveStellarBlade_eins0fx_P'
SHARED = 'CSS_SharedAssets_P'
UG = 'CSS_UnholyGenessa_eins0fx_P'
CW = 'CSS_CommanderWhite_eins0fx_P'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dotnet', type=Path, required=True)
    parser.add_argument('--mappings', type=Path, required=True)
    parser.add_argument('--auditor', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    authoring = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections'
    delivery = ROOT / 'dist/eve-v1.5.0'
    current = ROOT / 'work/three-outfit-rig-physics/peony-release5/pack'
    legacy = GAME / 'Content/CSS-namespace-trial1/backup'
    inputs = {
        EVE: (current / EVE, delivery / (EVE + '-v1.5.0.zip')),
        SHARED: (current / SHARED, delivery / (SHARED + '-v1.0.0-beta.12.zip')),
        UG: (legacy / UG, authoring / UG / 'dist/v0.2.0' / (UG + '-v0.2.0.zip')),
        CW: (legacy / CW, authoring / CW / 'dist' / (CW + '-v0.0.6-dev.zip')),
    }
    retoc = ROOT / 'build/retoc-css-target/release/retoc'
    auditor = args.auditor.resolve(strict=True)
    protected = {str(p): sha(p) for p in (retoc, DEFAULT_REPAK, auditor,
        auditor.parent / 'CUE4Parse.dll', args.dotnet, args.mappings,
        ROOT / 'build/host/css_package_dependency_tests')}
    manifests, inventories = {}, {}
    for name, (folder, archive_path) in inputs.items():
        archive_hash = sha(archive_path)
        sidecar = archive_path.with_suffix('.zip.sha256').read_text().split()[0]
        if archive_hash != sidecar:
            raise ValueError('Archive sidecar mismatch: ' + str(archive_path))
        protected[str(archive_path)] = archive_hash
        with zipfile.ZipFile(archive_path) as archive:
            for suffix in ('.pak', '.utoc', '.ucas'):
                path = folder / (name + suffix)
                digest = sha(path)
                with archive.open(name + '/' + name + suffix) as stream:
                    if hashlib.file_digest(stream, 'sha256').hexdigest() != digest:
                        raise ValueError('Retained container differs from published ZIP: ' + str(path))
                protected[str(path)] = digest
        meta = out / 'metadata' / name
        meta.mkdir(parents=True)
        subprocess.run([str(DEFAULT_REPAK), 'unpack', str(folder / (name + '.pak')),
                        '--output', str(meta)], check=True, stdout=subprocess.DEVNULL)
        manifest, = meta.rglob('manifest.json')
        manifests[name] = json.loads(manifest.read_text())
        listing = subprocess.check_output([str(retoc), 'list', '--hash', '--path',
                                          str(folder / (name + '.utoc'))], text=True)
        (out / (name + '-index.txt')).write_text(listing)
        exports = {}
        for line in listing.splitlines():
            if 'ExportBundleData' not in line:
                continue
            fields = line.split()
            path = fields[-1]
            if '/Content/' not in path or not path.endswith('.uasset'):
                raise ValueError('Unexpected export path')
            exports['/Game/' + path.split('/Content/', 1)[1][:-7]] = fields[2]
        if not exports:
            raise ValueError('Empty inventory')
        inventories[name] = exports
        print(name, manifests[name]['version'], len(exports), flush=True)
    write(out / 'inputs.json', protected)
    write(out / 'inventories.json', inventories)
    scenarios = {'peony-alone': [EVE, SHARED], 'legacy-genessa': [EVE, SHARED, UG],
                 'legacy-commander': [EVE, SHARED, CW], 'both-legacy': [EVE, SHARED, UG, CW]}
    expectations, results = [], []
    for case, names in scenarios.items():
        stage = out / case
        stage.mkdir()
        mount = stage / 'containers'
        Converter(retoc, DEFAULT_REPAK, stage).base_containers(GAME, mount)
        owners = {}
        for name in names:
            folder = inputs[name][0]
            for path in folder.iterdir():
                (mount / path.name).symlink_to(path)
            discover = out / 'cases' / case / name
            discover.mkdir(parents=True)
            for suffix in ('.pak', '.utoc', '.ucas'):
                (discover / (name + suffix)).symlink_to(folder / (name + suffix))
            for package, digest in inventories[name].items():
                owners.setdefault(package, []).append({'container': name, 'chunk_hash': digest})
        overlaps = {p: values for p, values in owners.items() if len(values) > 1}
        write(stage / 'overlaps.json', overlaps)
        packages = stage / 'packages.txt'
        packages.write_text('\n'.join(sorted(owners)) + '\n')
        references = stage / 'references.txt'
        references.write_text('')
        command = [str(args.dotnet), str(auditor), str(mount), str(args.mappings),
                   str(packages), str(references), str(stage / 'imports.json'), '--package-name-paths']
        write(stage / 'command.json', command)
        with (stage / 'imports.log').open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        write(stage / 'exit.json', {'returncode': result.returncode})
        if not (stage / 'imports.json').is_file():
            raise RuntimeError('Auditor produced no report; inspect ' + str(stage / 'imports.log'))
        audit = json.loads((stage / 'imports.json').read_text())
        results.append({'case': case, 'returncode': result.returncode,
                        'packages': len(audit['packages']), 'failures': len(audit['failures']),
                        'overlapping_packages': len(overlaps)})
        expectations.append({'name': case, 'outfits': sorted(manifests[n]['id'] for n in names if n != SHARED),
                             'libraries': ['css.locomotion'], 'loaded': len(names),
                             'rejected': 0, 'face_matches': 7})
        print(results[-1], flush=True)
    write(out / 'expectations.json', expectations)
    command = [str(ROOT / 'build/host/css_package_dependency_tests'), str(out / 'cases'),
               str(out / 'expectations.json')]
    with (out / 'catalog.log').open('w') as log:
        catalog = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    unchanged = all(sha(Path(path)) == digest for path, digest in protected.items())
    passed = unchanged and catalog.returncode == 0 and all(row['returncode'] == 0 for row in results)
    write(out / 'report.json', {'passed': passed, 'cases': results, 'catalog_returncode': catalog.returncode,
        'inputs_unchanged': unchanged, 'game_files_written': False,
        'scope': 'Container identity, conservative package imports and catalog revision matching. Not live rendering or soft-path execution.'})
    if not unchanged:
        raise ValueError('Protected input changed')
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
