"""Update an existing hand-support JSON catalog with measured finger targets."""
import argparse
import json
import math
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('profiles',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
families={'peony':'eve','unholy-eve':'unholy','commander-white':'commander'}
weapons={'ClockworkScythe':'scythe','HadernsSword':'hadern','BlackNeedle':'needle','HeavyHammer':'hammer','BattleAxe':'battle_axe'}
catalog=json.loads(args.output.read_text())
bones=catalog['target_bones']
profiles=json.loads(args.profiles.read_text())['profiles']
identities={(p['family'],p['weapon']) for p in profiles}
if len(profiles)!=15 or identities!={(f,w) for f in families for w in weapons}:raise ValueError('Expected every audited family and shoulder weapon exactly once')
entries=[]
for profile in profiles:
 rotations=profile['phases']['standing']['rotations']
 if set(rotations)!=set(bones):raise ValueError('Unexpected carry bone set')
 targets=[]
 for bone in bones:
  q=[float(rotations[bone][k]) for k in 'XYZW'];norm=math.sqrt(sum(v*v for v in q))
  if not math.isfinite(norm) or abs(norm-1)>1e-5:raise ValueError('Invalid measured quaternion: '+bone)
  targets.append([v/norm for v in q])
 entries.append(dict(id=families[profile['family']],weapon=weapons[profile['weapon']],standing=targets))
catalog['targets']=entries
args.output.write_text(json.dumps(catalog,indent=2)+'\n')
