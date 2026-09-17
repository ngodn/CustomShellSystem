"""Read Virt-A-Mate morph files (.vmb/.vmi) as vertex deltas.

VaM ships a morph as two files next to each other:

  Name.vmi   JSON: display name, group, min/max, numDeltas, and the `formulas`
             list that moves joint centres when the shape changes.
  Name.vmb   binary: int32 count, then count * (int32 vertexIndex, float dx,
             float dy, float dz), little endian, 16 bytes per record.

The deltas are indexed against the Genesis 2 Female base mesh (21556 vertices),
so a morph on its own is a diff with no original. It needs the base mesh to mean
anything. Morphs whose largest index runs past the base count target G2F plus a
geograft (the genitalia graft) rather than the plain base, and `targets_base()`
reports which kind a file is.

Deltas are in DAZ/VaM metres. `to_centimetres()` converts.

Validated against the 49 morph files in reference/CSS-v1.0.0-plan: all 49 parse
with no size mismatch, and every base-topology morph's largest index lands under
21556.
"""
from __future__ import annotations

import json
import struct
from dataclasses import dataclass
from pathlib import Path

# Genesis 2 Female base resolution. A morph indexing past this needs the grafted
# mesh, not the base one.
G2F_BASE_VERTICES = 21556

_RECORD = struct.Struct('<ifff')


@dataclass(frozen=True)
class Formula:
    """One joint correction: `target`'s `target_type` shifts by `multiplier` at weight 1."""
    target_type: str
    target: str
    multiplier: float


# The genitalia geograft is its own small mesh. A morph on it indexes 0..1451,
# which is also a legal range on the 21556-vertex base, so the index alone cannot
# tell the two apart. VaM files them under Morphs/female_genitalia/, and that is
# the only reliable signal, so it is carried on the morph.
GRAFT_VERTICES = 1452
_GRAFT_HINT = 'female_genitalia'


@dataclass
class Morph:
    name: str
    display_name: str
    indices: list[int]
    deltas: list[tuple[float, float, float]]
    minimum: float = 0.0
    maximum: float = 1.0
    group: str = ''
    formulas: list[Formula] = None
    source: str = ''                     # path it came from, for graft detection

    def __post_init__(self):
        if self.formulas is None:
            self.formulas = []
        if len(self.indices) != len(self.deltas):
            raise ValueError(f'{self.name}: {len(self.indices)} indices against {len(self.deltas)} deltas')

    def __len__(self) -> int:
        return len(self.indices)

    @property
    def max_index(self) -> int:
        return max(self.indices) if self.indices else -1

    def targets_graft(self) -> bool:
        """True when this morph belongs to the genitalia geograft rather than the body.

        Decided by where the file lives, not by index range: a graft morph indexes
        0..1451, which is a perfectly legal range on the body too, so an index
        test alone reports graft morphs as body morphs and would apply them to
        the wrong mesh.
        """
        return _GRAFT_HINT in self.source.replace('\\', '/').lower()

    def targets_base(self) -> bool:
        """True when this applies to the plain G2F base mesh."""
        if self.targets_graft():
            return False
        return self.max_index < G2F_BASE_VERTICES

    def largest_move(self) -> float:
        """Longest single delta, in the file's own units."""
        return max((dx * dx + dy * dy + dz * dz) ** .5 for dx, dy, dz in self.deltas) if self.deltas else 0.0

    def to_centimetres(self) -> 'Morph':
        """VaM stores metres; the CSS mesh schema is centimetres."""
        scaled = [(dx * 100.0, dy * 100.0, dz * 100.0) for dx, dy, dz in self.deltas]
        return Morph(self.name, self.display_name, list(self.indices), scaled,
                     self.minimum, self.maximum, self.group, list(self.formulas),
                     self.source)

    def apply(self, points, weight: float = 1.0):
        """Return `points` with this morph applied at `weight`.

        `points` is any sequence of (x, y, z) long enough to cover `max_index`.
        Units must match the morph's own, so convert one side or the other first.
        """
        if self.max_index >= len(points):
            raise ValueError(
                f'{self.name}: needs {self.max_index + 1} points, got {len(points)}. '
                f'{"Base mesh is too small" if self.targets_base() else "This morph needs the grafted mesh"}')
        out = [tuple(p) for p in points]
        for i, (dx, dy, dz) in zip(self.indices, self.deltas):
            x, y, z = out[i]
            out[i] = (x + dx * weight, y + dy * weight, z + dz * weight)
        return out


