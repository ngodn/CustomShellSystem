"""Check the cooked graph fields required by WalkOverride's idle activation."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('audit', type=Path)
args = parser.parse_args()
failures = []
for name in ('base', 'bikini', 'knit'):
    graph = json.loads((args.audit/name/'source-mesh.json').read_text())
    fields = {p['Name']:p for p in graph['ChildProperties']}
    for field, kind, size in [('CSSIdleEnabled','BoolProperty',1), ('CSSIdleSequence','ObjectProperty',8)]:
        p = fields.get(field, {})
        if p.get('Type') != kind or p.get('ElementSize') != size:
            failures.append(f'{name}: missing or incompatible {field}')
    if fields.get('CSSIdleSequence',{}).get('PropertyClass',{}).get('ObjectName') != "Class'AnimSequence'":
        failures.append(f'{name}: idle sequence must reference AnimSequence')
if failures:
    raise SystemExit('\n'.join(failures))
print('All three cooked graph classes satisfy the idle activation contract.')
