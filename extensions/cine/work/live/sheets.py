"""Contact sheets of the last run's screenshots (../export-live/reel-t*.jpg -> sheet-N.jpg). Python 3.14.

  sheets.py [from seconds] [to seconds]

Tiles keep the game window's own shape, whatever it is.
"""
import glob
import subprocess
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'export-live'
files = sorted(glob.glob(str(OUT / 'reel-t*.jpg')))
lo, hi = (float(sys.argv[1]), float(sys.argv[2])) if len(sys.argv) > 2 else (0, 1e9)
files = [f for f in files if lo <= float(Path(f).stem.split('-t')[1]) <= hi]
for old in OUT.glob('sheet-*.jpg'):
    old.unlink()
if files:
    w, h = map(int, subprocess.check_output(['ffprobe', '-v', 'error', '-show_entries', 'stream=width,height', '-of', 'csv=p=0', files[0]]).decode().strip().split(','))
    wide = w >= h
    th = 360 if wide else 520
    tw = round(th * w / h / 2) * 2
    cols = max(1, 2400 // tw)
    rows = max(1, (1100 if wide else 1050) // th)
    for n in range(0, len(files), cols * rows):
        chunk, ins, filt = files[n:n + cols * rows], [], []
        for i, f in enumerate(chunk):
            ins += ['-i', f]
            filt.append(f"[{i}:v]scale={tw}:{th},drawtext=text='{Path(f).stem.split('-t')[1]}':x=5:y=5:fontsize=22:fontcolor=yellow:box=1:boxcolor=black@0.6[v{i}]")
        layout = '|'.join(f'{(i % cols) * tw}_{(i // cols) * th}' for i in range(len(chunk)))
        filt.append(''.join(f'[v{i}]' for i in range(len(chunk))) + (f'xstack=inputs={len(chunk)}:layout={layout}:fill=black[o]' if len(chunk) > 1 else 'copy[o]'))
        subprocess.run(['ffmpeg', '-v', 'error', '-y', *ins, '-filter_complex', ';'.join(filt), '-map', '[o]', '-q:v', '3', str(OUT / f'sheet-{n // (cols * rows)}.jpg')], check=True)
print(len(files), 'frames')
