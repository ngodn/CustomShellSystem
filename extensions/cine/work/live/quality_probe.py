"""Hold a face close-up in the running game and photograph it under different render settings. Python 3.14.

Writes ../export-probe/quality-<stage>.jpg (a full-resolution crop of the frame centre) and puts
every console variable back. The test preset is written into the game's CINE folder and removed.
"""
import json
import subprocess
import sys
import time
from pathlib import Path

import reel as r

OUT = Path(__file__).resolve().parent.parent / 'export-probe'
PRESETS = r.cssx.mod_root(r.cssx.GAME) / 'extensions/eins0fx.cine/presets'
TEST = PRESETS / 'zz-quality-probe.cine.json'
STAGES = [
    ('a-game', {}),
    ('b-rays', {'r.Shadow.Virtual.SMRT.RayCountDirectional': 16, 'r.Shadow.Virtual.SMRT.SamplesPerRayDirectional': 8, 'r.Shadow.Virtual.SMRT.RayCountLocal': 16, 'r.Shadow.Virtual.SMRT.SamplesPerRayLocal': 8}),
    ('c-dither-6', {'r.Shadow.Virtual.SMRT.TexelDitherScaleLocal': 6, 'r.Shadow.Virtual.SMRT.TexelDitherScaleDirectional': 6}),
    ('d-dither-14', {'r.Shadow.Virtual.SMRT.TexelDitherScaleLocal': 14, 'r.Shadow.Virtual.SMRT.TexelDitherScaleDirectional': 14}),
    ('e-vsm-off-test', {'r.Shadow.Virtual.SMRT.TexelDitherScaleLocal': 2, 'r.Shadow.Virtual.SMRT.TexelDitherScaleDirectional': 2, 'r.Shadow.Virtual.SMRT.MaxRayAngleFromLight': 0.2}),
    ('f-ray-angle', {'r.Shadow.Virtual.SMRT.MaxRayAngleFromLight': 0.03, 'r.Shadow.Virtual.SMRT.TexelDitherScaleLocal': 6, 'r.Shadow.Virtual.SMRT.TexelDitherScaleDirectional': 6}),
]


def main():
    kismet = r.eng({'op': 'find', 'path': '/Script/Engine.Default__KismetSystemLibrary'})
    pawn = r.eng({'op': 'player'})['pawn']

    def get(name):
        return r.call(kismet, 'GetConsoleVariableFloatValue', {'VariableName': name})

    def put(name, value):
        r.call(kismet, 'ExecuteConsoleCommand', {'WorldContextObject': pawn, 'Command': f'{name} {value:g}', 'SpecificPlayer': None})

    pose = {'azimuth': float(sys.argv[1]) if len(sys.argv) > 1 else 18, 'distance': 100, 'height': 61, 'aim': 61, 'fov': 30, 'roll': 0}
    TEST.write_text(json.dumps({'schema': 1, 'id': 'zz-quality-probe', 'name': 'zz quality probe', 'kind': 'sequence', 'reference': 'capsule',
                                'tempo': {'bpm': 120, 'offset': 0}, 'end': 31, 'shots': [{'at': 1, 'from': pose}]}))
    saved, before = {}, {f.name for f in r.SHOTS.glob('ScreenShot*.png')} if r.SHOTS.exists() else set()
    old = {k: v['value'] for k, v in r.controls()[0].items() if k in ('preset', 'picture', 'breakdown', 'countdown')}
    try:
        r.event('reload'); r.event('preset', 'zz-quality-probe'); r.event('picture', 'game'); r.event('breakdown', 'off'); r.event('countdown', 0)
        if r.controls()[0]['enter'].get('enabled', True):
            r.event('enter'); time.sleep(0.6)
        r.event('take')
        for _ in range(100):
            time.sleep(0.2)
            if 'Rolling' in r.controls()[0]['status']['value']:
                break
        else:
            raise SystemExit('the probe take did not roll: ' + r.controls()[0]['status']['value'])
        time.sleep(3.0)
        for name, values in STAGES:
            for cvar, value in values.items():
                saved.setdefault(cvar, get(cvar))
                put(cvar, value)
            time.sleep(3.0)   # let the temporal passes settle
            r.engine_shot(name)
            time.sleep(0.8)
    finally:
        for cvar, value in saved.items():
            put(cvar, value)
        try:
            r.event('take')   # F8 again cancels
        except Exception:
            pass
        time.sleep(1.0)
        fresh = sorted(f for f in r.SHOTS.glob('ScreenShot*.png') if f.name not in before)
        for name, f in zip(r.asked, fresh):
            subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(f), '-vf', 'crop=1100:760:730:240', '-q:v', '2', str(OUT / f'quality-{name}.jpg')], check=False)
        for f in fresh:
            f.unlink()
        TEST.unlink(missing_ok=True)
        try:
            r.event('reload')
            for key, value in old.items():
                r.event(key, value)
        except Exception as e:
            print('settings not put back:', e)
        print('stages:', r.asked, '| restored:', {k: get(k) for k in saved}, '| status:', r.controls()[0]['status']['value'])


if __name__ == '__main__':
    main()
