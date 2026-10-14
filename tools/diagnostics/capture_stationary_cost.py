"""Repeat read-only CSS frame profiles with appearance and view guards."""
import argparse
import json
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import time

from capture_eve_surface_variant import ROOT, GAME, describe, identity, processes, sha, atomic
from check_peony_live_presets import engine

MOD = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=15, choices=range(10, 31))
    parser.add_argument('--repeats', type=int, default=3, choices=range(3, 7))
    args = parser.parse_args()
    out = args.output.resolve()
    pids = processes()
    if out.exists() or len(pids) != 1:
        raise RuntimeError('Need one running game and a new output directory')
    original = describe()
    if original['busy']:
        raise RuntimeError('Appearance is changing')
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out/'recipe.py')
    shutil.copyfile(ROOT/'tools/css_frame_profile.py', out/'frame-profile-recipe.py')
    player = engine({'op': 'player'})
    core = json.loads((MOD/'core.json').read_text())
    binary = MOD/'cores'/core['file']
    if core['file'] not in Path(f'/proc/{pids[0]}/maps').read_text():
        raise RuntimeError('Selected core is not mapped')
    saved = json.loads((MOD/'state/state.json').read_text())
    system = engine({'op': 'find', 'path': '/Script/Engine.Default__KismetSystemLibrary'})
    getter = engine({'op': 'describe', 'target': system, 'function': 'GetConsoleVariableStringValue'})

    def call(target, name, **fields):
        return engine({'op': 'call', 'target': target, 'function': name, 'args': fields})['ReturnValue']

    def context():
        current = describe()
        return dict(identity=identity(current), customize=current['customize'], idle_override=current['idle_override'],
                    player=engine({'op': 'player'}), position=call(player['pawn'], 'K2_GetActorLocation'),
                    rotation=call(player['pawn'], 'K2_GetActorRotation'),
                    control_rotation=call(player['controller'], 'GetControlRotation'),
                    view_target=call(player['controller'], 'GetViewTarget'),
                    cvars={n:call(system, 'GetConsoleVariableStringValue', VariableName=n) for n in
                           ('t.MaxFPS', 'r.VSync', 'r.ScreenPercentage', 'r.DynamicRes.OperationMode')})

    initial = context()
    report = dict(passed=False, process_before=pids, core=core, core_sha256=sha(binary),
                  getter=getter, context_before=initial, samples=[], settings_written=False,
                  gameplay_input=False, started_ns=time.time_ns())
    atomic(out/'state-before.json', saved)
    try:
        for i in range(args.repeats):
            if context() != initial or processes() != pids:
                raise RuntimeError('Appearance, player, camera or renderer setting changed')
            target = out/f'capture-{i+1}.json'
            command = [sys.executable, str(ROOT/'tools/css_frame_profile.py'), '--seconds', str(args.seconds),
                       '--core', str(binary), '--output', str(target)]
            atomic(out/f'command-{i+1}.json', command)
            with (out/f'capture-{i+1}.log').open('x') as log:
                result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
            sample = dict(file=str(target), returncode=result.returncode, context_after=context())
            report['samples'].append(sample)
            atomic(out/'result.json', report)
            result.check_returncode()
            data = json.loads(target.read_text())
            assert data['raw']['stop_reason'] == 'duration' and data['state_unchanged']
            assert sample['context_after'] == initial, 'Sample view or appearance changed'
            summary = data['summary']
            sample['summary'] = summary
            print(i+1, 'frames', summary['frames'], 'tick interval median/p95/p99 ms',
                  [summary['metrics']['interval_ms'][k] for k in ('p50', 'p95', 'p99')], flush=True)
        values = [s['summary']['metrics']['interval_ms']['p50'] for s in report['samples']]
        report['median_spread_ms'] = max(values)-min(values)
        report['median_of_medians_ms'] = statistics.median(values)
        report['captures_complete'] = True
    except Exception as error:
        report['error'] = repr(error)
        raise
    finally:
        report['process_after'] = processes()
        report['saved_state_equal'] = saved == json.loads((MOD/'state/state.json').read_text())
        report['core_unchanged'] = core == json.loads((MOD/'core.json').read_text()) and sha(binary) == report['core_sha256']
        report['finished_ns'] = time.time_ns()
        report['passed'] = bool(report.get('captures_complete') and report['saved_state_equal']
                              and report['core_unchanged'] and report['process_after'] == pids)
        atomic(out/'result.json', report)
    if not report['passed']:
        raise RuntimeError('Capture finished, but process, core or saved-state preservation failed')
    print(json.dumps({k:report[k] for k in ('passed', 'saved_state_equal', 'median_spread_ms', 'median_of_medians_ms')}))


if __name__ == '__main__':
    main()
