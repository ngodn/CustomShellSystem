#!/usr/bin/env python3
"""Snapshot incoming mod sources before conversion. Python 3.14."""
from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

ARCHIVES = {'.zip', '.rar', '.7z'}
PACKS = {'.pak', '.utoc', '.ucas'}
PARTIAL = {'.part', '.partial', '.crdownload', '.download', '.tmp'}


def snapshot(directory: Path) -> dict:
    """Include directories and companion files, so additions invalidate the snapshot."""
    result = {}
    for path in [directory, *sorted(directory.rglob('*'))]:
        if path.is_symlink():
            raise ValueError(f'Incoming sources must not contain symlinks: {path}')
        stat = path.stat()
        if not path.is_file() and not path.is_dir():
            raise ValueError(f'Unsupported source entry: {path}')
        result[path.relative_to(directory).as_posix()] = {
            'directory': path.is_dir(), 'bytes': stat.st_size,
            'mtime_ns': stat.st_mtime_ns, 'ctime_ns': stat.st_ctime_ns,
            'inode': stat.st_ino, 'device': stat.st_dev,
        }
    return result


def ready(before: dict, after: dict, now_ns: int, minimum_age: float = 60) -> tuple[bool, str]:
    if minimum_age < 60:
        raise ValueError('Incoming sources require at least 60 seconds of settling time')
    if before != after:
        return False, 'Source entries changed between observations'
    if any(now_ns - max(v['mtime_ns'], v['ctime_ns']) < minimum_age * 1e9 for v in after.values()):
        return False, 'A file or directory is less than 60 seconds old'
    files = [Path(k) for k, v in after.items() if not v['directory']]
    if any(p.suffix.lower() in PARTIAL for p in files):
        return False, 'A partial download or copy is present'
    if not any(p.suffix.lower() in ARCHIVES | PACKS for p in files):
        return False, 'No archives or Unreal containers found'
    stems = {str(p.with_suffix('')) for p in files if p.suffix.lower() == '.utoc'}
    payloads = {str(p.with_suffix('')) for p in files if p.suffix.lower() == '.ucas'}
    if stems != payloads:
        return False, 'IoStore companion files are incomplete'
    return True, 'Stable and at least 60 seconds old'


def unchanged(directory: Path, expected: dict) -> None:
    if snapshot(directory) != expected:
        raise ValueError(f'Source changed during processing; discard the staged conversion: {directory}')


def scan(root: Path, interval: float = 2) -> dict:
    if not 1 <= interval <= 60:
        raise ValueError('Observation interval must be between 1 and 60 seconds')
    # Only watch originals. Generated extraction, work and output folders cannot
    # become new source mods or reset the original download age.
    directories = sorted(p for p in root.glob('*/original') if p.is_dir())
    first = {p: snapshot(p) for p in directories}
    time.sleep(interval)
    result = {'schema': 1, 'observed_ns': time.time_ns(), 'sources': []}
    for path in directories:
        try:
            second = snapshot(path)
            stable, reason = ready(first[path], second, result['observed_ns'])
        except (OSError, ValueError) as error:
            second, stable, reason = {}, False, str(error)
        result['sources'].append({'directory': str(path.resolve()), 'ready': stable,
                                  'reason': reason, 'snapshot': second})
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = scan(args.root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    for source in report['sources']:
        print(('READY' if source['ready'] else 'DEFER'), source['directory'], source['reason'])
