"""Verify recorded native action interruptions and restoration without contacting the game."""
import argparse
import copy
import hashlib
import json
from pathlib import Path

from check_outfit_switch_lifecycle import OUTFITS
from check_sidearm_aim_visibility import geometry_restored


def action_matches(name, action):
    if action == 'shell_ability':
        return '/CorruptedGenessa/Abilities/GA_AstralClones_Action.' in name
    return ('/Dash/GA_Dash_CorruptedGenessa.' in name if action == 'dodge' else
            '/Axatana/' in name and '/GA_Player_Attack_' in name)


def verify_case(row, before, after, active_attacks):
    playing, stopped = row['playing'], row['stopped']
    assert row['started']['token'] == playing['token'] == stopped['token']
    assert row['started']['id'] == playing['id'] == stopped['id'] == row['clip']
    assert playing['active'] and playing['weight'] == 1
    assert not stopped['active'] and stopped['weight'] == 0 and stopped['phase'] == 'stopped'
    assert .5 <= playing['seconds'] <= stopped['seconds'] < stopped['duration']
    assert stopped['reason'] in ('Player is moving', 'A gameplay action is blocking input',
                                 'Gameplay montage is active', 'Wait for the gameplay montage to finish')
    assert active_attacks, 'No actual native action was recorded'
    for observed in active_attacks:
        name = observed['ability']['class']
        assert observed['count'] > 0
        assert action_matches(name, row.get('action', 'secondary_attack'))
    assert row.get('action_input', row.get('attack_input'))
    assert row.get('action_key_observed', row.get('attack_key_observed'))
    assert not row['movement_input'] and not row['camera_input']
    assert not row.get('forced_stop') and not row.get('error') and not row.get('restore_error')
    assert row['samples'] and not row['samples'][-1]['expression']['performance']
    assert before == after and geometry_restored(row['geometry_before'], row['geometry_after'])
    assert row['passed'] and all(row[k] for k in ('action_passed', 'geometry_restored',
        'state_restored', 'cine_restored', 'health_unchanged', 'keys_released', 'focus_restored'))
    if row.get('action') == 'shell_ability':
        assert row['secondary_eligible'] and row['resolve_restored']
        assert not row['actions_after_cleanup'] and not row['input_blocks_after_cleanup']
        assert row['resolve_before'] == row['resolve_after']
        initial, final = row['summon_before'], row['summon_after']
        assert not any(a['active'] for a in initial['observed']['actors'])
        assert not any(a['active'] for a in final['observed']['actors'])
        assert final == row['summon_samples'][-1]
        actors = {a['id'] for sample in row['summon_samples'] for a in sample['observed']['actors'] if a['active']}
        assert actors == {'AstralClonePrimary', 'AstralCloneSecondary'}
        start, end = initial['status'], final['status']
        assert start['active'] == start['cached'] == end['active'] == end['cached'] == 0
        assert not end['error'] and end['fallbacks'] == start['fallbacks']
        groups = 2 if before['doubles']['stray'] == 'css' else 0
        assert end['prepared'] - start['prepared'] == end['removed'] - start['removed'] == groups
    if row.get('action') == 'secondary_combo':
        assert not row['timeline_only'] and row['second_input']['key_observed']
        assert row['combo_count_after'] == 0
        assert row['combo_history_after'] == row['combo_history_before'] + row['combo_events']
        for index in (1, 2):
            assert any(f'GA_Player_Attack_Axatana_Axe_B{index}_' in event for event in row['combo_events'])
            assert any(f'/GA_Player_Attack_Axatana_Axe_B{index}.' in a['ability']['class'] or
                       f'/GA_Player_Attack_Axatana_Axe_B{index}_Hold.' in a['ability']['class'] for a in active_attacks)
        assert any(r['blocks'] for r in row['timeline'] if r['end'] < row['second_input']['pressed_at'])
        assert row['second_input']['before']['blocks'] == [] and row['second_input']['before']['count'] == 1


def recorded_attacks(path, action='secondary_attack'):
    lines = [json.loads(line) for line in path.read_text().splitlines()]
    attacks = []
    for request, reply in zip(lines, lines[1:]):
        q = request.get('request', {})
        if q.get('op') != 'get' or q.get('property') != 'ActivatableAbilities' or 'result' not in reply:
            continue
        for spec in reply['result']['Items']:
            name = spec['Ability']['class']
            matched = action_matches(name, action)
            if spec['ActiveCount'] > 0 and matched:
                attacks.append(dict(ability=spec['Ability'], count=spec['ActiveCount']))
    return attacks


