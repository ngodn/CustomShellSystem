"""Compare batched pose assignment with sequential actual Blender skinning."""
import json
import os
import sys
import time
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture, AUDIT, WORK, load, pair_set

out = Path(os.environ['CSS_BATCH_POSE_AUDIT_DIR']).resolve()
assert out.parent == WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out / 'report.json').exists()
skin = SkinFixture()
cases = [(f'control-{degree}', AUDIT / f'left-corrected-controller-sweep-v1/control-{degree}.json') for degree in (0, 60)]
cases += [(r['pose'], AUDIT / 'left-graph-anchor-thumb-stress-v1' / r['pose'])
          for r in load(WORK / 'arm-rest-thumb-stress-v1/report.json')['samples']]
rows = []
for name, path in cases:
    doc = load(path)
    started = time.monotonic()
    sequential = skin.evaluate(doc)
    sequential_seconds = time.monotonic() - started
    points = skin.last_positions.copy()
    started = time.monotonic()
    batch = skin.evaluate(doc, batch_pose=True)
    batch_seconds = time.monotonic() - started
    error_cm = float(np.max(np.linalg.norm(points - skin.last_positions, axis=1))) * 100
    same_pairs = pair_set(sequential) == pair_set(batch)
    row = dict(pose=name, vertices=len(points), max_error_cm=error_cm,
               exact_pairs_equal=same_pairs, sequential_seconds=sequential_seconds, batch_seconds=batch_seconds)
    rows.append(row)
    print(json.dumps(row), flush=True)
skin.assert_unchanged()
passed = all(r['max_error_cm'] < .001 and r['exact_pairs_equal'] for r in rows)
(out / 'report.json').write_text(json.dumps(dict(scope=__doc__, passed=passed, samples=rows), indent=2)+'\n')
raise SystemExit(0 if passed else 2)
