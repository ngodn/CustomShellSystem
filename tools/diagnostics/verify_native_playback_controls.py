"""Check native playback receipts against the recorded game responses."""
import argparse
import json
from pathlib import Path

from capture_eve_surface_variant import atomic, sha
from check_native_idle_library import MANIFEST, SKELETON
from verify_outfit_attack_interruptions import recorded_attacks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('loop_run', type=Path)
    parser.add_argument('pose_run', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    read = lambda path: json.loads(path.read_text())
    reports = [read(root / 'result.json') for root in (args.loop_run, args.pose_run)]
    library = read(MANIFEST)['catalog']['animation_library']['entries']
    expected = {entry['id']: asset['asset'] for entry in library for asset in entry['assets']
                if asset['skeleton'] == SKELETON}
    inputs, verified = {}, []

    def pairs(root):
        records = [json.loads(line) for line in (root / 'requests.jsonl').read_text().splitlines()]
        return [(q['request'], r['result']) for q, r in zip(records, records[1:])
                if 'request' in q and 'result' in r]

    def tracks(value, clip):
        assert len(value) == 1 and value[0]['SlotName'] == 'DefaultSlot'
        segments = value[0]['AnimTrack']['AnimSegments']
        assert len(segments) == 1
        segment = segments[0]
        assert segment['AnimReference']['name'].split(' ', 1)[1] == expected[clip]
        assert segment['AnimPlayRate'] == 1 and segment['LoopingCount'] == 1

    for root, report, mode in zip((args.loop_run, args.pose_run), reports, ('loop', 'pose')):
        assert all(report['restoration'].values()) and not report.get('restore_error')
        assert read(root / 'state-before.json') == read(root / 'state-after.json')
        log = pairs(root)
        row = next(r for r in report['cases'] if r['mode'] == mode)
        assert row['passed']
        tracks(row['tracks'], row['clip'])
        assert any(q.get('property') == 'SlotAnimTracks' and v == row['tracks'] for q, v in log)
        for key in ('initial', 'boundary_a', 'boundary_b', 'paused_a', 'paused_b', 'resumed'):
            sample = row[key]
            assert any(q.get('action') == 'performance' and q['op'] == 'status' and v == sample['performance'] for q, v in log)
            assert any(q.get('function') == 'Montage_GetPosition' and q['op'] == 'call'
                       and q['args']['Montage'] == sample['montage']
                       and v['ReturnValue'] == sample['montage_seconds'] for q, v in log)
            assert sample['performance']['active'] and sample['performance']['token'] == row['started']['token']
        a, b = row['boundary_a']['performance'], row['boundary_b']['performance']
        if mode == 'loop':
            assert a['phase'] == b['phase'] == 'playing' and 0 <= a['seconds'] < b['seconds'] < a['duration']
            assert any(q.get('op') == 'seek' and q['seconds'] == a['duration'] - .3 for q, v in log)
        else:
            assert a['phase'] == b['phase'] == 'holding' and a['seconds'] == b['seconds'] == a['duration']
        a, b = row['paused_a'], row['paused_b']
        assert a['performance']['paused'] and b['performance']['paused']
        assert a['performance']['seconds'] == b['performance']['seconds']
        assert a['montage_seconds'] == b['montage_seconds']
        assert not row['resumed']['performance']['paused'] and row['resumed']['performance']['seconds'] > .1
        verified.append(dict(mode=mode, clip=row['clip'], raw_pairs=len(log)))
        for path in root.rglob('*'):
            if path.is_file() and path.resolve() != args.output.resolve() and path.name != 'SHA256SUMS':
                inputs[str(path.resolve())] = sha(path)
    # The first run stopped on the fixture's minimum-duration assumption after
    # its loop passed. Its pose read shows the valid short clip already holding.
    assert not reports[0]['passed'] and reports[0]['error'] == 'AssertionError()'
    short = reports[0]['cases'][1]
    assert short['mode'] == 'pose' and short['started']['duration'] < .5
    assert short['initial']['performance']['phase'] == 'holding'
    assert reports[1]['passed']
    root = args.pose_run / 'parry'
    p = read(root / 'result.json')
    assert p == reports[1]['parry'] and p['passed']
    log = pairs(root)
    attacks = recorded_attacks(root / 'requests.jsonl', 'parry')
    assert attacks and p['action_key_observed'] and p['action_input']
    assert p['started']['token'] == p['playing']['token'] == p['stopped']['token']
    assert p['playing']['active'] and not p['stopped']['active']
    assert p['playing']['seconds'] <= p['stopped']['seconds'] < p['stopped']['duration']
    assert p['stopped']['reason'] == 'Player or animation binding changed'
    tracks(p['native_tracks'], p['clip'])
    assert any(q.get('property') == 'SlotAnimTracks' and v == p['native_tracks'] for q, v in log)
    assert p['native_montage_after'] is None
    assert not p.get('forced_stop') and not p.get('restore_error') and not p.get('error')
    assert all(p[k] for k in ('state_restored', 'cine_restored', 'geometry_restored', 'health_unchanged',
                             'resolve_restored', 'keys_released', 'focus_restored'))
    assert not p['actions_after_cleanup'] and not p['input_blocks_after_cleanup']
    atomic(args.output, dict(passed=True, controls=verified, parry_native_observations=len(attacks),
                            manifest_sha256=sha(MANIFEST), inputs=inputs))
    print(json.dumps(dict(passed=True, controls=verified, parry_native_observations=len(attacks))))


if __name__ == '__main__':
    main()
