#!/usr/bin/env python3
"""Read-only attachment regression check against the running developer core.

Equip Tiel or Eredrim with the outfit under test before running this command.
No gameplay input, appearance preferences or component properties are changed.
Fail if a visible shell accessory is attached to a missing bone, or if the
requested accessory is absent (an absent test subject is not a passing test).
"""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from css import ROOT
from cssx_cheat_check import request


def call(target, name, **args):
    value = request(dict(op='call', target=target, function=name, args=args), True)
    return next(iter(value.values())) if len(value) == 1 else value


def inspect():
    player = request({'op': 'player'}, True)
    mesh = request(dict(op='get', target=player['pawn'], property='Mesh'), True)
    rows = []
    for child in call(mesh, 'GetChildrenComponents', bIncludeAllDescendants=True):
        socket = call(child, 'GetAttachSocketName')
        if not any(part in str(socket).lower() for part in ('tiel', 'diapa')):
            continue
        parent = call(child, 'GetAttachParent')
        bone = call(parent, 'GetSocketBoneName', InSocketName=socket)
        index = call(parent, 'GetBoneIndex', BoneName=bone)
        rows.append(dict(child=child, parent=parent, socket=socket, bone=bone,
                         bone_index=index,
                         socket_transform=call(parent, 'GetSocketTransform',
                                               InSocketName=socket, TransformSpace=2),
                         hidden=request(dict(op='get', target=child,
                                             property='bHiddenInGame'), True)))
    return dict(shell=player['shell'], mesh=mesh, attachments=rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expect', required=True, choices=['tiel', 'diapason'])
    parser.add_argument('--label', default='current')
    args = parser.parse_args()
    if not args.label.replace('-', '').replace('_', '').isalnum():
        parser.error('label must contain only letters, numbers, hyphens or underscores')
    evidence = inspect()
    needle = 'tiel' if args.expect == 'tiel' else 'diapa'
    subjects = [row for row in evidence['attachments']
                if needle in str(row['socket']).lower() and not row['hidden']]
    evidence['pass'] = bool(subjects) and all(row['bone_index'] >= 0 for row in subjects)
    output = ROOT / 'work/support/jayluk3-2026-09-16' / (args.label + '.json')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence, indent=2))
    assert subjects, 'No visible matching accessory; this does not exercise the reported bug'
    assert evidence['pass'], 'Accessory attachment socket references a missing bone'


if __name__ == '__main__':
    main()
