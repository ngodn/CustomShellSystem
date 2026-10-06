"""Put an engine stat overlay on screen and grab an engine screenshot to read it. Python 3.14.
  stat.py unit        'stat unit' on, screenshot, 'stat unit' off
  stat.py <command>   any console command, no screenshot
Nothing runs on import.
"""
import sys, time
from pathlib import Path
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / 'core/tools'))
import cssx

def engine(req):
    r = cssx.request(cssx.GAME, {'op': 'engine', 'request': req}, 30)
    if not r.get('ok'): raise SystemExit(r.get('error'))
    return r['result']

def exec_cmd(command):
    kismet = engine({'op': 'find', 'path': '/Script/Engine.Default__KismetSystemLibrary'})
    pawn = engine({'op': 'player'})['pawn']
    return engine({'op': 'call', 'target': kismet, 'function': 'ExecuteConsoleCommand', 'args': {'WorldContextObject': pawn, 'Command': command, 'SpecificPlayer': None}})

def main():
    cmd = ' '.join(sys.argv[1:])
    if cmd == 'unit':
        exec_cmd('stat unit'); time.sleep(4)
        exec_cmd('shot showui'); time.sleep(3)
        exec_cmd('stat unit')
        shots = sorted(Path('/mnt/eins0fxE/SteamLibrary/steamapps/compatdata/2584270/pfx/drive_c/users/steamuser/AppData/Local/MortalShell2/Saved/Screenshots/Windows').glob('*.png'), key=lambda p: p.stat().st_mtime)
        print(shots[-1] if shots else 'no screenshot')
    else:
        print(exec_cmd(cmd))

if __name__ == '__main__':
    main()
