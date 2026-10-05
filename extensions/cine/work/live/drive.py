"""Drive CINE 1.1.0 in the running game through the CSSX dev channel. Python 3.14.

  drive.py state                 CINE's menu values and CSSX status
  drive.py scene [enemies|everyone]   clean scene on, screenshots, leave, screenshots
  drive.py breakdown [PRESET] [reel|preset]   roll a breakdown take, screenshot every 2 s, cost
  drive.py take PRESET           roll any preset and report status until it ends
  drive.py leave                 leave Cine World

Screenshots (only when the game window is on screen) go to ../export-live/.
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
STAMP = time.strftime('%H%M%S')
CINE = 'eins0fx.cine'


def req(value, timeout=20):
    reply = cssx.request(cssx.GAME, value, timeout)
    if not reply.get('ok'):
        raise RuntimeError(reply.get('error', 'request failed'))
    return reply.get('result')


def model():
    return req({'op': 'model', 'extension': CINE})


def controls():
    return {c['id']: c for section in model().get('sections', []) for c in section.get('controls', [])}


def values():
    return {key: c.get('value') for key, c in controls().items()}


def event(control, value=None):
    e = {'id': control}
    if value is not None:
        e['value'] = value
    return req({'op': 'event', 'extension': CINE, 'event': e})


def grab(name):
    try:
        clients = json.loads(subprocess.check_output(['hyprctl', '-j', 'clients']))
        monitors = json.loads(subprocess.check_output(['hyprctl', '-j', 'monitors']))
        game = [c for c in clients if c['class'] == 'steam_app_2584270' and c['title'].strip() == 'MortalShell2']
        shown = {m['activeWorkspace']['name'] for m in monitors}
        if not game or game[0]['workspace']['name'] not in shown:
            print(f'  (no screenshot {name}: game window not on screen)')
            return
        (x, y), (w, h) = game[0]['at'], game[0]['size']
        path = OUT / f'{STAMP}-{name}.jpg'
        subprocess.run(['grim', '-t', 'jpeg', '-q', '85', '-g', f'{x},{y} {w}x{h}', str(path)], check=True)
        print(f'  screenshot: {path.name}')
    except Exception as error:
        print(f'  (no screenshot {name}: {error})')


def cost():
    for row in req({'op': 'library'}).get('extensions', []):
        if row.get('id') == CINE:
            return row.get('cost', {})
    return {}


def show(*keys):
    v = values()
    for key in keys or ('status', 'scene_info', 'breakdown_info'):
        print(f'  {key}: {v.get(key)}')
    error = model().get('error')
    if error:
        print(f'  error: {error}')


def enter():
    if controls().get('enter', {}).get('enabled', True):   # the Enter button is off once inside
        event('enter')
        time.sleep(0.6)


def roll(preset, breakdown='off', shots_every=2.0, label='take'):
    event('preset', preset)
    event('breakdown', breakdown)
    event('countdown', 0)
    enter()
    before = cost()
    began = time.monotonic()
    event('take')
    last, rolling, rolled_at, next_shot = '', False, 0.0, 0.0
    while time.monotonic() - began < 140:
        time.sleep(0.25)
        status = values().get('status', '')
        if status != last:
            print(f'  {time.monotonic() - began:5.1f}s  {status}')
            last = status
        if not rolling and 'Rolling' in status:
            rolling, rolled_at = True, time.monotonic()
        if rolling and time.monotonic() - rolled_at >= next_shot:   # named by seconds into the take
            at = time.monotonic() - rolled_at
            grab(f'{label}-t{at:05.1f}')
            next_shot = at + shots_every
        if rolling and ('finished' in status or 'ended' in status or 'cancel' in status or 'stopped' in status or 'Could not' in status):
            break
    after = cost()
    seconds = time.monotonic() - began
    print('  cost over the take:', {k: round((after.get(k, 0) - before.get(k, 0)) / seconds, 1) for k in after if isinstance(after.get(k), (int, float)) and k in before}, 'per second')
    time.sleep(1.0)
    grab(f'{label}-after')
    show()


def main():
    args = sys.argv[1:] or ['state']
    what = args[0]
    if what == 'wait':   # until the save is loaded: a real shell, not the main menu character
        deadline = time.monotonic() + 3600
        while time.monotonic() < deadline:
            try:
                p = cssx.request(cssx.GAME, {'op': 'engine', 'request': {'op': 'player'}}, 8).get('result') or {}
                if p.get('shell') not in (None, 'None') and 'MainMenu' not in json.dumps(p.get('pawn', {})):
                    time.sleep(6)   # let the load-in finish
                    print('in the world as', p.get('shell'))
                    return
            except Exception:
                pass
            time.sleep(4)
        print('timeout waiting for the world')
    elif what == 'state':
        print(json.dumps(req({'op': 'status'}), indent=1)[:600])
        print(json.dumps(values(), indent=1))
    elif what == 'scene':
        mode = args[1] if len(args) > 1 else 'enemies'
        grab('scene-0-before')
        event('scene', mode)
        enter()
        for i in range(6):
            time.sleep(1.0)
            show('scene_info')
        grab('scene-1-clean')
        print('  cost:', cost())
        event('exit')
        time.sleep(1.5)
        grab('scene-2-restored')
        event('scene', 'off')
        show()
    elif what == 'breakdown':
        roll(args[1] if len(args) > 1 else 'breakdown-turntable', args[2] if len(args) > 2 else 'preset', 2.0, 'breakdown')
    elif what == 'take':
        roll(args[1], 'off', 3.0, 'take')
    elif what == 'leave':
        event('exit')
        show()
    else:
        print(__doc__)


main()
