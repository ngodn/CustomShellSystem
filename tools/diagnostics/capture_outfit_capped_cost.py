"""Capture the ten release variants at native caps and restore all outfit memories."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

from capture_eve_surface_variant import ROOT, GAME, atomic, describe, identity, settled, processes, sha
from check_peony_live_presets import customize, engine, request
from check_outfit_switch_lifecycle import OUTFITS
from check_sidearm_aim_visibility import MOD, geometry_restored


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    selection = parser.add_mutually_exclusive_group()
    choices = [o + '/' + v for o, variants in OUTFITS.items() for v in variants]
    selection.add_argument('--only', choices=choices)
    selection.add_argument('--start-at', choices=choices, help='Start at a variant after separately verified earlier cases')
    parser.add_argument('--caps', type=float, nargs='+', choices=(30., 60., 999.), default=[30., 60., 999.])
    parser.add_argument('--record-resets', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    original, pids = describe(), processes()
    if out.exists() or len(pids) != 1 or original['busy'] or original['idle_override']:
        raise RuntimeError('Need a settled player without an idle override and a new output')
    before = json.loads((MOD / 'state/state.json').read_text())
    core = json.loads((MOD / 'core.json').read_text())
    core_sha256 = sha(MOD / 'cores' / core['file'])
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
        if json.loads((MOD / 'core.json').read_text()) != core or sha(MOD / 'cores' / core['file']) != core_sha256:
            raise RuntimeError('Installed core changed')
        if identity(current) != target or current['customize'] != expected or current['idle_override'] != idle:
            raise RuntimeError('Concurrent appearance change')
        if not geometry_restored(baseline, geometry()):
            raise RuntimeError('Player or camera moved between cases')

    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', before)
    atomic(out / 'cine-before.json', cine)
    report = dict(passed=False, original=original, player=player, geometry_before=baseline,
                  core=core, core_sha256=core_sha256, cases=[], requested_caps=args.caps,
                  only=args.only, start_at=args.start_at, record_resets=args.record_resets)
    atomic(out / 'result.json', report)
    try:
        customize({'action': 'idle', 'value': 'eve'})
        idle = 'eve'
        selected = args.start_at is None
        for outfit, variants in OUTFITS.items():
            for variant in variants:
                selected = selected or args.start_at == outfit + '/' + variant
                if not selected:
                    continue
                if args.only and args.only != outfit + '/' + variant:
                    continue
                guard()
                target = original['shell'], outfit, variant
                expected = memories[outfit + '/' + variant]
                customize({'action': 'wear', 'outfit': outfit, 'variant': variant,
                           'customize': expected, 'persist': False})
                settled(target, custom=expected)
                time.sleep(2)
                guard()
                folder = out / (outfit + '-' + variant)
                command = [sys.executable, str(ROOT / 'tools/diagnostics/capture_capped_cost.py'),
                           '--output', str(folder), '--outfit', outfit, '--variant', variant, '--all-rigs',
                           '--caps', *[str(cap) for cap in args.caps]]
                if args.record_resets:
                    command.append('--record-resets')
                row = dict(outfit=outfit, variant=variant, evidence=str(folder), command=command)
                report['cases'].append(row)
                atomic(out / 'result.json', report)
                with (out / (folder.name + '.log')).open('x') as log:
                    result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
                row['returncode'] = result.returncode
                if (folder / 'result.json').exists():
                    child = json.loads((folder / 'result.json').read_text())
                    row['passed'] = child['passed']
                    row['observations_complete'] = child.get('observations_complete', False)
                atomic(out / 'result.json', report)
                if args.record_resets and result.returncode in (0, 2) and row.get('observations_complete'):
                    subprocess.run([sys.executable, str(ROOT / 'tools/diagnostics/verify_capped_cost.py'),
                                    str(folder), '--observations'], check=True)
                else:
                    result.check_returncode()
                if not row.get('passed') and not (args.record_resets and row.get('observations_complete')):
                    raise RuntimeError('Child did not verify captures and restoration')
                print(outfit, variant, args.caps, 'collected; reset-free:', row['passed'], flush=True)
        report['cases_passed'] = bool(report['cases']) and all(row.get('passed') for row in report['cases'])
        report['cases_complete'] = bool(report['cases']) and all(row.get('observations_complete') for row in report['cases'])
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
            report['observations_complete'] = bool(report.get('cases_complete') and not report.get('error') and all(
                report.get(k) for k in ('state_restored', 'geometry_restored', 'cine_restored', 'process_preserved')))
            report['passed'] = report['observations_complete'] and report.get('cases_passed', False)
        except Exception as error:
            report['restore_error'] = repr(error)
        atomic(out / 'result.json', report)
    if not report['passed']:
        if args.record_resets and report.get('observations_complete'):
            raise SystemExit(2)
        raise SystemExit(1)


if __name__ == '__main__':
    main()
