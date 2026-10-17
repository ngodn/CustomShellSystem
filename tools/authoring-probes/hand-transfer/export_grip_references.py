"""Update an existing hand-support JSON catalog with audited grip references."""
import argparse
import json
import math
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('references', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
variants = {'peony':'peony', 'black-pearl':'black_pearl', 'unholy-eve':'unholy_eve',
            'unholy-genessa':'unholy_genessa', 'commander-white':'commander'}
weapons = {'ClockworkScythe':'scythe', 'HadernsSword':'hadern', 'BlackNeedle':'needle',
           'HeavyHammer':'hammer', 'BattleAxe':'battle_axe'}
references = json.loads(args.references.read_text())
identities = {(r['family'],r['weapon']) for r in references}
if len(references) != 25 or identities != {(v,w) for v in variants for w in weapons}:
    raise ValueError('Expected all five variants and five shoulder weapons exactly once')
catalog = json.loads(args.output.read_text())
entries = []
for ref in references:
    q = [ref['transform']['Rotation'][k] for k in 'XYZW']
    p = [ref['transform']['Translation'][k] for k in 'XYZ']
    norm = math.sqrt(sum(v*v for v in q))
    if not all(math.isfinite(v) for v in q+p) or abs(norm-1) > 1e-5:
        raise ValueError('Invalid measured grip transform')
    entries.append(dict(id=variants[ref['family']], weapon=weapons[ref['weapon']],
                        rotation=[v/norm for v in q], translation=p))
catalog['grips'] = entries
args.output.write_text(json.dumps(catalog, indent=2) + '\n')
