"""Stage the dev build of CSSX core and CINE and make the running game load it. Python 3.14.

  reload.py          stage, then point core.json at a fresh copy of the core (the loader swaps to it)
  reload.py settle   point core.json back at the canonical core file and delete the copies

Refuses while a take is running.
"""
import json
import shutil
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / 'core/tools'))
import cssx  # noqa: E402


def status():
    reply = cssx.request(cssx.GAME, {'op': 'model', 'extension': 'eins0fx.cine'}, 20)
    if not reply.get('ok'):
        raise RuntimeError(reply.get('error', 'request failed'))
    model = reply['result']
    return next(c['value'] for s in model['sections'] for c in s['controls'] if c['id'] == 'status'), model.get('error')


def main():
    mod = cssx.mod_root(cssx.GAME)
    text, _ = status()
    if any(word in text for word in ('Rolling', 'Preparing', 'Building', 'Checking', 'Get ready', 'Closing')):
        raise SystemExit(f'a take is running ({text}); not reloading')
    current = json.loads((mod / 'core.json').read_text())['file']
    canonical = current.split('-r')[0].removesuffix('.dll') + '.dll'
    if len(sys.argv) > 1 and sys.argv[1] == 'settle':
        if current != canonical:
            cssx.atomic(mod / 'core.json', {'file': canonical})
            time.sleep(5)
        for copy in (mod / 'core').glob('*-r[0-9]*.dll'):
            copy.unlink()
        print('core.json ->', canonical, '|', status())
        return
    cssx.stage(cssx.GAME, True, False, True, False, False, True)
    canonical = json.loads((mod / 'core.json').read_text())['file'].split('-r')[0].removesuffix('.dll') + '.dll'
    fresh = f'{canonical.removesuffix(".dll")}-r{int(time.time()) % 100000}.dll'
    shutil.copy2(mod / 'core' / canonical, mod / 'core' / fresh)
    cssx.atomic(mod / 'core.json', {'file': fresh})
    time.sleep(6)
    for copy in (mod / 'core').glob('*-r[0-9]*.dll'):
        if copy.name != fresh:
            copy.unlink()
    print('reloaded ->', fresh, '|', status())


if __name__ == '__main__':
    main()
