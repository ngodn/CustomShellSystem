"""Verify archived aim-interruption evidence without contacting the game."""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path


def verify_case(row, before, after):
    started = row['performance_started']
    playing = row['performance_before_aim']
    stopped = row['performance_after_aim']
    assert started['token'] == playing['token'] == stopped['token']
    assert started['id'] == playing['id'] == stopped['id'] == row['interrupt_performance']
    assert playing['active'] and playing['weight'] == 1 and row['expression_before_aim']['performance']
    assert not stopped['active'] and stopped['weight'] == 0 and stopped['phase'] == 'stopped'
    assert not row['expression_after_aim']['performance']
    assert .5 <= playing['seconds'] <= stopped['seconds'] < stopped['duration']
    assert stopped['reason'] in ('A gameplay action is blocking input', 'Gameplay montage is active',
                                 'Wait for the gameplay montage to finish', 'Sidearm is in use')
    assert row['aim_key_observed'] and not row['firing_input'] and not row['movement_input']
    drawn, stowed = row['aimed'], row['stowed']
    assert drawn['socket'] == 'Socket_Prop_R' and drawn['equip_state'] == 1
    assert not drawn['actor_hidden'] and not drawn['mesh_hidden'] and drawn['effect_rendered']
    assert stowed['socket'] == 'Socket_NailShotgun_Stowed' and stowed['equip_state'] == 0
    assert stowed['actor_hidden'] and stowed['mesh_hidden'] and not stowed['effect_rendered']
    assert row['resources_before'] == row['resources_after']
    assert before == after and row['process_before'] == row['process_after']
    assert all(row[k] for k in ('state_restored', 'infusion_preserved', 'cine_restored', 'keys_released'))
    a, b = row['geometry_before'], row['geometry_after']
    assert all(a[k] == b[k] for k in ('position', 'control_rotation', 'view_target'))
    for axis in ('Pitch', 'Yaw', 'Roll'):
        delta = (b['rotation'][axis] - a['rotation'][axis] + 180) % 360 - 180
        assert math.isfinite(delta) and abs(delta) <= 1e-8
    assert row['passed'] and not row.get('cleanup_stop'), 'Normal interruption needed forced cleanup'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    root = args.directory.resolve()
    read = lambda p: json.loads(p.read_text())
    report = read(root / 'result.json')
    expected = {('eins0fx.seduxtress', v, c)
                for v in ('black_pearl', 'prototype', 'skin_suit', 'bikini', 'casual_sweater', 'midsummer_alice', 'peony')
                for c in ('irisout', 'comecloser')}
    expected |= {('eins0fx.unholygenessa', v, c) for v in ('eve', 'genessa') for c in ('irisout', 'comecloser')}
    expected |= {('eins0fx.commanderwhite', 'original', c) for c in ('irisout', 'comecloser')}
    counts = Counter((r['outfit'], r['variant'], r['clip']) for r in report['cases'])
    assert set(counts) == expected and set(counts.values()) == {1}
    results = []
    first = None
    for case in report['cases']:
        folder = Path(case.get('evidence', root / (case['outfit'] + '-' + case['variant'] + '-' + case['clip'])))
        row, before, after = (read(folder / name) for name in ('result.json', 'state-before.json', 'state-after.json'))
        assert case['returncode'] == 0 and case['passed']
        assert row['core_sha256'] == report['core_sha256']
        verify_case(row, before, after)
        first = first or (row, before, after)
        results.append(dict(outfit=case['outfit'], variant=case['variant'], clip=case['clip'],
                            evidence=str(folder), reused=case.get('reused', False),
                            evidence_sha256={name: hashlib.sha256((folder / name).read_bytes()).hexdigest()
                                             for name in ('result.json', 'state-before.json', 'state-after.json', 'recipe.py')},
                            stopped_seconds=row['performance_after_aim']['seconds'],
                            reason=row['performance_after_aim']['reason']))
    assert read(root / 'state-before.json') == read(root / 'state-after.json')
    cine_equal = read(root / 'cine-before.json')['result'] == read(root / 'cine-after.json')['result']
    assert cine_equal, 'CINE model changed beyond the RPC request identifier'
    assert report['passed'] and all(report[k] for k in
        ('state_restored', 'geometry_restored', 'cine_restored', 'process_preserved'))
    failures = []
    for label, field, value in (('still active', 'active', True), ('natural end', 'seconds', first[0]['performance_after_aim']['duration'])):
        bad = json.loads(json.dumps(first[0]))
        bad['performance_after_aim'][field] = value
        try:
            verify_case(bad, first[1], first[2])
        except AssertionError:
            failures.append(label)
    assert len(failures) == 2, 'Verifier accepted a non-interruption control'
    output = dict(passed=True, cases=len(results), results=results, rejected_controls=failures,
                  cine_model_equal=cine_equal,
                  scope='Actual aim interrupts two facial clips on ten variants in Faithful form; no firing, damage, movement or visual-contact acceptance.')
    (root / 'verified.json').write_text(json.dumps(output, indent=2) + '\n')
    files = sorted(p for p in root.rglob('*') if p.is_file() and p.name != 'SHA256SUMS')
    (root / 'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + str(p.relative_to(root)) + '\n' for p in files))
    print(json.dumps(dict(passed=True, cases=len(results), rejected_controls=failures)))


if __name__ == '__main__':
    main()
