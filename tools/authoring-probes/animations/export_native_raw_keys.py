"""Export unretargeted native pose keys for independent cooked-motion checks."""
import hashlib
import json
import os
from pathlib import Path

import unreal

work = Path(os.environ['CSS_NATIVE_LIBRARY_WORK']).resolve(strict=True)
plan = json.loads(Path(os.environ['CSS_NATIVE_LIBRARY_PLAN']).read_text())
reports = [Path(p) for p in json.loads(os.environ['CSS_NATIVE_LIBRARY_REPORTS'])]
rows = [r for p in reports for r in json.loads(p.read_text())['clips']]
selected = json.loads(os.environ.get('CSS_NATIVE_EXPORT_IDS', '[]'))
if selected:
    rows = [r for r in rows if r['id'] in selected]
    if {r['id'] for r in rows} != set(selected):
        raise ValueError('Requested raw-export identities missing')
clock = os.environ.get('CSS_NATIVE_EXPORT_CLOCK', 'nominal')
if clock not in ('nominal', 'acl_sample_index'):
    raise ValueError('Unknown sample clock')
results = []
for row in rows:
    source = unreal.load_asset(row['package'])
    if not source or json.loads(unreal.CSSRigAuditLibrary.snapshot_animation(source)) != json.loads(Path(row['snapshot']).read_text()):
        raise ValueError('Authored animation changed')
    label = hashlib.sha256(row['id'].encode()).hexdigest()[:12]
    output = work / (label+'.bin')
    raw = unreal.CSSRigAuditLibrary.export_raw_pose_keys(source, str(output), clock == 'acl_sample_index')
    if raw.startswith('ERROR:'):
        raise RuntimeError(raw)
    data = json.loads(raw)
    if (data['frames'] != row['keys'] or len(data['names']) != 1208 or
            data['sample_clock'] != clock or data['bytes'] != output.stat().st_size):
        raise ValueError('Incomplete native pose export')
    with output.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    data.update(id=row['id'], package=row['package'], file=str(output), sha256=digest)
    metadata = work / (label+'.json')
    metadata.write_text(json.dumps(data, indent=2)+'\n')
    results.append(dict(id=row['id'], metadata=str(metadata)))
    (work/'progress.json').write_text(json.dumps(dict(completed=len(results), total=len(rows), id=row['id']))+'\n')
for path, expected in plan['protected'].items():
    with Path(path).open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != expected:
            raise ValueError('Protected source changed')
(work/'result.json').write_text(json.dumps(dict(passed=True, clips=results), indent=2)+'\n')
