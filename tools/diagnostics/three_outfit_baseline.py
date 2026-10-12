"""Read installed package identities and overlapping exports without changing the game."""
import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PACKAGES = (
    'CSS_EveStellarBlade_eins0fx_P',
    'CSS_UnholyGenessa_eins0fx_P',
    'CSS_CommanderWhite_eins0fx_P',
    'CSS_SharedAssets_P',
    'CSS_AstralSharedAssets_P',
    'CSS_LocomotionSharedAssets_P',
)


def identity(path):
    before = path.stat()
    with path.open('rb') as stream:
        checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise RuntimeError(f'Input changed while hashing: {path}')
    return dict(path=str(path.resolve()), bytes=after.st_size,
                mtime_ns=after.st_mtime_ns, sha256=checksum)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--retoc', type=Path,
                        default=ROOT / 'build/retoc-css-target/release/retoc')
    args = parser.parse_args()
    game = args.game.resolve(strict=True)
    retoc = args.retoc.resolve(strict=True)
    output = args.output.resolve()
    if not output.resolve().is_relative_to((ROOT / 'work').resolve()) or output.exists():
        raise ValueError('Use a new output directory under the CSS work directory')
    output.mkdir(parents=True)
    css = game / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    selector = json.loads((css / 'core.json').read_text())
    filename = selector['file']
    if Path(filename).name != filename:
        raise ValueError('Core selector must name a file, not a path')
    receipt = json.loads((ROOT / 'work/cine-animation-port/gameplay-cancel-regression-1'
                         '/installation/deployment.json').read_text())
    core = identity(css / 'cores' / filename)
    historical = ROOT / 'work/cine-animation-port/outfit-compatibility'
    report = dict(schema=1, captured_utc=datetime.now(timezone.utc).isoformat(),
                  scope='Read-only installed-file identities and export ownership; not live acceptance',
                  retoc=identity(retoc), selector=selector, core=core,
                  core_matches_interruption_receipt=core['sha256'] == receipt['sha256'],
                  live_acceptance='pending', packages=[], overlaps={}, historical_meshes=[])
    providers = defaultdict(list)
    for name in PACKAGES:
        base = game / 'Content/Paks/~mods' / name
        files = {suffix: identity(base / (name + suffix))
                 for suffix in ('.pak', '.utoc', '.ucas')}
        result = subprocess.run([str(retoc), 'list', '--hash', '--path',
                                 str(base / (name + '.utoc'))],
                                capture_output=True, text=True, check=True)
        (output / (name + '-index.txt')).write_text(result.stdout)
        exports = []
        for line in result.stdout.splitlines():
            if 'ExportBundleData' not in line:
                continue
            fields = line.split()
            path = fields[-1]
            if '/Content/' not in path or not path.endswith('.uasset'):
                raise ValueError(f'Unexpected export index row: {line}')
            package = '/Game/' + path.split('/Content/', 1)[1].removesuffix('.uasset')
            export = dict(package=package, indexed_chunk_hash=fields[2], index_row=line)
            exports.append(export)
            providers[package].append(dict(container=name, indexed_chunk_hash=fields[2], index_row=line))
        if not exports or len({e['package'] for e in exports}) != len(exports):
            raise ValueError(f'Empty or ambiguous index: {name}')
        row = dict(container=name, files=files, exports=exports)
        old_manifest = historical / name / 'manifest.json'
        if old_manifest.exists():
            manifest = json.loads(old_manifest.read_text())
            row['historical_manifest'] = identity(old_manifest)
            row['matches_historical_payloads'] = all(
                files[extension]['sha256'] == item['sha256']
                for extension, item in manifest['containers'].items())
            row['historical_version'] = manifest['version']
        report['packages'].append(row)
        print(name, len(exports), 'exports', flush=True)
    report['overlaps'] = {p: rows for p, rows in sorted(providers.items()) if len(rows) > 1}
    old_report = historical / 'report.json'
    report['historical_mesh_report'] = identity(old_report)
    report['historical_meshes'] = json.loads(old_report.read_text())['results']
    status_path = css / 'runtime/status.json'
    status = json.loads(status_path.read_text())
    report['last_status'] = dict(mtime_ns=status_path.stat().st_mtime_ns,
                                 pid=status.get('pid'), applied=status.get('applied'),
                                 animation=status.get('animation'), mesh=status.get('mesh'))
    processes = []
    for entry in Path('/proc').iterdir():
        if not entry.name.isdigit():
            continue
        try:
            executable = (entry / 'comm').read_text().strip()
            if 'Mortal' in executable or 'Sparta' in executable:
                processes.append(dict(pid=int(entry.name), comm=executable))
        except (OSError, UnicodeError):
            continue
    report['matching_game_processes'] = processes
    (output / 'baseline.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Overlapping export paths:', len(report['overlaps']))
    print('Core matches previous receipt:', report['core_matches_interruption_receipt'])


if __name__ == '__main__':
    main()
