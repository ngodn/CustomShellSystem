"""Check facial-performance aim interruption across the ten release variants."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

from capture_eve_surface_variant import ROOT, GAME, atomic, describe, identity, settled, processes
from check_peony_live_presets import customize, engine, request
from check_outfit_switch_lifecycle import OUTFITS
from check_sidearm_aim_visibility import MOD, geometry_restored
from verify_outfit_aim_interruptions import verify_case


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--resume-from', type=Path,
                        help='Reuse independently verified completed cases from an interrupted run')
    args = parser.parse_args()
    out = args.output.resolve()
    original, pids = describe(), processes()
    player = engine({'op': 'player'})
    if out.exists() or len(pids) != 1 or original['busy'] or original['idle_override']:
        raise RuntimeError('Need one settled player without an idle override and a new output')
    before = json.loads((MOD / 'state/state.json').read_text())
    imported = []
    if args.resume_from:
        prior = args.resume_from.resolve()
        previous = json.loads((prior / 'result.json').read_text())
        if previous['core_sha256'] != args.core_sha256:
            raise RuntimeError('Prior cases used another core')
        if json.loads((prior / 'state-before.json').read_text()) != before:
            raise RuntimeError('Saved settings differ from the prior matrix')
        for row in previous['cases']:
            if not row.get('passed') or row.get('returncode') != 0:
                continue
            folder = Path(row.get('evidence', prior / (row['outfit'] + '-' + row['variant'] + '-' + row['clip'])))
            data = [json.loads((folder / name).read_text()) for name in
                    ('result.json', 'state-before.json', 'state-after.json')]
            verify_case(*data)
            imported.append(dict(row, evidence=str(folder), reused=True))
    completed = {(r['outfit'], r['variant'], r['clip']) for r in imported}
    memories = before['remembered_custom']
    restore_variants = {}
    for outfit, variants in OUTFITS.items():
        if any(outfit + '/' + v not in memories for v in variants):
            raise RuntimeError('Missing original variant memory')
        restore_variants[outfit] = next((v for v in variants if memories[outfit + '/' + v] == memories[outfit]), None)
        if restore_variants[outfit] is None:
            raise RuntimeError('No variant can restore the outfit-level memory')
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    if not cine['ok']:
        raise RuntimeError('Cannot read CINE')
    cine_path = MOD.parent / 'CSSX/state/eins0fx.cine.json'
    cine_saved = json.loads(cine_path.read_text())

    def cine_values(model):
        return {c['id']: (c.get('value'), c.get('enabled'))
                for section in model['result']['sections'] for c in section['controls']}

    def call(target, function):
        return engine({'op': 'call', 'target': target, 'function': function, 'args': {}})['ReturnValue']

    def geometry():
        return dict(position=call(player['pawn'], 'K2_GetActorLocation'),
                    rotation=call(player['pawn'], 'K2_GetActorRotation'),
                    view_target=call(player['controller'], 'GetViewTarget'),
                    control_rotation=call(player['controller'], 'GetControlRotation'))

    initial_geometry = geometry()
    target, expected, idle = identity(original), original['customize'], original['idle_override']

    def guard():
        current = describe()
        if processes() != pids or engine({'op': 'player'}) != player:
            raise RuntimeError('Player or process changed')
        if identity(current) != target or current['customize'] != expected or current['idle_override'] != idle:
            raise RuntimeError('Concurrent appearance change')

    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    shutil.copyfile(ROOT / 'tools/diagnostics/check_outfit_switch_lifecycle.py', out / 'outfit-matrix-source.py')
    atomic(out / 'state-before.json', before)
    atomic(out / 'cine-before.json', cine)
    report = dict(passed=False, original=original, player=player, geometry_before=initial_geometry,
                  core_sha256=args.core_sha256, cases=imported, firing_input=False, movement_input=False)
    try:
        customize({'action': 'idle', 'value': 'eve'})
        idle = 'eve'
        for outfit, variants in OUTFITS.items():
            for variant in variants:
                clips = [c for c in ('irisout', 'comecloser') if (outfit, variant, c) not in completed]
                if not clips:
                    continue
                guard()
                if not geometry_restored(initial_geometry, geometry()):
                    raise RuntimeError('Player or camera moved between cases')
                target = original['shell'], outfit, variant
                expected = memories[outfit + '/' + variant]
                customize({'action': 'wear', 'outfit': outfit, 'variant': variant,
                           'customize': expected, 'persist': False})
                settled(target, custom=expected)
                for clip in clips:
                    guard()
                    case_dir = out / (outfit + '-' + variant + '-' + clip)
                    command = [sys.executable, str(ROOT / 'tools/diagnostics/check_sidearm_aim_visibility.py'),
                               '--output', str(case_dir), '--core-sha256', args.core_sha256,
                               '--interrupt-performance', 'css.eve.dance.' + clip]
                    row = dict(outfit=outfit, variant=variant, clip=clip, command=command)
                    report['cases'].append(row)
                    atomic(out / 'result.json', report)
                    with (out / (case_dir.name + '.log')).open('x') as log:
                        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
                    row['returncode'] = result.returncode
                    if (case_dir / 'result.json').exists():
                        child = json.loads((case_dir / 'result.json').read_text())
                        row['passed'] = child['passed']
                        row['reason'] = child.get('performance_after_aim', {}).get('reason')
                    atomic(out / 'result.json', report)
                    result.check_returncode()
                    if not row.get('passed'):
                        raise RuntimeError('Child did not verify interruption and restoration')
                    print(outfit, variant, clip, 'pass', flush=True)
        report['cases_passed'] = True
    except Exception as error:
        report['error'] = repr(error)
        raise
    finally:
        try:
            guard()
            for outfit, variant in restore_variants.items():
                customize({'action': 'wear', 'outfit': outfit, 'variant': variant,
                           'customize': memories[outfit], 'persist': False})
                settled((original['shell'], outfit, variant), custom=memories[outfit])
            customize({'action': 'idle', 'value': original['idle_override']})
            customize({'action': 'wear', 'outfit': original['outfit'], 'variant': original['variant'],
                       'customize': original['customize'], 'persist': True})
            settled(identity(original), custom=original['customize'])
            time.sleep(1)
            after = json.loads((MOD / 'state/state.json').read_text())
            atomic(out / 'state-after.json', after)
            report['state_restored'] = after == before
            report['geometry_after'] = geometry()
            report['geometry_restored'] = geometry_restored(initial_geometry, report['geometry_after'])
            cine_after = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
            atomic(out / 'cine-after.json', cine_after)
            report['cine_model_equal'] = cine_after['result'] == cine['result']
            report['cine_restored'] = (json.loads(cine_path.read_text()) == cine_saved and
                                       cine_values(cine_after) == cine_values(cine))
            report['process_preserved'] = processes() == pids
            report['passed'] = bool(report.get('cases_passed') and all(report[k] for k in
                ('state_restored', 'geometry_restored', 'cine_restored', 'process_preserved')))
        except Exception as error:
            report['restore_error'] = repr(error)
            raise
        finally:
            atomic(out / 'result.json', report)
    if not report['passed']:
        raise RuntimeError('Final restoration did not pass')


if __name__ == '__main__':
    main()
