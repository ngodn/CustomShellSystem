"""Record the game window exactly as it is on screen while a CINE take rolls. Python 3.14.

  record.py [PRESET]

Records the window's own rectangle (no fullscreen, no resize, no trim) with the desktop's
sound into ~/Videos, one frame for each frame the game shows (up to 60 a second), so no
frame is doubled or dropped by the recorder. PRESET defaults to the one selected in CINE. Nothing about how
the game renders is changed: not its frame rate, not its resolution, not CINE's Picture setting.
Rolls the take, stops the recorder two seconds after it ends, and checks her look is as it was.
"""
import json
import signal
import subprocess
import sys
import time
from pathlib import Path

import reel as r


def window():
    clients = json.loads(subprocess.check_output(['hyprctl', '-j', 'clients']))
    monitors = json.loads(subprocess.check_output(['hyprctl', '-j', 'monitors']))
    game = [c for c in clients if c['class'] == 'steam_app_2584270' and c['title'].strip() == 'MortalShell2']
    if not game:
        raise SystemExit('The game window was not found.')
    shown = game[0]['workspace']['name'] in {m['activeWorkspace']['name'] for m in monitors}
    return game[0], shown


def main():
    game, shown = window()
    back = None
    if not shown:
        # A recorder sees only what is displayed: show the game's workspace on its own monitor
        # (the window keeps its size and place) and put the screen back afterwards.
        monitors = json.loads(subprocess.check_output(['hyprctl', '-j', 'monitors']))
        active = json.loads(subprocess.check_output(['hyprctl', '-j', 'activewindow']) or b'{}')
        on = next((m for m in monitors if m['id'] == game['monitor']), None)
        back = (on['activeWorkspace']['name'] if on else None, active.get('address'))
        # This Hyprland takes dispatchers as Lua: hl.dsp.focus({ workspace = "3" }).
        if subprocess.run(['hyprctl', 'dispatch', f'hl.dsp.focus({{ workspace = "{game["workspace"]["name"]}" }})'], capture_output=True).returncode != 0:
            raise SystemExit(f"The game window is on workspace {game['workspace']['name']}, which is not on screen, and this Hyprland refused the switch. Show that workspace and run this again.")
        time.sleep(1.5)
        before = (game['at'], game['size'])
        game, shown = window()
        if not shown or (game['at'], game['size']) != before:
            raise SystemExit(f"Could not show the game window unchanged (now at {game['at']} size {game['size']}).")
    (x, y), (w, h) = game['at'], game['size']
    w -= w % 2; h -= h % 2   # the encoder needs even sizes: at most one pixel off the right or bottom edge
    c, error = r.controls()
    preset = sys.argv[1] if len(sys.argv) > 1 else c['preset']['value']
    status = c['status']['value']
    _, error = r.controls()
    # The status line keeps its last report: a take that was cancelled still reads "Rolling."
    # with the cancellation in the error line.
    ended = any(word in (error or '') for word in ('cancel', 'stopped', 'finished', 'Could not'))
    if not ended and any(word in status for word in ('Rolling', 'Preparing', 'Building', 'Checking', 'Get ready', 'Closing')):
        raise SystemExit('A take is already running.')
    own = r.css({'action': 'describe'})
    out = Path.home() / 'Videos' / f"{preset}-{time.strftime('%Y-%m-%d_%H-%M-%S')}.mp4"
    recorder = subprocess.Popen(['gpu-screen-recorder', '-w', 'region', '-region', f'{w}x{h}+{x}+{y}', '-f', '60', '-fm', 'content', '-k', 'h264', '-q', 'ultra',
                                 '-cursor', 'no', '-a', 'default_output', '-o', str(out)], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        time.sleep(2.0)
        if recorder.poll() is not None:
            raise SystemExit('The recorder did not start: ' + recorder.stderr.read().decode(errors='replace')[-400:])
        print(f'recording {w}x{h} at {x},{y} -> {out}')
        r.event('preset', preset); r.event('breakdown', 'preset'); r.event('countdown', 0)
        if r.controls()[0]['enter'].get('enabled', True):
            r.event('enter'); time.sleep(0.6)
        r.event('take')
        began, last, rolled = time.monotonic(), '', False
        rolled_at, samples = None, []
        while time.monotonic() - began < 200:
            time.sleep(1.0)   # one question a second: the game is left alone while it is filmed
            c, error = r.controls()
            status = c['status']['value']
            if status != last:
                print(f'{time.monotonic() - began:6.1f}s  {status}' + (f'  ERROR {error}' if error else ''))
                last = status
            if not rolled and 'Rolling' in status:
                rolled_at = time.monotonic()
            rolled = rolled or 'Rolling' in status
            # The frame rate of the last second, by take time: which shots cost frames.
            if rolled and rolled_at is not None:
                try:
                    b = r.req({'op': 'frame.stats', 'seconds': 1}).get('engine', {})
                except Exception:
                    b = {}
                if b.get('count'):
                    samples.append((round(time.monotonic() - rolled_at, 1), round(b.get('hz', 0), 1), round(b.get('p99_ms', 0), 1)))
            if rolled and any(word in status for word in ('finished', 'cancel', 'stopped', 'Could not')):
                break
            if error and not rolled:
                break
        time.sleep(2.0)
    finally:
        recorder.send_signal(signal.SIGINT)
        try:
            recorder.wait(timeout=20)
        except subprocess.TimeoutExpired:
            recorder.kill()
    now = own
    for _ in range(24):
        time.sleep(0.5)
        now = r.css({'action': 'describe'})
        if r.same_look(now, own):
            break
    print('her look restored:', r.same_look(now, own))
    if samples:
        print('fps by take second (t, fps, slowest 1% ms):')
        print('  ' + '  '.join(f'{t:g}:{hz:g}' for t, hz, _ in samples))
        low = [s for s in samples if s[1] < 45]
        print(f'below 45 fps: {len(low)} of {len(samples)} seconds' + (f', lowest {min(low, key=lambda s: s[1])}' if low else ''))
    if back:
        if back[0]:
            subprocess.run(['hyprctl', 'dispatch', f'hl.dsp.focus({{ workspace = "{back[0]}" }})'], capture_output=True)
    if out.exists():
        info = subprocess.check_output(['ffprobe', '-v', 'error', '-show_entries', 'format=duration,size:stream=codec_name,width,height,r_frame_rate', '-of', 'compact', str(out)]).decode()
        print(info.strip())


if __name__ == '__main__':
    main()
