"""Pull VaM's built-in morphs out of an exported MorphBank prefab.

VaM ships ~1,457 morphs inside `FemaleCharacterMorphBank` rather than as loose
`.var` packages: the character heads (Victoria 6, Aiko 6, Stephanie 6, Brooke,
Carmen), the full facial rig (30-odd brow controls, eyes, nose, lips, cheeks,
jaw, chin), areola and genital shaping, and body proportions. None of it is in
the Hub packages, so without this the face can only be posed, not built.

They live in the Unity YAML that AssetRipper writes when it is given
`VaM_Data/Managed` alongside the `f_mb` bundle:

    _morphs:
    - morphName: SR Tracey Body
      displayName: SR Tracey Body
      group: Full Body/Real World
      numDeltas: 14584
      deltas:
      - vertex: 0
        delta: {x: 0.0136, y: 0.0083, z: -0.0152}

The delta semantics match `.vmb` exactly (vertex index into the 21556-vertex DAZ
base, offset in metres), so this writes real `.vmb`/`.vmi` pairs rather than a
private format. Everything already built on `vam_morph` then works on them
unchanged, including the vertex-order check.

    tools/extract_morphbank.py --prefab .../FemaleCharacterMorphBank.prefab \\
        --out work/v1.0.0-body/morphbank --match 'Victoria|Aiko|Brow|Lip'
"""
from __future__ import annotations

import argparse
import json
import re
import struct
from pathlib import Path

_FIELD = re.compile(r'^\s{4}(\w+):\s*(.*)$')
_ENTRY = re.compile(r'^\s{2}-\s+(\w+):\s*(.*)$')
_VERTEX = re.compile(r'^\s*-\s*vertex:\s*(\d+)\s*$')
_NUMBER = r'(-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)'
_DELTA = re.compile(r'^\s*delta:\s*\{x:\s*' + _NUMBER + r',\s*y:\s*' + _NUMBER
                    + r',\s*z:\s*' + _NUMBER + r'\}')

# Characters that are not safe in a filename, replaced rather than dropped so two
# morphs cannot collapse onto the same name.
_UNSAFE = re.compile(r'[^A-Za-z0-9 _.()&+-]')


class BankMorph:
    def __init__(self):
        self.fields: dict[str, str] = {}
        self.indices: list[int] = []
        self.deltas: list[tuple[float, float, float]] = []

    @property
    def name(self) -> str:
        return (self.fields.get('displayName') or self.fields.get('morphName') or '').strip()

    @property
    def declared(self) -> int:
        try:
            return int(self.fields.get('numDeltas', -1))
        except ValueError:
            return -1

    def ok(self) -> bool:
        return bool(self.name) and self.declared == len(self.indices) and self.declared > 0


def stream(path: Path, wanted: re.Pattern | None = None, skip_deltas: bool = False):
    """Yield each morph in the bank. Streamed: the prefab runs to hundreds of MB."""
    current: BankMorph | None = None
    reading = False
    pending: int | None = None
    collecting = True

    with path.open('r', errors='replace') as handle:
        for line in handle:
            entry = _ENTRY.match(line)
            if entry and entry.group(1) == 'visible':
                if current is not None and current.ok():
                    yield current
                current = BankMorph()
                reading = False
                pending = None
                collecting = True
                continue
            if current is None:
                continue

            field = _FIELD.match(line)
            if field:
                key, value = field.group(1), field.group(2).strip()
                if key == 'deltas':
                    reading = True
                    # Decide once, on a complete header, whether to keep the body.
                    collecting = not skip_deltas and (
                        wanted is None or bool(wanted.search(current.name)))
                else:
                    reading = False
                    current.fields[key] = value
                continue

            if not reading or not collecting:
                continue
            v = _VERTEX.match(line)
            if v:
                pending = int(v.group(1))
                continue
            d = _DELTA.match(line)
            if d and pending is not None:
                current.indices.append(pending)
                current.deltas.append((float(d.group(1)), float(d.group(2)), float(d.group(3))))
                pending = None

    if current is not None and current.ok():
        yield current


def write_vmb(path: Path, indices, deltas):
    blob = struct.pack('<i', len(indices))
    for index, (dx, dy, dz) in zip(indices, deltas):
        blob += struct.pack('<ifff', index, dx, dy, dz)
    path.write_bytes(blob)


def write_vmi(path: Path, morph: BankMorph):
    def number(key, fallback):
        try:
            return float(morph.fields.get(key, fallback))
        except ValueError:
            return float(fallback)
    path.write_text(json.dumps({
        'id': morph.fields.get('morphName', morph.name),
        'displayName': morph.name,
        'group': morph.fields.get('group', ''),
        'region': morph.fields.get('region', ''),
        'min': str(number('min', 0)),
        'max': str(number('max', 1)),
        'numDeltas': str(len(morph.indices)),
        'isPoseControl': morph.fields.get('isPoseControl', '0'),
        'formulas': [],
    }, indent=3))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--prefab', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--match', help='regex on the display name; omit for all')
    parser.add_argument('--list', action='store_true', help='only list what is there')
    args = parser.parse_args(argv)

    wanted = re.compile(args.match, re.I) if args.match else None
    if args.list:
        rows = []
        for morph in stream(args.prefab, wanted, skip_deltas=True):
            if wanted is None or wanted.search(morph.name):
                rows.append((morph.name, morph.declared, morph.fields.get('group', '')))
        for name, count, group in sorted(rows):
            print(f'  {name[:44]:<46} {count:>7} deltas   {group[:34]}')
        print(f'{len(rows)} morphs')
        return 0

    args.out.mkdir(parents=True, exist_ok=True)
    written = 0
    index = []
    for morph in stream(args.prefab, wanted):
        if wanted is not None and not wanted.search(morph.name):
            continue
        if not morph.indices:
            continue
        safe = _UNSAFE.sub('_', morph.name).strip()[:80]
        write_vmb(args.out / f'{safe}.vmb', morph.indices, morph.deltas)
        write_vmi(args.out / f'{safe}.vmi', morph)
        largest = max((x * x + y * y + z * z) ** .5 for x, y, z in morph.deltas)
        index.append({'name': morph.name, 'file': safe, 'deltas': len(morph.indices),
                      'max_index': max(morph.indices), 'largest_cm': largest * 100,
                      'group': morph.fields.get('group', '')})
        written += 1
        print(f'  {morph.name[:44]:<46} {len(morph.indices):>7} deltas  max {max(morph.indices):>6}'
              f'  {largest * 100:>6.2f} cm')
    (args.out / 'index.json').write_text(json.dumps(index, indent=2))
    print(f'{written} morphs written to {args.out}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
