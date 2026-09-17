"""Read VaM hair (.vab written by RuntimeHairGeometryCreator) as strand polylines.

VaM hair is not a polygon mesh. It is GPUTools strand hair: a scalp mesh, a
per-scalp-vertex grow mask, and one polyline per growing vertex. VaM builds the
render geometry on the GPU every frame, so nothing in the file is triangles.
That is why the clothing reader chokes on it and why no amount of looking for a
DAZMesh section finds one.

The layout is taken from `RuntimeHairGeometryCreator.LoadFromBinaryReader` in
VaM's own Assembly-CSharp, decompiled with ilspycmd, so this is the format
rather than a guess:

    byte   1                      written by the outer store, not by the creator
    string "RuntimeHairGeometryCreator"
    string schema, "1.0" or "1.1"
    string scalpProviderName      LeytonScalp / UdaneScalp / PantyRegionScalp
    int32  segments               control points per strand (25 long, 5 pubic)
    float  segmentLength          metres (0.0161 long, 0.003 pubic)
    ScalpMask strandsMask:
        string name
        int32 count, then count bools     which scalp vertices grow hair
    int32  scalpVertexCount, then one Strand each:
        int32 scalpIndex
        int32 count, then count * (float x, y, z)   the styled control points
    int32  indexCount, then that many int32         line indices
    int32  vertexCount, then xyz floats             the rest strand vertices
    [1.1]  int32 rigidityCount, then floats         painted stiffness
    int32  rootCount, then int32 each               hair root -> scalp vertex
    int32  groupCount, then per group int32 n and n * (x, y, z, w)

Strings use the .NET 7-bit-encoded length prefix, as everywhere else in VaM.

The `vertices` array is the useful one: `indices` walks it in pairs, so the
strands come back as polylines ready to be turned into hair cards. Units are
metres in scalp-local space, Y-up facing -Z like the rest of DAZ.

    tools/vam_hair.py 'Lexi Long (REN).vab' --out work/hair
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from vam_clothing import VabError, _Reader

_SECTION = 'RuntimeHairGeometryCreator'
_SCHEMAS = ('1.0', '1.1')


class HairReader(_Reader):
    def boolean(self) -> bool:
        return self._take(1)[0] != 0

    def int32_array(self, count: int) -> np.ndarray:
        raw = self._take(count * 4)
        return np.frombuffer(raw, dtype='<i4', count=count).astype(np.int64)


class Hair:
    def __init__(self):
        self.scalp = ''
        self.schema = ''
        self.segments = 0
        self.segment_length = 0.0
        self.mask_name = ''
        self.mask = np.zeros(0, dtype=bool)
        self.strands: list[np.ndarray] = []       # styled control points, per scalp vertex
        self.indices = np.zeros(0, dtype=np.int64)
        self.vertices = np.zeros((0, 3))
        self.rigidities = None
        self.roots = np.zeros(0, dtype=np.int64)

    def __repr__(self):
        return (f'<Hair {self.scalp!r} schema {self.schema} {self.segments} segments '
                f'x {self.segment_length * 100:.2f} cm, {len(self.vertices)} strand vertices, '
                f'{int(self.mask.sum())} of {len(self.mask)} scalp vertices growing>')

    def polylines(self) -> list[np.ndarray]:
        """Split `vertices` into one array of points per strand.

        The array is strand-major with exactly `segments` points each, which is
        checked rather than assumed: `len(vertices) == len(roots) * segments`
        holds on every sample hair VaM ships.

        `indices` is *not* a line list. It comes from
        `ScalpProcessingTools.ProcessIndices` and holds the scalp triangles that
        grow hair, three indices per triangle. Reading it as line pairs looks
        like it works on the long hairs and quietly produces nonsense strand
        counts, so it is left alone here.
        """
        stride = max(1, self.segments)
        expected = len(self.roots) * stride
        if expected and expected != len(self.vertices):
            raise VabError(f'{len(self.vertices)} strand vertices is not '
                           f'{len(self.roots)} roots x {stride} segments')
        return [self.vertices[i:i + stride] for i in range(0, len(self.vertices), stride)]

    def scalp_triangles(self) -> np.ndarray:
        """The scalp triangles hair grows from, as indices into the scalp mesh."""
        return self.indices[:len(self.indices) // 3 * 3].reshape(-1, 3)

def read_hair(path) -> Hair:
    blob = Path(path).read_bytes()
    reader = HairReader(blob)
    wrapper = reader.string()
    if wrapper != 'DynamicStore':
        raise VabError(f'{path}: expected DynamicStore, found {wrapper!r}')
    reader.string()                                    # wrapper schema

    # The creator's own reader starts at its name. One byte sits between the
    # wrapper and it, written by the storable that owns the hair. Tolerate any
    # small prefix rather than hard-coding 1, then check the name.
    start = reader.at
    for skip in (1, 0, 2, 3, 4):
        reader.at = start + skip
        try:
            if reader.string() == _SECTION:
                break
        except VabError:
            continue
    else:
        raise VabError(f'{path}: no {_SECTION} section found')

    hair = Hair()
    hair.schema = reader.string()
    if hair.schema not in _SCHEMAS:
        raise VabError(f'{path}: hair schema {hair.schema!r} is not one of {_SCHEMAS}')
    hair.scalp = reader.string()
    hair.segments = reader.int32()
    hair.segment_length = reader.single()

    hair.mask_name = reader.string()
    mask_count = reader.int32()
    hair.mask = np.frombuffer(reader._take(mask_count), dtype=np.uint8).astype(bool)

    for _ in range(reader.int32()):
        reader.int32()                                 # scalpIndex, positional anyway
        count = reader.int32()
        hair.strands.append(reader.floats(count, 3) if count else np.zeros((0, 3)))

    hair.indices = reader.int32_array(reader.int32())
    hair.vertices = reader.floats(reader.int32(), 3)

    if hair.schema == '1.1':
        count = reader.int32()
        if count:
            raw = reader._take(count * 4)
            hair.rigidities = np.frombuffer(raw, dtype='<f4', count=count).astype(np.float64)

    hair.roots = reader.int32_array(reader.int32())
    return hair


def write_curves_obj(path, strands: list[np.ndarray]):
    """Write the strands as OBJ polylines (`l` records), which Blender imports."""
    lines, base = [f'# {len(strands)} hair strands'], 1
    for strand in strands:
        for x, y, z in strand:
            lines.append(f'v {x:.6f} {y:.6f} {z:.6f}')
        lines.append('l ' + ' '.join(str(base + i) for i in range(len(strand))))
        base += len(strand)
    Path(path).write_text('\n'.join(lines) + '\n')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('paths', nargs='+')
    parser.add_argument('--out', type=Path, help='write strand OBJs here')
    args = parser.parse_args(argv)

    targets: list[Path] = []
    for entry in args.paths:
        path = Path(entry)
        targets += sorted(path.rglob('*.vab')) if path.is_dir() else [path]
    if args.out:
        args.out.mkdir(parents=True, exist_ok=True)

    report = {}
    for path in targets:
        try:
            hair = read_hair(path)
        except VabError as exc:
            print(f'  {path.name[:40]:<42} not hair: {exc}')
            continue
        strands = hair.polylines()
        lengths = [float(np.linalg.norm(np.diff(s, axis=0), axis=1).sum()) for s in strands if len(s) > 1]
        longest = max(lengths) if lengths else 0.0
        print(f'  {path.stem[:30]:<32} scalp {hair.scalp[:18]:<20} {len(strands):>5} strands  '
              f'{hair.segments:>3} seg  longest {longest * 100:>6.1f} cm')
        report[path.stem] = {'scalp': hair.scalp, 'schema': hair.schema,
                             'segments': hair.segments,
                             'segment_length_cm': round(hair.segment_length * 100, 3),
                             'strands': len(strands), 'vertices': len(hair.vertices),
                             'growing_scalp_vertices': int(hair.mask.sum()),
                             'scalp_vertices': int(len(hair.mask)),
                             'longest_strand_cm': round(longest * 100, 2),
                             'has_rigidity_paint': hair.rigidities is not None}
        if args.out:
            safe = ''.join(c if c.isalnum() or c in ' _.-' else '_' for c in path.stem)
            write_curves_obj(args.out / f'{safe}.strands.obj', strands)
    if args.out:
        (args.out / 'hair.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
