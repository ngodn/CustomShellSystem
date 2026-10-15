"""Check mapped action interruption across all ten release variants."""
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--action', choices=('secondary_attack', 'secondary_combo', 'dodge', 'shell_ability'), default='secondary_attack')
    parser.add_argument('--ground-proof', type=Path)
    parser.add_argument('--start-evidence', type=Path)
    args = parser.parse_args()
    out = args.output.resolve()
    original, pids = describe(), processes()
    if out.exists() or len(pids) != 1 or original['busy'] or original['idle_override']:
        raise RuntimeError('Need a settled player without an idle override and a new output')
    before = json.loads((MOD / 'state/state.json').read_text())
    memories = before['remembered_custom']
    restore_variants = {}
    for outfit, variants in OUTFITS.items():
        if any(outfit + '/' + v not in memories for v in variants):
            raise RuntimeError('Missing original variant memory')
        restore_variants[outfit] = next((v for v in variants if memories[outfit + '/' + v] == memories[outfit]), None)
        if restore_variants[outfit] is None:
            raise RuntimeError('Cannot restore the original outfit-level memory')
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result']
    player = engine({'op': 'player'})

    def call(target, function):
        engine({'op': 'describe', 'target': target, 'function': function})
        return engine({'op': 'call', 'target': target, 'function': function, 'args': {}})['ReturnValue']

    def geometry():
        return {key: call(player[target], function) for key, target, function in (
            ('position', 'pawn', 'K2_GetActorLocation'), ('rotation', 'pawn', 'K2_GetActorRotation'),
            ('view_target', 'controller', 'GetViewTarget'), ('control_rotation', 'controller', 'GetControlRotation'))}

    baseline = geometry()
    target, expected, idle = identity(original), original['customize'], original['idle_override']

    def guard():
        current = describe()
        if processes() != pids or engine({'op': 'player'}) != player:
            raise RuntimeError('Player or process changed')
        if identity(current) != target or current['customize'] != expected or current['idle_override'] != idle:
            raise RuntimeError('Concurrent appearance change')
        if not geometry_restored(baseline, geometry()):
            raise RuntimeError('Player or camera moved between cases')

    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', before)
    atomic(out / 'cine-before.json', cine)
    report = dict(passed=False, original=original, player=player, geometry_before=baseline,
                  core_sha256=args.core_sha256, cases=[], action=args.action)
    atomic(out / 'result.json', report)
    try:
        customize({'action': 'idle', 'value': 'eve'})
        idle = 'eve'
        for outfit, variants in OUTFITS.items():
            for variant in variants:
                guard()
                target = original['shell'], outfit, variant
                expected = memories[outfit + '/' + variant]
                customize({'action': 'wear', 'outfit': outfit, 'variant': variant,
                           'customize': expected, 'persist': False})
                settled(target, custom=expected)
                for clip in ('irisout', 'comecloser'):
                    guard()
                    folder = out / (outfit + '-' + variant + '-' + clip)
                    command = [sys.executable, str(ROOT / 'tools/diagnostics/check_attack_interruption.py'),
                               '--output', str(folder), '--core-sha256', args.core_sha256,
                               '--clip', 'css.eve.dance.' + clip, '--action', args.action]
                    if args.ground_proof:
                        command += ['--ground-proof', str(args.ground_proof.resolve())]
                    if args.start_evidence:
                        command += ['--start-evidence', str(args.start_evidence.resolve())]
                    row = dict(outfit=outfit, variant=variant, clip=clip, evidence=str(folder), command=command)
                    report['cases'].append(row); atomic(out / 'result.json', report)
                    with (out / (folder.name + '.log')).open('x') as log:
                        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
                    row['returncode'] = result.returncode
                    if (folder / 'result.json').exists():
                        child = json.loads((folder / 'result.json').read_text())
                        row['passed'] = child['passed']
                        row['reason'] = child.get('stopped', {}).get('reason')
                    atomic(out / 'result.json', report)
                    result.check_returncode()
                    if not row.get('passed'):
                        raise RuntimeError('Child did not verify action and restoration')
                    print(outfit, variant, clip, 'pass', flush=True)
        report['cases_passed'] = True
    except Exception as error:
        report['error'] = repr(error)
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
            report['geometry_restored'] = geometry_restored(baseline, report['geometry_after'])
            cine_after = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result']
            atomic(out / 'cine-after.json', cine_after)
            report['cine_restored'] = cine_after == cine
            report['process_preserved'] = processes() == pids
            report['passed'] = bool(report.get('cases_passed') and all(report.get(k) for k in
                ('state_restored', 'geometry_restored', 'cine_restored', 'process_preserved')))
        except Exception as error:
            report['restore_error'] = repr(error)
        atomic(out / 'result.json', report)
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
