"""Bounded read-only graph/pose observation, Python 3.14. No game input."""
import argparse
import json
from pathlib import Path
import sys
import time

CSS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CSS / 'tools'))
from css import processes
from css_live_snapshot import Probe

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--samples', type=int, default=15)
args = parser.parse_args()
out = args.output.resolve()
assert out.is_relative_to(CSS / 'work') and not out.exists()
assert 1 <= args.samples <= 30
assert len(processes()) == 1
out.mkdir()
rows = []
with (out / 'requests.jsonl').open('x') as log:
    p = Probe(log)
    player = p.send('player')
    assert player.get('pawn')
    mesh = p.send('get', target=player['pawn'], property='Mesh')
    p.send('describe', target=mesh, function='GetAnimInstance')
    anim = p.call(mesh, 'GetAnimInstance')
    p.send('describe', target=mesh, function='SnapshotPose')
    p.send('describe', target=anim, function='GetCurrentActiveMontage')
    skeleton = p.send('get', target=anim, property='CurrentSkeleton')
    overlay = p.send('get', target=anim, property='AdditiveBodyAdjustmentPose')
    (out / 'context.json').write_text(json.dumps(dict(player=player, mesh=mesh, anim=anim, overlay=overlay, skeleton=skeleton), indent=2)+'\n')
    try:
        for i in range(args.samples):
            assert p.send('player') == player, 'Player changed; stop using old handles'
            started = time.time_ns()
            before = p.send('get', target=anim, property='AdditiveBodyAdjustmentAlpha')
            pose = p.send('call', target=mesh, function='SnapshotPose', args={})
            captured = time.time_ns()
            montage = p.call(anim, 'GetCurrentActiveMontage')
            after = p.send('get', target=anim, property='AdditiveBodyAdjustmentAlpha')
            doc = dict(pose=pose, started_ns=started, captured_ns=captured,
                       ended_ns=time.time_ns(), alpha_before=before, alpha_after=after,
                       montage=montage)
            (out / f'pose-{i:02d}.json').write_text(json.dumps(doc, indent=2)+'\n')
            rows.append({k:v for k,v in doc.items() if k!='pose'} | dict(pose=f'pose-{i:02d}.json'))
            print(json.dumps(dict(sample=i, alpha_before=before, alpha_after=after, montage=montage)), flush=True)
            time.sleep(.5)
    finally:
        (out / 'report.json').write_text(json.dumps(dict(scope='Sequential graph reads bracketing an atomic local-pose snapshot. Matching alpha endpoints do not prove no intermediate change. Montage is read after the pose. No movement input or mutations.', samples=rows), indent=2)+'\n')
