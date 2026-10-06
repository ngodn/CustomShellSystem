"""Perf Tuner live checks over the CSSX dev channel. Python 3.14. Nothing runs on import.

  check.py model            print the tuner's menu model (status, sections, findings)
  check.py library          the extension library (is the tuner loaded, its cost)
  check.py event <id> [v]   send a control event (enable true, audit, probe, measure, tweak:<id> true)
  check.py wait <text>      poll the model until the status contains <text> (or 120 s)
  check.py report <name>    print a report file from the extension folder (engine-ini-report.txt, world-audit-report.txt)
  check.py frame [seconds]  frame.brief over the dev channel

Only `event` changes anything in the game, and only what the named control does.
"""
import json
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / 'core/tools'))
import cssx  # noqa: E402

EXT = 'eins0fx.perf-tuner'


def request(payload, timeout=30):
    reply = cssx.request(cssx.GAME, payload, timeout)
    if not reply.get('ok'):
        raise SystemExit(f"request failed: {reply.get('error')}")
    return reply['result']


def model():
    return request({'op': 'model', 'extension': EXT})


def print_model(m):
    print('status:', m.get('status'))
    if m.get('error'):
        print('error:', m['error'])
    for section in m.get('sections', []):
        print(f"\n== {section['title']}")
        if section.get('description'):
            print('  ', section['description'][:300])
        for c in section['controls']:
            line = f"  [{c['type']}] {c['label']}"
            if 'value' in c:
                line += f" = {c['value']}"
            if c.get('hint'):
                line += f"  ({c['hint']})"
            if c.get('enabled') is False:
                line += '  [disabled]'
            print(line)


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    cmd = sys.argv[1]
    if cmd == 'model':
        print_model(model())
    elif cmd == 'library':
        lib = request({'op': 'library'})
        for e in lib if isinstance(lib, list) else lib.get('extensions', []):
            if EXT in json.dumps(e):
                print(json.dumps(e, indent=2))
    elif cmd == 'event':
        cid = sys.argv[2]
        value = None
        if len(sys.argv) > 3:
            raw = sys.argv[3]
            value = True if raw == 'true' else False if raw == 'false' else raw
        event = {'id': cid}
        if value is not None:
            event['value'] = value
        print(json.dumps(request({'op': 'event', 'extension': EXT, 'event': event}), indent=2))
    elif cmd == 'wait':
        text = sys.argv[2]
        deadline = time.time() + 120
        while time.time() < deadline:
            m = model()
            status = str(m.get('status'))
            print(status)
            if text in status:
                break
            time.sleep(3)
    elif cmd == 'report':
        folder = cssx.mod_root(cssx.GAME) / 'output' / EXT
        print((folder / sys.argv[2]).read_text())
    elif cmd == 'frame':
        print(json.dumps(request({'op': 'frame.stats', 'seconds': float(sys.argv[2]) if len(sys.argv) > 2 else 10}), indent=2)[:3000])
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
