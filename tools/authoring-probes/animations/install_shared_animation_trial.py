"""Install the combined 63-entry private trial through the existing rollback path."""
import argparse
import json
from pathlib import Path

from update_performance_trial import GAME, NAME, check_hashes, sha, update


def validate_shared_package(pack, old, baseline_path):
    del old
    verification = pack / 'verification.json'
    verified = json.loads(verification.read_text())
    baseline_path = baseline_path.resolve(strict=True)
    baseline = json.loads(baseline_path.read_text())
    if (baseline.get('passed') is not True or baseline.get('entries') != 63
            or baseline.get('exports') != 98 or baseline.get('adapters') != 5
            or baseline.get('eve_performances') != 6 or baseline.get('commander_performances') != 6
            or baseline.get('commander_controls') != 128):
        raise ValueError('Expected the audited combined facial baseline')
    evidence = {str(p): sha(p) for p in (verification, baseline_path)}
    current, seen = verified, {verification.resolve()}
    for _ in range(8):
        parent_path = Path(current.get('base_verification', baseline_path)).resolve(strict=True)
        if parent_path in seen or current.get('protected', {}).get(str(parent_path)) != sha(parent_path):
            raise ValueError('Changed or cyclic animation replacement provenance')
        parent = json.loads(parent_path.read_text())
        count = len(current.get('replacements', {}))
        extra = current.get('added_auxiliary_exports', 1)
        if (current.get('passed') is not True or parent.get('passed') is not True
                or current.get('entries') != 63 or parent.get('entries') != 63
                or not 1 <= count <= 8 or type(extra) is not int or not 0 <= extra <= 8
                or current.get('unchanged_existing_exports') != parent['exports'] - count
                or current.get('exports') != parent['exports'] + extra
                or current.get('dependencies') != baseline.get('dependencies')):
            raise ValueError('Incomplete animation replacement proof')
        evidence[str(parent_path)] = sha(parent_path)
        if parent_path == baseline_path:
            break
        current = parent
        seen.add(parent_path)
    else:
        raise ValueError('Animation replacement provenance exceeds eight revisions')
    files = verified.get('files', {})
    if set(files) != {NAME + suffix for suffix in ('.pak', '.utoc', '.ucas')}:
        raise ValueError('Incomplete shared container set')
    host_path = pack / 'host-validation.json'
    host = json.loads(host_path.read_text())
    if (host.get('passed') is not True or len(host.get('results', [])) != 2
            or any(row.get('exit_code') != 0 for row in host['results'])):
        raise ValueError('Combined and standalone catalog validation must pass')
    motion_path = pack / 'original-clips-full-motion.json'
    motion = json.loads(motion_path.read_text())
    results = motion.get('results', [])
    if (motion.get('passed') is not True or motion.get('clips') != 57 or len(results) != 57
            or len({row['id'] for row in results}) != 57
            or sum(row['frames'] for row in results) != motion.get('frames')
            or sum(row['frames'] * row['tracks'] for row in results) != motion.get('transforms')):
        raise ValueError('Incomplete original-clip comparison')
    for row in results:
        path = Path(row['report']).resolve(strict=True)
        evidence[str(path)] = row['sha256']
        if json.loads(path.read_text()).get('passed') is not True:
            raise ValueError('A full-motion comparison failed: ' + row['id'])
    for path in (host_path, motion_path):
        evidence[str(path)] = sha(path)
    check_hashes(evidence)
    check_hashes({str(pack / NAME / name): digest for name, digest in files.items()})
    check_hashes(verified['dependencies'])
    return verified, evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('previous', 'pack', 'baseline', 'receipt'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--core', type=Path, required=True)
    parser.add_argument('--check-only', action='store_true', help='Validate the candidate without writing game files')
    args = parser.parse_args()
    previous = args.previous.resolve(strict=True)
    pack = args.pack.resolve(strict=True)
    baseline = args.baseline.resolve(strict=True)
    core = args.core.resolve(strict=True)
    if args.check_only:
        validate_shared_package(pack, {}, baseline)
        print(json.dumps(dict(candidate_valid=True, installed=False, core=str(core), core_sha256=sha(core))))
        return
    # CINE already exposes the shared service. Preserve the installed extension,
    # including unrelated scene-control work, while changing CSS and the pack.
    result = update(GAME, previous, pack, args.receipt.resolve(), core,
                    None, None, baseline, keep_cine=True, validator=validate_shared_package)
    print(result)


if __name__ == '__main__':
    main()
