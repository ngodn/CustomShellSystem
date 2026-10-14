"""Temporarily wear one repaired Eve variant, capture it, and restore the look.

Uses the established CSS customization service. No movement or camera input.
"""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import css_capture
from css import GAME, atomic, processes, sha
from check_peony_live_presets import customize, sample


def describe():
    return customize({'action': 'describe'})


def identity(value):
    return value['shell'], value['outfit'], value['variant']


def settled(wanted, timeout=30, custom=None):
    end = time.monotonic()+timeout
    while time.monotonic() < end:
        value = describe()
        if not value['busy'] and identity(value) == wanted and (custom is None or value['customize'] == custom):
            return value
        time.sleep(.2)
    raise TimeoutError('CSS did not settle on the requested outfit')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=['black_pearl', 'prototype'], required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--show-clothing', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists() or len(processes()) != 1:
        raise RuntimeError('Need one running game and a new output directory')
    original = describe()
    if original['busy'] or original['outfit'] != 'eins0fx.seduxtress' or original['variant'] == args.variant:
        raise RuntimeError('Need a settled Eve appearance distinct from the target')
    out.mkdir(parents=True)
    mod = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    state_path = mod/'state/state.json'
    state_before = json.loads(state_path.read_text())
    atomic(out/'state-before.json', state_before)
    report = dict(original=original, process_before=processes(), started_ns=time.time_ns(),
                  requested_variant=args.variant, camera_input=False, movement_input=False,
                  restored=False, recipe_sha256=sha(Path(__file__)))
    atomic(out/'result.json', report)
    changed = False
    expected_custom = None
    target_original_custom = None
    target = original['shell'], original['outfit'], args.variant
    try:
        if describe() != original:
            raise RuntimeError('Appearance changed before switching')
        changed = True
        report['wear_reply'] = customize({'action': 'wear', 'outfit': original['outfit'],
                                         'variant': args.variant, 'persist': False})
        active = report['active'] = settled(target)
        expected_custom = active['customize']
        target_original_custom = copy.deepcopy(expected_custom)
        if args.show_clothing:
            garment_ids = [c['id'] for c in active['controls']
                           if c['kind'] == 'toggle' and c['role'] == 'garment']
            if not garment_ids:
                raise RuntimeError('No garment visibility controls found')
            shown = copy.deepcopy(expected_custom)
            for control in garment_ids:
                shown.setdefault('values', {})[control] = [1, 0, 0, 1]
            reply = customize({'action': 'apply', 'persist': False,
                               'commands': [{'action': 'restore', 'customize': shown}]})
            expected_custom = reply['customize']
            report['clothed'] = settled(target, custom=expected_custom)
        report['rig'] = sample()
        atomic(out/'result.json', report)
        command = [sys.executable, str(ROOT/'tools/diagnostics/check_live_animation_owner.py'),
            '--catalog', str(ROOT/'build/eve-surface-package1/metadata/CSS_SharedAssets_P/MortalShell2/Content/CSS/Packages/css.locomotion/manifest.json'),
            '--output', str(out/'owner.json')]
        atomic(out/'owner-command.json', command)
        with (out/'owner.log').open('x') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        atomic(out/'owner-exit.json', {'returncode': result.returncode})
        result.check_returncode()
        current = describe()
        if identity(current) != target or current['customize'] != expected_custom:
            raise RuntimeError('Concurrent appearance change before screenshot')
        css_capture.MEDIA = out
        report['screenshot'] = str(css_capture.shot(args.variant))
        report['after_capture'] = describe()
        if identity(report['after_capture']) != target or report['after_capture']['customize'] != expected_custom:
            raise RuntimeError('Concurrent appearance change during screenshot')
        report['capture_complete'] = True
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        try:
            if changed:
                current = describe()
                if processes() != report['process_before'] or identity(current) not in (target, identity(original)):
                    raise RuntimeError('Player or appearance changed independently; automatic restore skipped')
                if identity(current) == target and expected_custom is not None and current['customize'] != expected_custom:
                    raise RuntimeError('Customization changed independently; preserved it')
                if identity(current) == identity(original) and current['customize'] != original['customize']:
                    raise RuntimeError('Original appearance changed independently; preserved it')
                if identity(current) == target and target_original_custom != expected_custom:
                    customize({'action': 'apply', 'persist': False,
                               'commands': [{'action': 'restore', 'customize': target_original_custom}]})
                    report['target_restored'] = settled(target, custom=target_original_custom)
                customize({'action': 'wear', 'outfit': original['outfit'], 'variant': original['variant'],
                           'customize': original['customize'], 'persist': True})
                restored = report['after_restore'] = settled(identity(original))
                if restored['customize'] != original['customize'] or restored['idle_override'] != original['idle_override']:
                    raise RuntimeError('Original look or idle override was not restored exactly')
                report['restored'] = True
                time.sleep(1.2)
                state_after = json.loads(state_path.read_text())
                atomic(out/'state-after.json', state_after)
                report['saved_state_equal'] = state_after == state_before
                report['changed_state_keys'] = sorted(k for k in state_before.keys() | state_after.keys()
                                                     if state_before.get(k) != state_after.get(k))
                if not report['saved_state_equal']:
                    raise RuntimeError('Saved CSS state differs after restoration; inspect receipt')
        except Exception as error:
            report['restore_error'] = str(error)
            if 'error' not in report:
                raise
        finally:
            report['process_after'] = processes()
            report['finished_ns'] = time.time_ns()
            atomic(out/'result.json', report)
    print(json.dumps({k: report[k] for k in ('capture_complete', 'restored', 'saved_state_equal', 'changed_state_keys')}))


if __name__ == '__main__':
    main()
