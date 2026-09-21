#!/usr/bin/env python3
"""Move legacy CSSX data out of Mods/CustomShellSystem into Mods/CSSX (game closed).

  cssx_migrate.py [--game DIR] [--dry-run]     plan or perform the migration
  cssx_migrate.py --restore STAMP              put every file back from Mods/CSSX/backup/STAMP

What moves: extensions/<id>/, state/extensions/<id>.json[.bak], logs/extensions/<id>/,
logs/cssx.jsonl*, output/extensions/<id>/, assets/cssx-logo.png.
What is deleted (after a verified backup): cssx.json and cores/cssx_core*.dll.
Never touched: core.json, cores/css_core*, dlls/main.dll, state/state.json,
catalog/, enabled.txt, any other mod. See docs/migration.md.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import shutil
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cssx import GAME, processes, sha  # noqa: E402


def plan(css: Path, cssx: Path) -> dict:
    """Describe every move and delete without touching anything."""
    moves: list[tuple[Path, Path]] = []
    deletes: list[Path] = []
    skipped: list[str] = []
    ext = css / 'extensions'
    if ext.is_dir():
        for folder in sorted(p for p in ext.iterdir() if p.is_dir() and not p.is_symlink()):
            target = cssx / 'extensions' / folder.name
            if target.exists():
                skipped.append(f'extensions/{folder.name}: already present in Mods/CSSX, left in place')
            else:
                moves.append((folder, target))
    state = css / 'state/extensions'
    if state.is_dir():
        for file in sorted(state.glob('*.json*')):
            target = cssx / 'state' / file.name
            if target.exists() and target.read_text().strip() not in ('{}', ''):
                skipped.append(f'state/{file.name}: already present in Mods/CSSX, left in place')
            else:
                moves.append((file, target))   # an empty placeholder written by a fresh start is replaced
    logs = css / 'logs/extensions'
    if logs.is_dir():
        for folder in sorted(p for p in logs.iterdir() if p.is_dir()):
            target = cssx / 'logs' / folder.name
            if target.exists():
                skipped.append(f'logs/{folder.name}: already present, left in place')
            else:
                moves.append((folder, target))
    for file in sorted((css / 'logs').glob('cssx.jsonl*')) if (css / 'logs').is_dir() else []:
        target = cssx / 'logs/legacy' / file.name
        moves.append((file, target))
    output = css / 'output/extensions'
    if output.is_dir():
        for folder in sorted(p for p in output.iterdir() if p.is_dir()):
            target = cssx / 'output' / folder.name
            if target.exists():
                skipped.append(f'output/{folder.name}: already present, left in place')
            else:
                moves.append((folder, target))
    logo = css / 'assets/cssx-logo.png'
    if logo.is_file():
        moves.append((logo, cssx / 'assets/legacy-cssx-logo.png'))
    if (css / 'cssx.json').is_file():
        deletes.append(css / 'cssx.json')
    if (css / 'cores').is_dir():
        deletes.extend(sorted((css / 'cores').glob('cssx_core*.dll')))
    return {'moves': moves, 'deletes': deletes, 'skipped': skipped}


def file_hashes(path: Path) -> dict[str, str]:
    if path.is_file():
        return {'.': sha(path)}   # a moved file may be renamed; only its content matters
    return {str(p.relative_to(path.parent)): sha(p) for p in sorted(path.rglob('*')) if p.is_file()}


def perform(css: Path, cssx: Path, dry_run: bool) -> Path | None:
    if processes():
        raise SystemExit('Close the game first: UE4SS and CSS hold these files open')
    p = plan(css, cssx)
    print(f'{len(p["moves"])} moves, {len(p["deletes"])} deletes, {len(p["skipped"])} skipped')
    for source, target in p['moves']:
        print(f'  move   {source.relative_to(css)}  ->  {target.relative_to(cssx)}')
    for target in p['deletes']:
        print(f'  delete {target.relative_to(css)}')
    for note in p['skipped']:
        print(f'  skip   {note}')
    if dry_run or not (p['moves'] or p['deletes']):
        return None
    stamp = time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())
    backup = cssx / 'backup' / stamp
    backup.mkdir(parents=True)
    manifest = {'stamp': stamp, 'css': str(css), 'cssx': str(cssx), 'entries': []}
    for source, target in p['moves'] + [(d, None) for d in p['deletes']]:
        saved = backup / 'CustomShellSystem' / source.relative_to(css)
        saved.parent.mkdir(parents=True, exist_ok=True)
        if source.is_dir():
            shutil.copytree(source, saved)
        else:
            shutil.copy2(source, saved)
        hashes = file_hashes(source)
        if hashes != file_hashes(saved):
            raise RuntimeError(f'Backup verification failed for {source}')
        manifest['entries'].append({'source': str(source.relative_to(css)), 'target': str(target.relative_to(cssx)) if target else None,
                                    'kind': 'move' if target else 'delete', 'hashes': hashes})
    (backup / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for source, target in p['moves']:
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.is_file():
            target.unlink()   # empty placeholder (see plan)
        shutil.move(str(source), str(target))
        if file_hashes(target) != next(e['hashes'] for e in manifest['entries'] if e['source'] == str(source.relative_to(css))):
            raise RuntimeError(f'Move verification failed for {target}')
    for target in p['deletes']:
        target.unlink()
    for folder in (css / 'extensions', css / 'state/extensions', css / 'logs/extensions', css / 'output/extensions'):
        try:
            if folder.is_dir() and not any(folder.iterdir()):
                folder.rmdir()
        except OSError:
            pass
    (backup / 'restore.json').write_text(json.dumps({'stamp': stamp, 'restorable': True}, indent=2) + '\n')
    print(f'Migration complete. Backup: {backup}')
    return backup


def restore(css: Path, cssx: Path, stamp: str) -> None:
    if processes():
        raise SystemExit('Close the game first')
    backup = cssx / 'backup' / stamp
    manifest = json.loads((backup / 'manifest.json').read_text())
    for entry in manifest['entries']:
        saved = backup / 'CustomShellSystem' / entry['source']
        original = css / entry['source']
        if entry['kind'] == 'move':
            moved = cssx / entry['target']
            if moved.exists():
                if moved.is_dir():
                    shutil.rmtree(moved)
                else:
                    moved.unlink()
        original.parent.mkdir(parents=True, exist_ok=True)
        if original.exists():
            raise RuntimeError(f'Refusing to overwrite existing {original}')
        if saved.is_dir():
            shutil.copytree(saved, original)
        else:
            shutil.copy2(saved, original)
        if file_hashes(original) != entry['hashes']:
            raise RuntimeError(f'Restore verification failed for {original}')
    print(f'Restored {len(manifest["entries"])} entries from {backup}')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--game', type=Path, default=GAME)
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--restore', metavar='STAMP')
    args = parser.parse_args()
    mods = args.game / 'Binaries/Win64/ue4ss/Mods'
    css, cssx = mods / 'CustomShellSystem', mods / 'CSSX'
    if args.restore:
        restore(css, cssx, args.restore)
    else:
        perform(css, cssx, args.dry_run)


if __name__ == '__main__':
    main()
