"""Capture one outfit at native frame caps, preserving console priority and settings."""
import argparse
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import time

from capture_eve_surface_variant import ROOT, describe, identity, processes, sha, atomic
from check_peony_live_presets import engine, sample, request, GAME
from check_sidearm_aim_visibility import MOD, geometry_restored
from read_frame_cap_flags import FrameCapFlags
from sample_secondary_rigs import sample_secondary_rigs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--outfit', default='eins0fx.seduxtress')
    parser.add_argument('--variant', default='peony')
    parser.add_argument('--all-rigs', action='store_true')
    parser.add_argument('--record-resets', action='store_true',
                        help='Continue collecting after reset events, without marking them as passing')
    parser.add_argument('--caps', type=float, nargs='+', choices=(30., 60., 999.), default=[30., 60., 999.])
    args = parser.parse_args()
    if len(set(args.caps)) != len(args.caps):
        parser.error('Frame caps must not repeat')
    if args.record_resets and not args.all_rigs:
        parser.error('--record-resets requires --all-rigs')
    out = args.output.resolve()
    pids = processes()
    if out.exists() or len(pids) != 1:
        raise RuntimeError('Need one running game and a new output directory')
    original = describe()
    if original['busy'] or (original['outfit'], original['variant']) != (args.outfit, args.variant):
        raise RuntimeError('Expected settled requested outfit')
    sample_rigs = sample_secondary_rigs if args.all_rigs else sample
    out.mkdir(parents=True)
    for name in ('capture_capped_cost.py', 'capture_stationary_cost.py', 'read_frame_cap_flags.py',
                 'sample_secondary_rigs.py', 'verify_capped_cost.py'):
        shutil.copyfile(ROOT / 'tools/diagnostics' / name, out / name)
    flags = FrameCapFlags(pids[0])
    before = json.loads((MOD / 'state/state.json').read_text())
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result']
    settings_path = (GAME.parents[2] / 'compatdata/2584270/pfx/drive_c/users/steamuser/'
                     'AppData/Local/MortalShell2/Saved/Config/Windows/GameUserSettings.ini')
    settings_sha = sha(settings_path)
    shutil.copyfile(settings_path, out / 'GameUserSettings-before.ini')
    log = (out / 'requests.jsonl').open('x')

    def send(value):
        log.write(json.dumps({'request': value}) + '\n'); log.flush()
        result = engine(value)
        log.write(json.dumps({'result': result}) + '\n'); log.flush()
        return result

    def call(target, function, **fields):
        send({'op': 'describe', 'target': target, 'function': function})
        return send({'op': 'call', 'target': target, 'function': function, 'args': fields}).get('ReturnValue')

    player = send({'op': 'player'})
    default = send({'op': 'find', 'path': '/Script/Sparta.Default__SpartaGameUserSettings'})
    settings = call(default, 'GetSpartaUserSettings')
    system = send({'op': 'find', 'path': '/Script/Engine.Default__KismetSystemLibrary'})
    original_cap = call(settings, 'GetFrameRateLimit_Always')
    expected_cap = original_cap
    if original_cap != 999.:
        raise RuntimeError('This run expects the recorded original 999 cap')

    def geometry():
        return {k: call(player[t], f) for k, t, f in (
            ('position', 'pawn', 'K2_GetActorLocation'), ('rotation', 'pawn', 'K2_GetActorRotation'),
            ('control_rotation', 'controller', 'GetControlRotation'), ('view_target', 'controller', 'GetViewTarget'))}

    def cvars():
        return {n: call(system, 'GetConsoleVariableStringValue', VariableName=n) for n in
                ('t.MaxFPS', 'r.VSync', 'r.ScreenPercentage', 'r.DynamicRes.OperationMode')}

    baseline, original_cvars = geometry(), cvars()
    window = lambda name: json.loads(subprocess.check_output(['hyprctl', name, '-j']))
    prior_window = window('activewindow')
    game_window = next(w for w in window('clients') if w['pid'] == pids[0] and w['class'] == 'steam_app_2584270')

    def focus(address):
        if not address.startswith('0x') or any(c not in '0123456789abcdef' for c in address[2:]):
            raise RuntimeError('Invalid window address')
        subprocess.run(['hyprctl', 'dispatch', 'hl.dsp.focus({window="address:' + address + '"})'],
                       check=True, capture_output=True)

    def guard():
        current = describe()
        if processes() != pids or send({'op': 'player'}) != player:
            raise RuntimeError('Game or player changed')
        if identity(current) != identity(original) or any(current[k] != original[k] for k in ('customize', 'idle_override')):
            raise RuntimeError('Appearance changed')
        if not geometry_restored(baseline, geometry()):
            raise RuntimeError('Player or view changed')
        if json.loads((MOD / 'state/state.json').read_text()) != before or sha(settings_path) != settings_sha:
            raise RuntimeError('Saved settings changed')
        if call(settings, 'GetFrameRateLimit_Always') != expected_cap or flags.read() != flags.initial:
            raise RuntimeError('Frame cap or console ownership changed')
        values = cvars()
        if float(values.pop('t.MaxFPS')) != expected_cap:
            raise RuntimeError('Native cap did not reach t.MaxFPS')
        if any(values[n] != original_cvars[n] for n in values):
            raise RuntimeError('Unrelated graphics setting changed')
        if window('activewindow').get('address') != game_window['address']:
            raise RuntimeError('Game lost focus')

    def finite(value):
        if isinstance(value, dict): return all(finite(v) for v in value.values())
        if isinstance(value, list): return all(finite(v) for v in value)
        return not isinstance(value, float) or math.isfinite(value)

    report = dict(passed=False, player=player, original=original, original_cap=original_cap,
                  flags_before=flags.initial, geometry_before=baseline, cvars_before=original_cvars,
                  settings_file=str(settings_path), settings_sha256=settings_sha, cases=[],
                  gameplay_input=False, raw_memory_writes=False,
                  requested_caps=args.caps,
                  record_resets=args.record_resets,
                  rig_observer='secondary_rigs' if args.all_rigs else 'eve_body')
    atomic(out / 'state-before.json', before)
    atomic(out / 'result.json', report)
    try:
        focus(game_window['address']); guard()
        for cap in args.caps:
            guard()
            row = dict(cap=cap, before=flags.read())
            report['cases'].append(row)
            atomic(out / 'result.json', report)
            expected_cap = cap
            call(settings, 'SetFrameRateLimit_Always', NewLimitFPS=cap)
            time.sleep(1)
            guard()
            row['rig_before'] = sample_rigs()
            folder = out / ('cap-' + str(int(cap)))
            command = [sys.executable, str(ROOT / 'tools/diagnostics/capture_stationary_cost.py'),
                       '--output', str(folder), '--seconds', '10', '--repeats', '3']
            row['command'] = command
            atomic(out / 'result.json', report)
            with (out / (folder.name + '.log')).open('x') as stream:
                run = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
            row['returncode'] = run.returncode
            run.check_returncode()
            guard()
            row['capture'] = json.loads((folder / 'result.json').read_text())
            row['rig_after'] = sample_rigs()
            row['after'] = flags.read()
            if not row['capture']['passed'] or not finite(row['rig_after']):
                raise RuntimeError('Capture failed or body rig contains nonfinite values')
            if args.all_rigs:
                from verify_capped_cost import verify_secondary_rigs
                row['reset_events'] = verify_secondary_rigs(
                    row['rig_before'], row['rig_after'], record_resets=args.record_resets)
            elif row['rig_after']['rig']['TotalSteps'] <= row['rig_before']['rig']['TotalSteps']:
                raise RuntimeError('Body rig stopped updating')
            row['observations_complete'] = True
            row['passed'] = not row.get('reset_events')
            atomic(out / 'result.json', report)
            print(int(cap), 'cap: three captures complete; reset events:',
                  len(row.get('reset_events', [])), flush=True)
        report['captures_complete'] = True
    except Exception as error:
        report['error'] = repr(error)
    finally:
        try:
            if processes() != pids or call(settings, 'GetFrameRateLimit_Always') != expected_cap:
                raise RuntimeError('Cannot overwrite an independently changed cap')
            call(settings, 'SetFrameRateLimit_Always', NewLimitFPS=original_cap)
            expected_cap = original_cap
            report['flags_after'] = flags.read()
            report['cvars_after'] = cvars()
            report['geometry_after'] = geometry()
            report['state_restored'] = json.loads((MOD / 'state/state.json').read_text()) == before
            report['cine_restored'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine
            report['settings_file_unchanged'] = sha(settings_path) == settings_sha
            report['cap_restored'] = call(settings, 'GetFrameRateLimit_Always') == original_cap
            report['flags_restored'] = report['flags_after'] == flags.initial
            report['graphics_restored'] = report['cvars_after'] == original_cvars
            report['geometry_restored'] = geometry_restored(baseline, report['geometry_after'])
        except Exception as error:
            report['restore_error'] = repr(error)
        if window('activewindow').get('address') == game_window['address'] and any(
                w['address'] == prior_window['address'] for w in window('clients')):
            focus(prior_window['address'])
        report['focus_restored'] = window('activewindow').get('address') == prior_window['address']
        report['observations_complete'] = bool(report.get('captures_complete') and not report.get('error') and
            not report.get('restore_error') and all(report.get(k) for k in
                ('state_restored', 'cine_restored', 'settings_file_unchanged', 'cap_restored',
                 'flags_restored', 'graphics_restored', 'geometry_restored', 'focus_restored')))
        report['passed'] = report['observations_complete'] and all(row.get('passed') for row in report['cases'])
        atomic(out / 'result.json', report); log.close()
    if not report['passed']:
        if args.record_resets and report['observations_complete']:
            raise SystemExit(2)
        raise SystemExit(1)


if __name__ == '__main__':
    main()
