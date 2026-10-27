#!/usr/bin/env python3
"""Build catalog/shader-donors.json: our materials that borrow game shaders, each paired with
the game material that has the identical static permutation.

A material instance whose parent is a game material carries a shader map that points into the
game's own shader libraries. A game build that lacks those exact shaders draws it grey. CSS can
then rebuild it at runtime as a dynamic instance of the donor (same permutation, shaders always
present in the player's own game) with our parameter values copied on top.

Every source (a release zip or a folder of .pak/.utoc/.ucas) is audited with
tools/ShaderDependencyAudit against the installed game. A material is listed when one of its
shader maps is shared (not inline). Its donor must match on every field that selects the
compiled permutation; anything else fails the build instead of guessing.

Python 3.14. Usage:
  build_shader_donors.py --game <MortalShell2> --work <dir> --out <json> SOURCE...
      [--include-report REPORT.json ...] [--stale-report REPORT.json=OUTFIT_ID@VERSION ...]

--include-report reuses a finished audit report as a source (a release zip that is no longer on
disk keeps its materials covered). --stale-report does the same for a release version that failed
the audit on this game build and lists it under stale_outfits, so CSS auto-rebuilds that outfit's
materials for players who never updated it. A stale report that passed is an error.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
DOTNET = Path.home() / '.local/share/mise/installs/dotnet/10.0.401/dotnet'
AUDIT = ROOT / 'tools/ShaderDependencyAudit/bin/Release/net10.0/ShaderDependencyAudit.dll'
RETOC = ROOT / 'build/retoc-css-target/release/retoc'
TABLE = re.compile(r'^(?:pakchunk\d+-Windows|global)\.utoc$')
SIGNATURE = ('Parent', 'StaticParametersRuntime', 'StaticParameters', 'BasePropertyOverrides',
             'bHasStaticPermutationResource', 'SubsurfaceProfile', 'bOverrideSubsurfaceProfile')
# Game materials our outfits were copied from. Candidates are these plus every game material
# with the same asset name as a target.
KNOWN_DONORS = (
    '/Game/MS1/Characters/NPC/Corrupted_Sester/Materials/MI_Genessa_Corrupt_top',
    '/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_body',
    '/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_skirt_NEW',
    '/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_top',
    '/Game/Sparta/Characters/NPCs/ShellKeeper/Art/Mesh/Textures/MI_ShellKeeper_Body_01',
    '/Game/Sparta/Characters/NPCs/ShellKeeper/Art/Mesh/Textures/MI_ShellKeeper_Hair_01',
    '/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_LadyKnight_1001',
    '/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_LadyKnight_Obsidian_1001',
    '/Game/Sparta/Characters/Shells/Smert/Mesh/Textures/MI_Smert_HoodRobe_01',
)


def signature(properties: dict) -> str:
    return json.dumps({key: properties.get(key) for key in SIGNATURE}, sort_keys=True)


def package_names(utoc: Path, work: Path) -> list[str]:
    subprocess.run([str(RETOC), 'manifest', str(utoc)], cwd=work, check=True, capture_output=True)
    store = work / 'pakstore.json'
    entries = json.loads(store.read_text())['oplog']['entries']
    store.unlink()
    return [entry['packagestoreentry']['packagename'] for entry in entries]


def audit(mount: Path, packages: list[str], work: Path, name: str, mapping: Path) -> dict:
    listing, output = work / f'{name}-packages.txt', work / f'{name}-audit.json'
    listing.write_text('\n'.join(sorted(set(packages))) + '\n')
    output.unlink(missing_ok=True)
    with (work / f'{name}-audit.log').open('w') as log:
        result = subprocess.run([str(DOTNET), str(AUDIT), str(mount), str(mapping), str(listing), str(output)],
                                stdout=log, stderr=subprocess.STDOUT)
    # A non-zero exit also reports missing game shaders, which old releases have on purpose
    # (that is the case this catalog exists for). Only an incomplete audit is a failure.
    report = json.loads(output.read_text()) if output.exists() else {}
    if not report or report.get('failures') or report.get('packages_inspected') != report.get('packages_requested'):
        raise RuntimeError(f'Audit failed for {name} (exit {result.returncode}); see {work / (name + "-audit.log")}')
    return report


def mount_dir(work: Path, name: str, paks: Path, extra: list[Path]) -> Path:
    mount = work / f'{name}-mount'
    if mount.exists():
        shutil.rmtree(mount)
    mount.mkdir()
    for f in [*paks.iterdir(), *extra]:
        if f.is_file() and f.suffix in ('.pak', '.utoc', '.ucas') and not (mount / f.name).exists():
            (mount / f.name).symlink_to(f.resolve())
    return mount


def containers(source: Path, work: Path, name: str) -> list[Path]:
    if source.is_dir():
        files = [f for f in source.iterdir() if f.suffix in ('.pak', '.utoc', '.ucas')]
    else:
        target = work / f'{name}-src'
        if target.exists():
            shutil.rmtree(target)
        target.mkdir()
        with zipfile.ZipFile(source) as archive:
            for member in archive.namelist():
                path = Path(member)
                if path.suffix in ('.pak', '.utoc', '.ucas') and '..' not in path.parts:
                    (target / path.name).write_bytes(archive.read(member))
        files = list(target.iterdir())
    if not any(f.suffix == '.utoc' for f in files):
        raise RuntimeError(f'No IoStore container in {source}')
    return files


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--game', type=Path, required=True, help='MortalShell2 folder of the installed game')
    parser.add_argument('--shared', type=Path, required=True, help='CSS_SharedAssets_P folder (outfit parents)')
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--steam-build', required=True, help='Steam build id of the installed game')
    parser.add_argument('sources', type=Path, nargs='*', help='Release zips or folders of containers')
    parser.add_argument('--include-report', type=Path, action='append', default=[])
    parser.add_argument('--stale-report', action='append', default=[], metavar='REPORT=ID@VERSION')
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    paks = args.game / 'Content/Paks'
    mapping = next((args.game / 'Binaries/Win64/ue4ss').glob('*.usmap'))
    shared = [f for f in args.shared.iterdir() if f.suffix in ('.pak', '.utoc', '.ucas')]

    # Donor candidates: known donors plus same-named game materials, read from the game itself.
    game_packages = set()
    for utoc in sorted(paks.glob('pakchunk*-Windows.utoc')):
        game_packages.update(package_names(utoc, args.work))

    stale: dict[str, set[str]] = {}
    reports: list[tuple[Path, Path]] = [(r, r) for r in args.include_report]
    for rule in args.stale_report:
        report_path, _, target = rule.partition('=')
        outfit, _, version = target.partition('@')
        if not report_path or not outfit or not version:
            raise SystemExit(f'Bad --stale-report {rule}')
        failed = json.loads(Path(report_path).read_text())
        if failed.get('passed') and not failed.get('missing_hashes'):
            raise SystemExit(f'{report_path} passed the audit; {outfit} {version} is not stale')
        stale.setdefault(outfit, set()).add(version)
        reports.append((Path(report_path), Path(report_path)))

    targets: dict[str, dict] = {}
    seen_sources = []
    for source in [*args.sources, *(r for r, _ in reports)]:
        if source.suffix == '.json':   # a finished report standing in for its source
            report = json.loads(source.read_text())
        else:
          stamp = f'{source.resolve()}:{source.stat().st_mtime_ns}'
          name = f'{source.stem}-{hashlib.sha1(stamp.encode()).hexdigest()[:10]}'
          cached = args.work / f'{name}-audit.json'
          if cached.exists():   # same file, same timestamp: a finished audit stays valid
            report = json.loads(cached.read_text())
          else:
            files = containers(source, args.work, name)
            packages = []
            for utoc in (f for f in files if f.suffix == '.utoc'):
                packages += package_names(utoc, args.work)
            own = [] if any(f.name.startswith('CSS_SharedAssets_P') for f in files) else shared
            report = audit(mount_dir(args.work, name, paks, files + own), packages, args.work, name, mapping)
            # Extracted release zips are gigabytes each; keep only the reports.
            shutil.rmtree(args.work / f'{name}-mount', ignore_errors=True)
            shutil.rmtree(args.work / f'{name}-src', ignore_errors=True)
        count = 0
        for material in report['materials']:
            if not any(entry['storage'] != 'inline' for entry in material['maps']):
                continue
            path = material['package']
            row = targets.setdefault(path, {'signatures': {}, 'sources': []})
            row['signatures'].setdefault(signature(material['properties']), []).append(str(source))
            row['sources'].append(str(source))
            count += 1
        seen_sources.append({'source': str(source), 'materials': len(report['materials']), 'shared': count})
        print(f'{source}: {count} of {len(report["materials"])} materials borrow game shaders', flush=True)

    names = {path.rsplit('/', 1)[1] for path in targets}
    candidates = sorted(set(KNOWN_DONORS) | {p for p in game_packages if p.rsplit('/', 1)[1] in names})
    donor_report = audit(mount_dir(args.work, 'donors', paks, []), candidates, args.work, 'donors', mapping)
    if donor_report.get('missing_hashes'):
        raise RuntimeError('A donor itself lacks shaders on this game build')
    donors = {m['package']: signature(m['properties']) for m in donor_report['materials']}
    shutil.rmtree(args.work / 'donors-mount', ignore_errors=True)

    result, problems = {}, []
    for path, row in sorted(targets.items()):
        if len(row['signatures']) != 1:
            problems.append(f'{path}: versions disagree on the static permutation ({len(row["signatures"])} variants)')
            continue
        wanted = next(iter(row['signatures']))
        same_name = path.rsplit('/', 1)[1]
        matches = sorted((p for p, s in donors.items() if s == wanted),
                         key=lambda p: (p.rsplit('/', 1)[1] != same_name, p))
        if not matches:
            problems.append(f'{path}: no game material with the same permutation')
            continue
        donor = matches[0]
        result[f'{path}.{path.rsplit("/", 1)[1]}'] = f'{donor}.{donor.rsplit("/", 1)[1]}'
    # Our own outfits must all be covered. Other authors' converted packs keep whatever has an
    # exact donor; the rest stay on their packaged shaders, listed in the report.
    ours = [line for line in problems if line.startswith('/Game/CSS/eins0fx/')]
    if ours:
        raise SystemExit('Shader donor build failed:\n  ' + '\n  '.join(ours))
    if problems:
        print('Left on packaged shaders (no exact game donor):\n  ' + '\n  '.join(problems))

    tables = {f.name: f.stat().st_size for f in sorted(paks.iterdir()) if TABLE.match(f.name)}
    document = {
        'schema': 1,
        'note': 'Generated by tools/authoring/shader-donors/build_shader_donors.py. Do not edit by hand.',
        'verified_builds': [{'steam_build': args.steam_build, 'pak_tables': tables}],
        'donors': result,
    }
    if stale:
        document['stale_outfits'] = {outfit: sorted(versions) for outfit, versions in sorted(stale.items())}
    args.out.write_text(json.dumps(document, indent=1, sort_keys=False) + '\n')
    (args.work / 'sources.json').write_text(json.dumps({'sources': seen_sources, 'not_covered': problems}, indent=1) + '\n')
    print(f'{len(result)} materials, {len(set(result.values()))} donors -> {args.out}')


if __name__ == '__main__':
    main()
