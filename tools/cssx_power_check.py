#!/usr/bin/env python3
"""Reversible live clone check. Requires Genessa, no active cheats, and a safe area."""
import json
import time
from pathlib import Path
from cssx_cheat_check import request, event, model


def get(target, field):
    return request({'op': 'get', 'target': target, 'property': field}, True)


def ability(name):
    player = request({'op': 'player'}, True)
    asc = get(player['pawn'], 'AbilitySystemComponent')
    matches = {}
    for item in get(asc, 'ActivatableAbilities')['Items']:
        for field in ('NonReplicatedInstances', 'ReplicatedInstances'):
            for value in item.get(field, []):
                if value and value['class'].endswith('.' + name):
                    matches[value['$object']] = value
    assert len(matches) == 1, 'Expected one player-owned ability'
    return next(iter(matches.values()))


def clones():
    target = ability('GA_AstralClones_Action_C')
    def sample():
        return {field: get(target, field) for field in
                ('SpawnCount', 'CurrentPrimaryClone', 'CurrentSecondaryClone')}
    evidence = {'before': sample(), 'samples': []}
    controls = model()
    assert not controls['apply_settings']['enabled'], 'Existing pending settings'
    assert not controls['disable_all']['enabled'], 'Existing cheats must be off'
    assert evidence['before']['CurrentPrimaryClone'] is None
    assert evidence['before']['CurrentSecondaryClone'] is None
    try:
        event('genessa_clones', value=True)
        evidence['draft'] = sample()
        assert evidence['draft'] == evidence['before'], 'Draft changed gameplay'
        event('apply_settings')
        for _ in range(10):
            time.sleep(.4)
            current = sample()
            evidence['samples'].append(current)
            if current['CurrentPrimaryClone'] and current['CurrentSecondaryClone']:
                break
        assert current['CurrentPrimaryClone'] and current['CurrentSecondaryClone'], 'Pair did not appear'
        assert model()['genessa_clones']['value'] is True
    finally:
        try:
            if model()['disable_all'].get('enabled', False):
                event('disable_all')
        except Exception as error:
            evidence['cleanup_error'] = str(error)
            raise
        finally:
            evidence['after'] = sample()
            output = Path(__file__).resolve().parents[1] / 'work/cssx-native/clones-pair-live-check.json'
            output.write_text(json.dumps(evidence, indent=2) + '\n')
    assert evidence['after'] == evidence['before'], 'Cleanup differs from baseline'
    print('Clone drafts, paired actors and exact cleanup passed')


if __name__ == '__main__':
    clones()