def read_vmb(path) -> tuple[list[int], list[tuple[float, float, float]]]:
    """Parse the binary delta table. Raises if the declared count and file size disagree."""
    raw = Path(path).read_bytes()
    if len(raw) < 4:
        raise ValueError(f'{path}: too short to hold a count')
    count = struct.unpack_from('<i', raw, 0)[0]
    expected = 4 + count * _RECORD.size
    if count < 0 or expected != len(raw):
        raise ValueError(f'{path}: declares {count} deltas, which needs {expected} bytes, file is {len(raw)}')
    indices, deltas = [], []
    for i in range(count):
        idx, dx, dy, dz = _RECORD.unpack_from(raw, 4 + i * _RECORD.size)
        indices.append(idx)
        deltas.append((dx, dy, dz))
    return indices, deltas


def read_vmi(path) -> dict:
    """Parse the JSON sidecar. VaM writes numbers as strings, so coerce on read."""
    meta = json.loads(Path(path).read_text(encoding='utf-8-sig'))
    formulas = [Formula(f.get('targetType', ''), f.get('target', ''), float(f.get('multiplier', 0)))
                for f in meta.get('formulas', [])]
    return {
        'display_name': meta.get('displayName', ''),
        'group': meta.get('group', ''),
        'minimum': float(meta.get('min', 0)),
        'maximum': float(meta.get('max', 1)),
        'declared_deltas': int(meta.get('numDeltas', -1)),
        'formulas': formulas,
    }


def load(vmb_path) -> Morph:
    """Load a morph from its .vmb, reading the .vmi beside it when present."""
    vmb = Path(vmb_path)
    indices, deltas = read_vmb(vmb)
    meta = {'display_name': vmb.stem, 'group': '', 'minimum': 0.0, 'maximum': 1.0,
            'declared_deltas': -1, 'formulas': []}
    vmi = vmb.with_suffix('.vmi')
    if vmi.exists():
        meta = read_vmi(vmi)
        declared = meta['declared_deltas']
        if declared >= 0 and declared != len(indices):
            raise ValueError(f'{vmb.name}: .vmi says {declared} deltas, .vmb holds {len(indices)}')
    return Morph(vmb.stem, meta['display_name'] or vmb.stem, indices, deltas,
                 meta['minimum'], meta['maximum'], meta['group'], meta['formulas'],
                 source=str(vmb))


def load_package(var_path, into=None) -> list[Morph]:
    """Load every morph in a .var (a zip). Extracts to `into` when given, else a temp dir."""
    import tempfile
    import zipfile
    destination = Path(into) if into else Path(tempfile.mkdtemp(prefix='vam-morphs-'))
    with zipfile.ZipFile(var_path) as archive:
        wanted = [n for n in archive.namelist() if n.lower().endswith(('.vmb', '.vmi'))]
        archive.extractall(destination, members=wanted)
    return [load(p) for p in sorted(destination.rglob('*.vmb'))]


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description='Inspect VaM morph files.')
    parser.add_argument('paths', nargs='+', help='.vmb files, .var packages, or directories')
    parser.add_argument('--json', action='store_true', help='machine-readable output')
    args = parser.parse_args(argv)

    morphs: list[Morph] = []
    for entry in args.paths:
        path = Path(entry)
        if path.is_dir():
            morphs += [load(p) for p in sorted(path.rglob('*.vmb'))]
        elif path.suffix.lower() == '.var':
            morphs += load_package(path)
        else:
            morphs.append(load(path))

    if args.json:
        print(json.dumps([{
            'name': m.name, 'deltas': len(m), 'max_index': m.max_index,
            'targets_base': m.targets_base(), 'largest_move_cm': m.largest_move() * 100,
            'joint_formulas': len(m.formulas),
        } for m in morphs], indent=2))
        return 0

    print(f"{'morph':<46} {'deltas':>7} {'maxIdx':>7} {'move cm':>8} {'joints':>7}  base")
    print('-' * 88)
    for m in sorted(morphs, key=lambda x: x.name):
        print(f'{m.name[:44]:<46} {len(m):>7} {m.max_index:>7} '
              f'{m.largest_move() * 100:>8.2f} {len(m.formulas):>7}  '
              f'{"yes" if m.targets_base() else "GRAFT"}')
    grafted = [m for m in morphs if not m.targets_base()]
    print(f'\n{len(morphs)} morphs, {len(grafted)} needing the grafted mesh')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
