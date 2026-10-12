"""Capture renderer console values through CSSX without changing game settings."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'extensions/core/tools'))
from cssx import GAME, atomic, processes, request, sha


VARIABLES = (
    'r.ReflectionMethod', 'r.DynamicGlobalIlluminationMethod',
    'r.RayTracing', 'r.RayTracing.Shadows', 'r.Shadow.Denoiser',
    'r.Shadow.Denoiser.TemporalAccumulation', 'r.Shadow.Virtual.Enable',
    'r.Shadow.Virtual.SMRT.RayCountDirectional',
    'r.Shadow.Virtual.SMRT.SamplesPerRayDirectional', 'r.ContactShadows',
    'r.Lumen.HardwareRayTracing', 'r.Lumen.Reflections.Allow',
    'r.Lumen.Reflections.Temporal',
    'r.Lumen.Reflections.Temporal.MaxFramesAccumulated',
    'r.Lumen.Reflections.BilateralFilter', 'r.AntiAliasingMethod',
    'r.ScreenPercentage', 'sg.ShadowQuality', 'sg.ReflectionQuality',
    'sg.GlobalIlluminationQuality',
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    pids = processes()
    if args.output.exists() or len(pids) != 1:
        raise RuntimeError('Need one running game and a new output path')

    def engine(value):
        reply = request(GAME, {'op': 'engine', 'request': value})
        if not reply['ok']:
            raise RuntimeError(reply['error'])
        return reply['result']

    def appearance():
        return engine({'op': 'service.call', 'service': 'css.customize',
                       'version': 1, 'request': {'action': 'describe'}})['customize']

    result = dict(started=datetime.now(timezone.utc).isoformat(), pids=pids,
                  settings_written=False, gameplay_input=False, complete=False,
                  screenshot_synchronized=False,
                  limitations=['Sequential console reads are not an atomic snapshot.',
                               'Console defaults do not establish final view settings or active GPU passes.',
                               'This records this machine, not the reporter\'s configuration.'])
    try:
        result['script_sha256'] = sha(Path(__file__))
        result['player_before'] = engine({'op': 'player'})
        result['customize_before'] = appearance()
        system = engine({'op': 'find', 'path': '/Script/Engine.Default__KismetSystemLibrary'})
        result['getter'] = engine({'op': 'describe', 'target': system,
                                   'function': 'GetConsoleVariableStringValue'})

        def read(name):
            return engine({'op': 'call', 'target': system,
                           'function': 'GetConsoleVariableStringValue',
                           'args': {'VariableName': name}})['ReturnValue']

        result['missing_control'] = read('css.diagnostic.missing_9d0f0f1a')
        if result['missing_control'] != '':
            raise RuntimeError('Missing console variable did not return the expected empty string')
        for phase in ('before', 'after'):
            values = {name: read(name) for name in VARIABLES}
            result[phase] = {name: value if value != '' else {'unavailable': True}
                             for name, value in values.items()}
        result['customize_after'] = appearance()
        result['player_after'] = engine({'op': 'player'})
        result['pids_after'] = processes()
        result['settings_unchanged'] = result['before'] == result['after']
        result['customize_unchanged'] = result['customize_before'] == result['customize_after']
        result['player_unchanged'] = result['player_before'] == result['player_after']
        result['process_unchanged'] = result['pids'] == result['pids_after']
        result['complete'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        result['finished'] = datetime.now(timezone.utc).isoformat()
        atomic(args.output, result)
    print(json.dumps({key: value for key, value in result.items()
                      if key in ('complete', 'before') or key.endswith('_unchanged')}, indent=2))


if __name__ == '__main__':
    main()
