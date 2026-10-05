"""UE 5.6.1: check saved source and target sampling clocks without modifying assets."""
import hashlib
import json
import os
from pathlib import Path
import re

import unreal


ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
config = json.loads((WORK/'batch-config.json').read_text())
revision = config['revision']
assert re.fullmatch(r'[A-Za-z0-9]{1,8}', revision)
batch = json.loads((WORK/'batch.json').read_text())
assert 1 <= len(batch) <= 64
results = []
for label, filename in batch:
    assert re.fullmatch(r'[A-Za-z][A-Za-z0-9]{0,15}', label)
    assert re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,80}', filename)
    data = json.loads((WORK/(filename+'.json')).read_text())
    for prefix in ('AN', 'RT'):
        path = f'/Game/CSS/AnimLab/{prefix}_{revision}_{label}'
        animation = unreal.load_asset(path)
        assert animation, path
        keys = unreal.AnimationLibrary.get_num_keys(animation)
        duration = unreal.AnimationLibrary.get_sequence_length(animation)
        frame_time = unreal.AnimationLibrary.get_time_at_frame(animation, 1)
        assert keys == data['frames'], (path, keys, data['frames'])
        assert abs(duration-data['duration']) <= max(1e-6, data['duration']*1.2e-7), path
        assert abs(frame_time-1/data['fps']) < 1e-6, (path, frame_time, data['fps'])
        results.append(dict(asset=path, sampled_keys=keys, duration=duration,
                            first_interval_seconds=frame_time, expected_hz=data['fps']))
protected = json.loads((WORK/'protected.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
with (WORK/'sampling-result.json').open('x') as stream:
    json.dump(dict(passed=True, results=results,
                   scope='Saved editor assets sampled-key counts and timebase. Not cooked validation.'),
              stream, indent=2)
    stream.write('\n')
