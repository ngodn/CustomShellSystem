"""Verify recorded attack interruptions and restoration without contacting the game."""
import argparse
import copy
import hashlib
import json
from pathlib import Path

from check_outfit_switch_lifecycle import OUTFITS
from check_sidearm_aim_visibility import geometry_restored


def verify_case(row, before, after, active_attacks):
    playing, stopped = row['playing'], row['stopped']
    assert row['started']['token'] == playing['token'] == stopped['token']
    assert row['started']['id'] == playing['id'] == stopped['id'] == row['clip']
    assert playing['active'] and playing['weight'] == 1
    assert not stopped['active'] and stopped['weight'] == 0 and stopped['phase'] == 'stopped'
    assert .5 <= playing['seconds'] <= stopped['seconds'] < stopped['duration']
    assert stopped['reason'] in ('Player is moving', 'A gameplay action is blocking input',
                                 'Gameplay montage is active', 'Wait for the gameplay montage to finish')
    assert active_attacks, 'No actual native attack was recorded'
    assert row['attack_input'] and row['attack_key_observed']
    assert not row['movement_input'] and not row['camera_input']
    assert not row.get('forced_stop') and not row.get('error') and not row.get('restore_error')
    assert row['samples'] and not row['samples'][-1]['expression']['performance']
    assert before == after and geometry_restored(row['geometry_before'], row['geometry_after'])
    assert row['passed'] and all(row[k] for k in ('action_passed', 'geometry_restored',
        'state_restored', 'cine_restored', 'health_unchanged', 'keys_released', 'focus_restored'))


def recorded_attacks(path):
    lines = [json.loads(line) for line in path.read_text().splitlines()]
    attacks = []
    for request, reply in zip(lines, lines[1:]):
        q = request.get('request', {})
        if q.get('op') != 'get' or q.get('property') != 'ActivatableAbilities' or 'result' not in reply:
            continue
        for spec in reply['result']['Items']:
            name = spec['Ability']['class']
            if spec['ActiveCount'] > 0 and '/Axatana/' in name and '/GA_Player_Attack_' in name:
                attacks.append(dict(ability=spec['Ability'], count=spec['ActiveCount']))
    return attacks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    root = args.directory.resolve()
    read = lambda p: json.loads(p.read_text())
    matrix = read(root / 'result.json')
    expected = {(o, v, c) for o, variants in OUTFITS.items() for v in variants for c in ('irisout', 'comecloser')}
    actual = [(r['outfit'], r['variant'], r['clip']) for r in matrix['cases']]
    assert len(actual) == len(expected) and set(actual) == expected
    results, first = [], None
    for case in matrix['cases']:
        folder = Path(case['evidence'])
        row, before, after = (read(folder / n) for n in ('result.json', 'state-before.json', 'state-after.json'))
        attacks = recorded_attacks(folder / 'requests.jsonl')
        assert case['passed'] and case['returncode'] == 0
        assert row['core_sha256'] == matrix['core_sha256']
        assert row['original']['outfit'] == case['outfit'] and row['original']['variant'] == case['variant']
        assert row['clip'] == 'css.eve.dance.' + case['clip']
        assert row['original']['shell'] == matrix['original']['shell']
        verify_case(row, before, after, attacks)
        first = first or (row, before, after, attacks)
        results.append(dict(outfit=case['outfit'], variant=case['variant'], clip=case['clip'],
                            stopped_seconds=row['stopped']['seconds'], reason=row['stopped']['reason'],
                            active_attack_observations=len(attacks), evidence=str(folder),
                            sha256={name: hashlib.sha256((folder / name).read_bytes()).hexdigest()
                                    for name in ('result.json', 'requests.jsonl', 'recipe.py')}))
    assert read(root / 'state-before.json') == read(root / 'state-after.json')
    assert read(root / 'cine-before.json') == read(root / 'cine-after.json')
    assert geometry_restored(matrix['geometry_before'], matrix['geometry_after'])
    assert matrix['passed'] and all(matrix[k] for k in
        ('state_restored', 'geometry_restored', 'cine_restored', 'process_preserved'))
    rejected = []
    for label in ('still active', 'natural end', 'no native attack'):
        row, before, after, attacks = copy.deepcopy(first)
        if label == 'still active':
            row['stopped']['active'] = True
        elif label == 'natural end':
            row['stopped']['seconds'] = row['stopped']['duration']
        else:
            attacks = []
        try:
            verify_case(row, before, after, attacks)
        except AssertionError:
            rejected.append(label)
    assert len(rejected) == 3
    verified = dict(passed=True, cases=len(results), shell=matrix['original']['shell'],
                    results=results, rejected_controls=rejected,
                    scope='Secondary Axatana attacks interrupt Iris Out and Come Closer on ten outfits; no combo, dodge, damage or visual-contact acceptance.')
    (root / 'verified.json').write_text(json.dumps(verified, indent=2) + '\n')
    print(json.dumps({k: v for k, v in verified.items() if k != 'results'}))


if __name__ == '__main__':
    main()
