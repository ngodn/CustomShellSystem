"""Read camera settings and post-process volumes without changing the scene."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from css import GAME, atomic, processes, sha, wait_json


FIELDS = (
    'AutoExposureMethod', 'AutoExposureBias', 'AutoExposureBiasCurve',
    'AutoExposureMinBrightness', 'AutoExposureMaxBrightness',
    'AutoExposureLowPercent', 'AutoExposureHighPercent',
    'HistogramLogMin', 'HistogramLogMax', 'BloomIntensity',
    'LocalExposureHighlightContrastScale', 'LocalExposureShadowContrastScale',
    'FilmSlope', 'FilmToe', 'FilmShoulder',
)
CVAR_TYPES = {
    'r.DefaultFeature.AutoExposure': 'Int',
    'r.DefaultFeature.AutoExposure.Method': 'Int',
    'r.DefaultFeature.AutoExposure.Bias': 'Float',
    'r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange': 'Int',
    'r.DefaultFeature.LocalExposure.HighlightContrastScale': 'Float',
    'r.DefaultFeature.LocalExposure.ShadowContrastScale': 'Float',
}
CALLS = {'GetAllActorsOfClass', 'GetConsoleVariableIntValue', 'GetConsoleVariableFloatValue',
         'GetConsoleVariableStringValue'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True, help='expected installed diagnostic core hash')
    args = parser.parse_args()
    if args.output.exists():
        raise RuntimeError('Refusing to overwrite a capture')
    if len(args.core_sha256) != 64 or any(c not in '0123456789abcdef' for c in args.core_sha256):
        raise ValueError('Expected a lowercase SHA-256 digest')
    if not processes():
        raise RuntimeError('Game is not running; no request was sent')
    mod = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    core = json.loads((mod / 'core.json').read_text())['file']
    assert Path(core).name == core
    if sha(mod / 'cores' / core) != args.core_sha256:
        raise RuntimeError('Installed core differs from the requested diagnostic build; no request was sent')
    result = dict(started=datetime.now(timezone.utc).isoformat(), core_sha256=args.core_sha256,
                  requests_finished=False, appearance_writes=False, camera_input=False,
                  final_render_exposure_available=False, screenshot_synchronized=False,
                  limitations=['Camera settings, volume settings and project defaults are separate inputs.',
                      'No volume containment/blend calculation or final GPU exposure is captured.',
                      'Console strings and numeric values are sequential reads, not an atomic snapshot.'])

    def probe(**request):
        assert request['op'] in {'player', 'find', 'get', 'get.member', 'describe', 'call'}
        if request['op'] in ('describe', 'call'):
            assert request['function'] in CALLS
        rid = str(time.time_ns())
        atomic(mod / 'request.json', dict(id=rid, action='css_probe', request=request))
        reply = wait_json(mod / 'runtime/css-probe.json', lambda j: j.get('id') == rid)
        if not reply['ok']:
            raise RuntimeError(reply['error'])
        return reply['result']

    def member(target, *path):
        try:
            return probe(op='get.member', target=target, path=list(path))
        except RuntimeError as error:
            return {'unavailable': str(error)}

    def settings(target, *prefix):
        return {name: dict(value=member(target, *prefix, name),
                          override=member(target, *prefix, 'bOverride_' + name)) for name in FIELDS}

    def appearance():
        status = json.loads((mod / 'runtime/status.json').read_text())
        return {k: status.get(k) for k in ('applied', 'customize', 'animation')}

    def camera_pose(camera):
        return {name: member(camera, 'CameraCachePrivate', 'POV', name)
                for name in ('Location', 'Rotation', 'FOV')}

    def unavailable_paths(value, prefix=''):
        if isinstance(value, dict):
            if 'unavailable' in value or value.get('$unsupported') is True:
                return [prefix]
            return [path for key, item in value.items()
                    for path in unavailable_paths(item, prefix + '/' + str(key))]
        if isinstance(value, list):
            return [path for index, item in enumerate(value)
                    for path in unavailable_paths(item, prefix + '/' + str(index))]
        return []

    try:
        player = probe(op='player')
        result['player'] = player
        assert player['pawn'] and player['controller'], 'No playable character'
        mesh = probe(op='get', target=player['pawn'], property='Mesh')
        assert probe(op='get.member', target=player['pawn'], path=['Mesh']) == mesh
        for invalid, expected in (([], 'Member path must contain'),
                                  (['Mesh', 'AField'], 'Member path can only descend into structs')):
            try:
                probe(op='get.member', target=player['pawn'], path=invalid)
            except RuntimeError as error:
                if expected not in str(error):
                    raise
                continue
            raise AssertionError('Invalid member path unexpectedly accepted')
        result['member_query_checks_passed'] = True
        result['appearance_before'] = appearance()
        camera = probe(op='get', target=player['controller'], property='PlayerCameraManager')
        assert camera, 'No player camera manager'
        result['camera'] = camera
        result['camera_before'] = camera_pose(camera)
        result['camera_postprocess_weight'] = member(camera, 'CameraCachePrivate', 'POV', 'PostProcessBlendWeight')
        result['camera_settings'] = settings(camera, 'CameraCachePrivate', 'POV', 'PostProcessSettings')
        system = probe(op='find', path='/Script/Engine.Default__KismetSystemLibrary')
        gameplay = probe(op='find', path='/Script/Engine.Default__GameplayStatics')
        volume_class = probe(op='find', path='/Script/Engine.PostProcessVolume')
        assert system and gameplay and volume_class
        result['function_metadata'] = {}
        for function in sorted(CALLS):
            target = gameplay if function == 'GetAllActorsOfClass' else system
            result['function_metadata'][function] = probe(op='describe', target=target, function=function)
        result['project_defaults'] = {}
        for name, kind in CVAR_TYPES.items():
            raw = probe(op='call', target=system, function='GetConsoleVariableStringValue',
                        args={'VariableName': name})['ReturnValue']
            if raw == '':
                result['project_defaults'][name] = {'unavailable': 'Console variable not found'}
                continue
            value = probe(op='call', target=system, function='GetConsoleVariable' + kind + 'Value',
                          args={'VariableName': name})['ReturnValue']
            result['project_defaults'][name] = {'string': raw, 'value': value}
        volumes = probe(op='call', target=gameplay, function='GetAllActorsOfClass',
            args={'WorldContextObject': player['pawn'], 'ActorClass': volume_class})['OutActors']
        result['volume_count'] = len(volumes)
        result['volumes_not_sampled'] = volumes[8:]
        result['volumes'] = []
        for volume in volumes[:8]:
            row = dict(object=volume, **{name: member(volume, name) for name in
                       ('bEnabled', 'bUnbound', 'Priority', 'BlendWeight', 'BlendRadius')})
            if row['bEnabled'] is True:
                row['settings'] = settings(volume, 'Settings')
            result['volumes'].append(row)
        result['camera_after'] = camera_pose(camera)
        result['appearance_after'] = appearance()
        result['player_after'] = probe(op='player')
        result['camera_unchanged'] = (None if unavailable_paths(result['camera_before']) or
                                     unavailable_paths(result['camera_after']) else
                                     result['camera_before'] == result['camera_after'])
        result['appearance_unchanged'] = result['appearance_before'] == result['appearance_after']
        result['player_unchanged'] = player == result['player_after']
        result['requests_finished'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        result['unavailable_fields'] = unavailable_paths(result)
        result['finished'] = datetime.now(timezone.utc).isoformat()
        atomic(args.output, result)
    print(json.dumps({k: result[k] for k in ('requests_finished', 'camera_unchanged',
        'appearance_unchanged', 'player_unchanged', 'volume_count', 'unavailable_fields')}, indent=2))


if __name__ == '__main__':
    main()
