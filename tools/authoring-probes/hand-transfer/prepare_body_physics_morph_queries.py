"""Add audited morph-extreme points and off-body controls to engine query fixtures."""
import argparse
import hashlib
import json
from pathlib import Path

WORK = Path(__file__).resolve().parents[4]/'CustomShellSystem/work/grip-grounding-v1'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--fit', type=Path, required=True)
parser.add_argument('--audit', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
fit, audit_path, output = [p.resolve() for p in (args.fit, args.audit, args.output)]
assert all(p.is_relative_to(WORK.resolve()) for p in (fit, audit_path, output))
assert not output.exists()
audit = json.loads(audit_path.read_text())
candidate = fit/'candidate.json'
assert Path(audit['asset']) == candidate
assert hashlib.sha256(candidate.read_bytes()).hexdigest() == audit['protected_hashes'][str(candidate)]
assert audit['all_corners_enclosed'] and len(audit['cases']) == 64
rows = json.loads((fit/'queries.json').read_text())
for case in audit['cases']:
    point = case['worst_point']
    start = [point[0], point[1]-100, point[2]]
    end = [point[0], point[1]+100, point[2]]
    for miss in (False, True):
        offset = 1000 if miss else 0
        rows.append(dict(id=f"morph-corner/{case['corner']}"+('/miss' if miss else ''),
            start=[x+offset for x in start], end=[x+offset for x in end], expected=not miss,
            probe_point=[x+offset for x in point], probe_bone=case['worst_bone']))
assert len(rows) == 398
output.write_text(json.dumps(rows, indent=2)+'\n')
