"""Pull the Genesis 2 Female bone hierarchy out of VaM's exported Person prefab.

Fitting the figure to Mortal Shell II's skeleton needs to know where the figure's
own joints are. The mesh alone does not say: an elbow is a crease, not a point,
and guessing joint centres from the surface puts them a centimetre or two out,
which is exactly enough to make an arm bend from the wrong place.

VaM carries the real rig. The Person prefab holds one Unity GameObject per DAZ
bone with a Transform beside it, so the hierarchy and the local transforms are
all there; they just have to be walked to get world positions.

The prefab is 171 MB of Unity YAML, so it is scanned once in a single pass,
keeping only what is needed:

    --- !u!1 &4517264            GameObject
    GameObject:
      m_Name: lShldr
    --- !u!4 &4517265            Transform
    Transform:
      m_GameObject: {fileID: 4517264}
      m_LocalRotation: {x: .., y: .., z: .., w: ..}
      m_LocalPosition: {x: .., y: .., z: ..}
      m_Father: {fileID: 4517100}

DAZ bone names are the Genesis 2 set (hip, abdomen, chest, lShldr, rThigh ...).
Output is a JSON list of {name, parent, local, world} in metres, in the same
space as the mesh `extract_daz_mesh.py` writes, so the two line up directly.

    tools/extract_daz_rig.py --prefab .../Person.prefab --out work/.../rig.json
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

import numpy as np

_DOC = re.compile(r'^--- !u!(\d+) &(\d+)')
_NAME = re.compile(r'^  m_Name:\s*(.*?)\s*$')
_GAMEOBJECT = re.compile(r'^  m_GameObject:\s*\{fileID:\s*(-?\d+)\}')
_FATHER = re.compile(r'^  m_Father:\s*\{fileID:\s*(-?\d+)\}')
_NUMBER = r'(-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)'
_POSITION = re.compile(r'^  m_LocalPosition:\s*\{x:\s*' + _NUMBER + r',\s*y:\s*' + _NUMBER
                       + r',\s*z:\s*' + _NUMBER + r'\}')
_ROTATION = re.compile(r'^  m_LocalRotation:\s*\{x:\s*' + _NUMBER + r',\s*y:\s*' + _NUMBER
                       + r',\s*z:\s*' + _NUMBER + r',\s*w:\s*' + _NUMBER + r'\}')
_SCALE = re.compile(r'^  m_LocalScale:\s*\{x:\s*' + _NUMBER + r',\s*y:\s*' + _NUMBER
                    + r',\s*z:\s*' + _NUMBER + r'\}')

# The Genesis 2 skeleton, so the figure's rig can be told apart from the rest of
# the scene. Matching on a handful of unmistakable names is enough; the walk
# then takes whatever hangs off them.
G2_ROOTS = ('hip',)
G2_MARKERS = {'hip', 'abdomen', 'abdomen2', 'chest', 'neck', 'head',
              'lCollar', 'rCollar', 'lShldr', 'rShldr', 'lForeArm', 'rForeArm',
              'lHand', 'rHand', 'lThigh', 'rThigh', 'lShin', 'rShin', 'lFoot', 'rFoot'}


def quaternion_matrix(q) -> np.ndarray:
    x, y, z, w = q
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def scan(path: Path):
    """One pass over the YAML. Returns (names by GameObject id, transforms by id)."""
    names: dict[int, str] = {}
    transforms: dict[int, dict] = {}
    kind = ident = None
    current: dict | None = None

    with path.open('r', errors='replace') as handle:
        for line in handle:
            head = _DOC.match(line)
            if head:
                kind, ident = head.group(1), int(head.group(2))
                # 4 is Transform, 224 is RectTransform; both carry a hierarchy.
                current = {'id': ident} if kind in ('4', '224') else None
                if current is not None:
                    transforms[ident] = current
                continue
            if kind == '1':
                found = _NAME.match(line)
                if found:
                    names[ident] = found.group(1)
                continue
            if current is None:
                continue
            for pattern, key, count in ((_GAMEOBJECT, 'go', 1), (_FATHER, 'father', 1),
                                        (_POSITION, 'position', 3), (_ROTATION, 'rotation', 4),
                                        (_SCALE, 'scale', 3)):
                found = pattern.match(line)
                if found:
                    current[key] = (int(found.group(1)) if count == 1
                                    else [float(found.group(i + 1)) for i in range(count)])
                    break
    return names, transforms


def build(names, transforms) -> list[dict]:
    named = {ident: names.get(entry.get('go', -1), '') for ident, entry in transforms.items()}
    markers = {ident for ident, name in named.items() if name in G2_MARKERS}
    if not markers:
        raise SystemExit('no Genesis 2 bone names found in the prefab')

    # Walk up from a marker to the topmost transform that is still part of the
    # figure, then take everything under it. Picking the marker set's common
    # ancestor rather than a fixed name keeps this working if VaM renames the
    # wrapper GameObjects between versions.
    def ancestry(ident):
        chain, seen = [], set()
        while ident in transforms and ident not in seen:
            seen.add(ident)
            chain.append(ident)
            ident = transforms[ident].get('father', 0)
        return chain

    hips = [i for i, n in named.items() if n in G2_ROOTS]
    root = hips[0] if hips else markers.pop()
    for ident in ancestry(root):
        if named.get(ident) in G2_ROOTS:
            root = ident

    children: dict[int, list[int]] = {}
    for ident, entry in transforms.items():
        children.setdefault(entry.get('father', 0), []).append(ident)

    out: list[dict] = []
    order: dict[int, int] = {}

    def visit(ident, parent_index, parent_matrix, parent_origin):
        entry = transforms[ident]
        rotation = quaternion_matrix(entry.get('rotation', [0, 0, 0, 1]))
        local = np.asarray(entry.get('position', [0, 0, 0]), dtype=np.float64)
        origin = parent_origin + parent_matrix @ local
        matrix = parent_matrix @ rotation
        index = len(out)
        order[ident] = index
        out.append({'name': named.get(ident, f'unnamed_{ident}'), 'parent': parent_index,
                    'local': [round(float(v), 7) for v in local],
                    'world': [round(float(v), 7) for v in origin]})
        for child in sorted(children.get(ident, []), key=lambda c: named.get(c, '')):
            visit(child, index, matrix, origin)

    visit(root, -1, np.eye(3), np.zeros(3))
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--prefab', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args(argv)

    names, transforms = scan(args.prefab)
    print(f'scanned {len(names)} GameObjects, {len(transforms)} transforms')
    bones = build(names, transforms)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(bones, indent=1) + '\n')

    lookup = {b['name']: b for b in bones}
    print(f'{len(bones)} bones written to {args.out}')
    for name in ('hip', 'abdomen', 'chest', 'neck', 'head', 'lShldr', 'lForeArm',
                 'lHand', 'lThigh', 'lShin', 'lFoot'):
        bone = lookup.get(name)
        if bone:
            print(f'  {name:<10} world {np.round(bone["world"], 4)}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