def verify_summon_log(path, row):
    records = [json.loads(line) for line in path.read_text().splitlines()]
    observations = [reply['result'] for request, reply in zip(records, records[1:])
                    if request.get('request', {}).get('op') == 'astral.observe' and 'result' in reply]
    assert observations == [row['summon_before']['observed'],
                            *[sample['observed'] for sample in row['summon_samples']]]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--check-only', action='store_true', help='Verify without replacing the archived verification receipt')
    args = parser.parse_args()
    root = args.directory.resolve()
    read = lambda p: json.loads(p.read_text())
    matrix = read(root / 'result.json')
    action = matrix.get('action', 'secondary_attack')
    assert action in ('secondary_attack', 'secondary_combo', 'dodge', 'shell_ability')
    expected = {(o, v, c) for o, variants in OUTFITS.items() for v in variants for c in ('irisout', 'comecloser')}
    actual = [(r['outfit'], r['variant'], r['clip']) for r in matrix['cases']]
    assert len(actual) == len(expected) and set(actual) == expected
    results, first = [], None
    for case in matrix['cases']:
        folder = Path(case['evidence'])
        row, before, after = (read(folder / n) for n in ('result.json', 'state-before.json', 'state-after.json'))
        attacks = recorded_attacks(folder / 'requests.jsonl', action)
        if action == 'shell_ability':
            verify_summon_log(folder / 'requests.jsonl', row)
        if action == 'secondary_combo':
            records = [json.loads(line) for line in (folder / 'requests.jsonl').read_text().splitlines()]
            histories = [reply['result'] for request, reply in zip(records, records[1:])
                         if request.get('request', {}).get('property') == 'DebugComboHistory' and 'result' in reply]
            assert histories == [row['combo_history_before'], row['combo_history_after']]
        assert case['passed'] and case['returncode'] == 0
        assert row['core_sha256'] == matrix['core_sha256']
        assert row.get('action', 'secondary_attack') == action
        assert row['original']['outfit'] == case['outfit'] and row['original']['variant'] == case['variant']
        assert row['clip'] == 'css.eve.dance.' + case['clip']
        assert row['original']['shell'] == matrix['original']['shell']
        verify_case(row, before, after, attacks)
        first = first or (row, before, after, attacks)
        results.append(dict(outfit=case['outfit'], variant=case['variant'], clip=case['clip'],
                            stopped_seconds=row['stopped']['seconds'], reason=row['stopped']['reason'],
                            active_action_observations=len(attacks), evidence=str(folder),
                            sha256={name: hashlib.sha256((folder / name).read_bytes()).hexdigest()
                                    for name in ('result.json', 'requests.jsonl', 'recipe.py')}))
    assert read(root / 'state-before.json') == read(root / 'state-after.json')
    assert read(root / 'cine-before.json') == read(root / 'cine-after.json')
    assert geometry_restored(matrix['geometry_before'], matrix['geometry_after'])
    assert matrix['passed'] and all(matrix[k] for k in
        ('state_restored', 'geometry_restored', 'cine_restored', 'process_preserved'))
    rejected = []
    controls = ['still active', 'natural end', 'no native attack']
    if action == 'secondary_combo':
        controls += ['no second stage', 'unobserved second input']
    if action == 'shell_ability':
        controls += ['clone still active', 'missing secondary clone', 'input still blocked', 'resources changed']
    for label in controls:
        row, before, after, attacks = copy.deepcopy(first)
        if label == 'still active':
            row['stopped']['active'] = True
        elif label == 'natural end':
            row['stopped']['seconds'] = row['stopped']['duration']
        elif label == 'no native attack':
            attacks = []
        elif label == 'no second stage':
            attacks = [a for a in attacks if 'Axe_B2' not in a['ability']['class']]
        elif label == 'unobserved second input':
            row['second_input']['key_observed'] = False
        elif label == 'clone still active':
            row['summon_after']['status']['active'] = 1
            row['summon_samples'][-1]['status']['active'] = 1
        elif label == 'missing secondary clone':
            for sample in row['summon_samples']:
                sample['observed']['actors'] = [a for a in sample['observed']['actors'] if a['id'] != 'AstralCloneSecondary']
            row['summon_after'] = copy.deepcopy(row['summon_samples'][-1])
        elif label == 'input still blocked':
            row['input_blocks_after_cleanup'] = [1]
        else:
            row['resolve_after']['CurrentValue'] -= 1
        try:
            verify_case(row, before, after, attacks)
        except AssertionError:
            rejected.append(label)
    assert len(rejected) == len(controls)
    verified = dict(passed=True, action=action, cases=len(results), shell=matrix['original']['shell'],
                    results=results, rejected_controls=rejected,
                    scope=f'{action} interrupts Iris Out and Come Closer on ten outfits; '
                          'no damage, visual-contact or other-action acceptance.')
    if not args.check_only:
        (root / 'verified.json').write_text(json.dumps(verified, indent=2) + '\n')
    print(json.dumps({k: v for k, v in verified.items() if k != 'results'}))


if __name__ == '__main__':
    main()
