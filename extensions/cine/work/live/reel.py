"""Roll a CINE take in the running game and prove the character is as she was. Python 3.14.

  reel.py PRESET [breakdown: off|preset|reel] [seconds between screenshots] [extra,take,times]

Snapshots her CSS look first, logs the take's status, takes a screenshot every 2.4 s when the
game window is on screen (../export-live/reel-*.jpg), then waits for her look to match the
snapshot. If it does not, it clears foreign materials, wears her look again and says so.
"""
import json
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / 'core/tools'))
import cssx  # noqa: E402

OUT = HERE.parent / 'export-live'
OUT.mkdir(exist_ok=True)
CINE = 'eins0fx.cine'


def req(value, timeout=20):
    reply = cssx.request(cssx.GAME, value, timeout)
    if not reply.get('ok'):
        raise RuntimeError(reply.get('error', 'request failed'))
    return reply.get('result')


def eng(request):
    return req({'op': 'engine', 'request': request})


def call(target, function, args=None):
    reply = eng({'op': 'call', 'target': target, 'function': function, 'args': args or {}})
    return reply.get('ReturnValue', reply) if isinstance(reply, dict) else reply


def css(request):
    return eng({'op': 'service.call', 'service': 'css.customize', 'version': 1, 'request': request})


def event(control, value=None):
    e = {'id': control}
    if value is not None:
        e['value'] = value
    return req({'op': 'event', 'extension': CINE, 'event': e})


def controls():
    model = req({'op': 'model', 'extension': CINE})
    return {c['id']: c for s in model['sections'] for c in s['controls']}, model.get('error')


def grab(name):
    try:
        clients = json.loads(subprocess.check_output(['hyprctl', '-j', 'clients']))
        monitors = json.loads(subprocess.check_output(['hyprctl', '-j', 'monitors']))
        game = [c for c in clients if c['class'] == 'steam_app_2584270' and c['title'].strip() == 'MortalShell2']
        if not game or game[0]['workspace']['name'] not in {m['activeWorkspace']['name'] for m in monitors}:
            return False
        (x, y), (w, h) = game[0]['at'], game[0]['size']
        subprocess.run(['grim', '-t', 'jpeg', '-q', '80', '-g', f'{x},{y} {w}x{h}', str(OUT / f'reel-{name}.jpg')], check=True)
        return True
    except Exception:
        return False


# The engine's own screenshot (with the HUD) works when the game window is on a hidden
# workspace. Files land in the game's Saved folder; collect() moves them here afterwards.
SHOTS = Path('/mnt/eins0fxE/SteamLibrary/steamapps/compatdata/2584270/pfx/drive_c/users/steamuser/AppData/Local/MortalShell2/Saved/Screenshots/Windows')
asked = []


def engine_shot(name):
    try:
        kismet = eng({'op': 'find', 'path': '/Script/Engine.Default__KismetSystemLibrary'})
        call(kismet, 'ExecuteConsoleCommand', {'WorldContextObject': eng({'op': 'player'})['pawn'], 'Command': 'shot showui', 'SpecificPlayer': None})
        asked.append(name)
        return True
    except Exception:
        return False


def collect(before):
    time.sleep(1.5)
    fresh = sorted(f for f in SHOTS.glob('ScreenShot*.png') if f.name not in before) if SHOTS.exists() else []
    for name, f in zip(asked, fresh):
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(f), '-q:v', '3', str(OUT / f'reel-{name}.jpg')], check=False)
    for f in fresh:
        f.unlink()
    return len(fresh)


def slot_names():
    mesh = eng({'op': 'get', 'target': eng({'op': 'player'})['pawn'], 'property': 'Mesh'})
    names = []
    for i in range(call(mesh, 'GetNumMaterials')):
        material = call(mesh, 'GetMaterial', {'ElementIndex': i})
        names.append(material.get('name', '?').split('.')[-1].split(':')[-1] if isinstance(material, dict) else None)
    return mesh, names


def same_look(now, own):
    return now['outfit'] == own['outfit'] and now['variant'] == own['variant'] and now['customize'] == own['customize'] and not now.get('busy')


def main():
    preset = sys.argv[1] if len(sys.argv) > 1 else 'showreel-124'
    breakdown = sys.argv[2] if len(sys.argv) > 2 else 'preset'
    every = float(sys.argv[3]) if len(sys.argv) > 3 else 2.4   # seconds between screenshots
    extra = sorted(float(x) for x in sys.argv[4].split(',')) if len(sys.argv) > 4 else []   # more screenshots at these take times
    for old in OUT.glob('reel-t*.jpg'):
        old.unlink()
    shots_before = {f.name for f in SHOTS.glob('ScreenShot*.png')} if SHOTS.exists() else set()
    own = css({'action': 'describe'})
    if own.get('busy'):
        raise SystemExit('CSS is busy before the take; not starting')
    (OUT / 'own-look.json').write_text(json.dumps(own))
    print('own look:', own['outfit'], own['variant'], own['palette'])
    c, _ = controls()
    event('preset', preset); event('breakdown', breakdown); event('countdown', 0); event('guide', 'off')
    if c['enter'].get('enabled', True):
        event('enter'); time.sleep(0.6)
    event('take')
    began, last, rolled, shots, next_shot = time.monotonic(), '', None, 0, 0.0
    while time.monotonic() - began < 170:
        time.sleep(0.2)
        c, error = controls()
        status = c['status']['value']
        if status != last:
            print(f'{time.monotonic() - began:6.1f}s  {status}' + (f'  ERROR {error}' if error else ''))
            last = status
        if rolled is None and 'Rolling' in status:
            rolled = time.monotonic()
        if rolled and time.monotonic() - rolled >= next_shot:
            at = time.monotonic() - rolled
            shots += grab(f't{at:05.1f}') or engine_shot(f't{at:05.1f}')
            next_shot = at + every
        if rolled and extra and time.monotonic() - rolled >= extra[0]:
            at = time.monotonic() - rolled
            extra.pop(0)
            shots += grab(f't{at:05.1f}') or engine_shot(f't{at:05.1f}')
        if rolled and any(word in status for word in ('finished', 'cancel', 'stopped', 'Could not')):
            break
        if error and not rolled:
            break
    print('screenshots:', shots, '| from the engine:', collect(shots_before))
    now = own
    for _ in range(24):   # up to 12 s for her own look to land
        time.sleep(0.5)
        now = css({'action': 'describe'})
        if same_look(now, own):
            break
    mesh, names = slot_names()
    left = [n for n in names if n and '_Cine_' in n]
    if same_look(now, own) and not left:
        print('RESTORED: her look is identical to before, no CINE material on her, CSS idle')
    else:
        print('NOT RESTORED:', now['outfit'], now['variant'], now['palette'], 'busy', now.get('busy'), '| CINE materials left:', len(left))
        try:
            event('exit')
        except Exception:
            pass
        for i in range(len(names)):
            call(mesh, 'SetMaterial', {'ElementIndex': i, 'Material': None})
        time.sleep(2.0)
        css({'action': 'wear', 'outfit': own['outfit'], 'variant': own['variant'], 'customize': own['customize'], 'persist': True})
        for _ in range(20):
            time.sleep(0.5)
            now = css({'action': 'describe'})
            if same_look(now, own):
                break
        print('repaired by hand:', same_look(now, own))
    log = cssx.GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem/CSS.log'
    blocked = [line for line in log.read_text(errors='replace').splitlines()[-60:] if 'blocked' in line]
    print('CSS blocked lines in its last 60:', blocked[-2:] if blocked else 'none')


if __name__ == '__main__':
    main()
