"""Check both Stray clones for one saved look and restore the original selection."""
import argparse
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys

from capture_eve_surface_variant import ROOT, GAME, atomic, describe, identity, settled, processes, sha
from check_peony_live_presets import customize, engine, request
from check_outfit_switch_lifecycle import OUTFITS, MOD
from verify_doubles_summon import verify_appearance
sys.path.insert(0, str(ROOT / 'tools'))
from css import wait_json

CASES = {'peony': ('eins0fx.seduxtress', 'peony'),
         'ug-eve': ('eins0fx.unholygenessa', 'eve'),
         'ug-genessa': ('eins0fx.unholygenessa', 'genessa')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=CASES, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    args = parser.parse_args()
    out, pids, original = args.output.resolve(), processes(), describe()
    if out.exists() or not out.is_relative_to((ROOT / 'work').resolve()) or len(pids) != 1:
        raise RuntimeError('Need one game and a new evidence directory under work')
    if original['busy'] or original['idle_override'] or original['shell'] != 'CharacterId.Player.Darkform.CorruptedGenessa':
        raise RuntimeError('Need settled Stray without an external idle override')
    focus = json.loads(subprocess.check_output(['hyprctl', 'activewindow', '-j']))
    if focus.get('pid') != pids[0] or focus.get('class') != 'steam_app_2584270':
        raise RuntimeError('Game is not focused; nothing changed')
    before = json.loads((MOD / 'state/state.json').read_text())
    if before['harbinger_mirror'] or before['doubles']['stray'] != 'css':
        raise RuntimeError('Need an independent Stray appearance and Use CSS doubles')
    core = json.loads((MOD / 'core.json').read_text())
    if sha(MOD / 'cores' / core['file']) != args.core_sha256 or core['file'] not in Path(f'/proc/{pids[0]}/maps').read_text():
        raise RuntimeError('Expected core is not installed and mapped')
    outfit, variant = CASES[args.case]
    memories = before['remembered_custom']
    restore_variant = next((v for v in OUTFITS[outfit] if memories.get(outfit+'/'+v) == memories[outfit]), None)
    if restore_variant is None or outfit+'/'+variant not in memories:
        raise RuntimeError('Cannot restore the existing outfit memories')
    player = engine({'op': 'player'})

    def geometry():
        result = {}
        for name, key, function in (
            ('position', 'pawn', 'K2_GetActorLocation'), ('rotation', 'pawn', 'K2_GetActorRotation'),
            ('view_target', 'controller', 'GetViewTarget'), ('control_rotation', 'controller', 'GetControlRotation')):
            engine({'op': 'describe', 'target': player[key], 'function': function})
            result[name] = engine({'op': 'call', 'target': player[key], 'function': function, 'args': {}})['ReturnValue']
        return result

    expected, target, custom = copy.deepcopy(before), identity(original), original['customize']

    def guard():
        current = describe()
        if processes() != pids or engine({'op': 'player'}) != player:
            raise RuntimeError('Player or process changed')
        if identity(current) != target or current['customize'] != custom or current['idle_override']:
            raise RuntimeError('New appearance preserved; restoration stopped')
        if json.loads((MOD / 'state/state.json').read_text()) != expected:
            raise RuntimeError('New saved settings preserved; restoration stopped')

    def wear(next_outfit, next_variant, next_custom):
        nonlocal expected, target, custom
        guard()
        next_state = copy.deepcopy(expected)
        next_state['selections'][original['shell']] = dict(outfit=next_outfit, variant=next_variant, customize=next_custom)
        next_state['remembered_custom'][next_outfit] = next_custom
        next_state['remembered_custom'][next_outfit+'/'+next_variant] = next_custom
        customize(dict(action='wear', outfit=next_outfit, variant=next_variant, customize=next_custom, persist=True))
        expected, target, custom = next_state, (original['shell'], next_outfit, next_variant), next_custom
        settled(target, custom=custom)
        wait_json(MOD / 'state/state.json', lambda s: s == expected)

    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    if not cine['ok']:
        raise RuntimeError('Cannot inspect CINE')
    buttons = {c['id']: c.get('enabled', True) for s in cine['result']['sections'] for c in s['controls']}
    if not buttons.get('record', False) or not buttons.get('capture', False):
        raise RuntimeError('CINE is recording or playing a take; nothing changed')
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', before)
    baseline = geometry()
    report = dict(passed=False, original=original, player=player, core=core,
                  core_sha256=args.core_sha256, geometry_before=baseline, case=args.case)
    try:
        wear(outfit, variant, memories[outfit+'/'+variant])
        command = [sys.executable, str(ROOT / 'tools/diagnostics/check_doubles_forms.py'),
                   '--output', str(out / 'summon'), '--cases', 'stray-css', '--core-sha256', args.core_sha256]
        report['command'] = command
        atomic(out / 'result.json', report)
        with (out / 'summon.log').open('x') as log:
            child = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        report['returncode'] = child.returncode
        child.check_returncode()
        report['verification'] = verify_appearance(out / 'summon/stray-css', 'stray', secondary=True)
    except Exception as error:
        report['error'] = repr(error)
    finally:
        try:
            wear(outfit, restore_variant, memories[outfit])
            wear(original['outfit'], original['variant'], original['customize'])
            after = json.loads((MOD / 'state/state.json').read_text())
            atomic(out / 'state-after.json', after)
            report['state_restored'] = after == before
            report['geometry_after'] = geometry()
            report['geometry_restored'] = report['geometry_after'] == baseline
            report['cine_restored'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine['result']
            report['appearance_restored'] = all(describe()[k] == original[k] for k in ('shell', 'outfit', 'variant', 'customize', 'idle_override'))
            report['passed'] = bool('verification' in report and 'error' not in report and all(report[k] for k in
                ('state_restored', 'geometry_restored', 'cine_restored', 'appearance_restored')))
        except Exception as error:
            report['restore_error'] = repr(error)
        atomic(out / 'result.json', report)
    print(json.dumps({k: report.get(k) for k in ('passed', 'error', 'restore_error', 'verification',
        'state_restored', 'geometry_restored', 'cine_restored', 'appearance_restored')}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
