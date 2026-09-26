"""Compare completed native tail trials without treating coordinate checks as fit acceptance."""
import argparse
import hashlib
import json
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--work',type=Path,default=Path(__file__).resolve().parents[2]/'work/eve26')
p.add_argument('--trials',nargs='+',required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
assert not a.output.exists()
rows=[]
for name in a.trials:
    motion_path=a.work/f'planet-tail-{name}.json'
    verified=json.loads((a.work/f'planet-tail-{name}-verified.json').read_text())
    assert verified['source_sha256']==hashlib.sha256(motion_path.read_bytes()).hexdigest()
    motion=json.loads(motion_path.read_text())
    cases=verified['cases'];worst=max(cases,key=lambda r:r['edge_ratio_max'])
    rows.append({'trial':name,'coordinate_check_passed':verified['passed'],'frames':len(cases),
        'worst_edge_ratio':worst['edge_ratio_max'],'worst_frame':worst['frame'],
        'first_edge_ratio':cases[0]['edge_ratio_max'],
        'fixed_max_cm':max(c['fixed_max_cm'] for c in cases),
        'mean_simulation_ms':sum(f['simulation_ms'] for f in motion['frames'])/len(motion['frames']),
        'iterations':sorted({f['iterations'] for f in motion['frames']})})
a.output.write_text(json.dumps({'scope':'Diagnostic trials, not cloth quality or game performance acceptance','trials':rows},indent=2)+'\n')
print(json.dumps(rows,indent=2))
