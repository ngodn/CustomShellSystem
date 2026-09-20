#!/usr/bin/env python3
"""Record a short Mortal Shell Xwayland window clip without desktop focus.

Python 3.14. The caller must have current authorization for live game work.
No desktop capture fallback, audio recording, game launch or input injection.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

from css import ROOT
from css_capture import command


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--window-id', required=True, type=lambda v:int(v,0))
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--seconds', type=int, default=10, choices=range(3,31))
    parser.add_argument('--orbit', action='store_true')
    parser.add_argument('--world', action='store_true', help='Record gameplay with Inventory closed')
    args=parser.parse_args()
    output=args.output.resolve()
    if not output.is_relative_to(ROOT/'work') or output.exists():
        parser.error('Use a new output file inside CustomShellSystem/work')
    identity=subprocess.check_output(['xprop','-id',hex(args.window_id),'WM_NAME','WM_CLASS'],text=True)
    if 'WM_NAME(STRING) = "MortalShell2  "' not in identity or 'steam_app_2584270' not in identity:
        parser.error('Window is not the Mortal Shell game view')
    view=command('inventory_inspect')['css']
    if args.world and args.orbit:
        parser.error('Preview orbit cannot be used for a world recording')
    if args.world and view['active']:
        parser.error('Close CSS before recording the game world')
    if not args.world and not view['active']:
        parser.error('Open the CSS page before recording')
    original=[view[k] for k in ('yaw','zoom','pan','frame')]
    ffmpeg=['ffmpeg','-nostdin','-hide_banner','-loglevel','warning','-f','x11grab',
            '-framerate','30','-draw_mouse','0','-window_id',hex(args.window_id),
            '-i',os.environ['DISPLAY'],'-t',str(args.seconds),'-vf','scale=1920:-2',
            '-c:v','libx264','-preset','ultrafast','-threads','2','-crf','18',
            '-pix_fmt','yuv420p','-movflags','+faststart',str(output)]
    timeline=[]
    started=time.time_ns()
    with output.with_suffix('.log').open('x') as log:
        process=subprocess.Popen(ffmpeg,stdout=log,stderr=subprocess.STDOUT)
        try:
            if args.orbit:
                time.sleep(1)
                duration=(args.seconds-2)/2
                target=[original[0]+160,*original[1:]]
                timeline.append({'ns':time.time_ns(),'view':target,'seconds':duration})
                command('inventory_capture_motion',view=target,seconds=duration)
                time.sleep(duration)
                timeline.append({'ns':time.time_ns(),'view':original,'seconds':duration})
                command('inventory_capture_motion',view=original,seconds=duration)
            process.wait(timeout=args.seconds+15)
            if process.returncode:
                raise RuntimeError('Recording failed; inspect '+str(output.with_suffix('.log')))
        finally:
            if process.poll() is None:
                process.terminate(); process.wait(timeout=10)
            if args.orbit:
                current=command('inventory_inspect')['css']
                command('inventory_test_motion',movement=[v-current[k] for v,k in zip(original,('yaw','zoom','pan','frame'))])
    details=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries',
        'stream=width,height,nb_frames,duration,avg_frame_rate','-of','json',str(output)],text=True))
    details.update(window=identity,started_ns=started,ended_ns=time.time_ns(),timeline=timeline,
                   scope=('Game window only, in-world observation. No injected input.' if args.world else
                          'Game window only. Camera orbit is a controlled preview movement, not locomotion acceptance.'))
    output.with_suffix('.json').write_text(json.dumps(details,indent=2)+'\n')
    print(output)


if __name__=='__main__':
    main()
