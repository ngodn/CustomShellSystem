"""Pull DAZ-ordered base meshes out of VaM's exported Person prefab.

The mesh AssetRipper exports as .glb is VaM's *render* mesh: split at UV and
material seams, and reordered. Welding recovers the right vertex count but not
the right order, and a VaM morph indexes the DAZ order, so applying deltas to
the welded glb shreds the mesh. `gltf_mesh.morph_lands_where_expected` catches
that, which is how this was found rather than shipped.

VaM keeps both. From the decompiled `DAZMesh.cs`:

    protected Vector3[] _baseVertices;              // DAZ order, numBaseVertices
    protected DAZVertexMap[] _baseVerticesToUVVertices;
    protected Vector3[] _UVVertices;                // render order, numUVVertices

Those are serialized on the DAZMesh component, so they land in the Unity YAML of
`Assets/vamassets/prefabs/people/Person.prefab` once AssetRipper exports with the
managed assemblies present. Without the assemblies it cannot deserialise custom
MonoBehaviour fields and the prefab comes out nearly empty.

    tools/extract_daz_mesh.py --prefab .../Person.prefab --out work/v1.0.0-body/daz

Known components in VaM 1.22.0.13, by `_numBaseVertices`:

    21556  Genesis 2 Female, the figure every morph in the library indexes
     1452  the genitalia geograft (matches GenInnie's 1452 deltas exactly)
    23008  Genesis 2 Female with the graft merged
    17074  a hair/tail item
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

import numpy as np

_NUM_BASE = re.compile(r'^\s*_numBaseVertices:\s*(\d+)\s*$')
_NUM_UV = re.compile(r'^\s*_numUVVertices:\s*(\d+)\s*$')
_ARRAY_KEY = re.compile(r'^\s*(_baseVertices|_UVVertices|_basePolyList|_UVPolyList|_OrigUV'
                        r'|_baseVerticesToUVVertices):\s*(\[\])?\s*$')
# Unity writes floats like -1.2e-05, so the exponent sign must be allowed;
# leaving it out silently dropped 59 of 21556 vertices, which is exactly the
# kind of near-miss that would corrupt every morph applied afterwards.
_NUMBER = r'(-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)'
_VEC3 = re.compile(r'^\s*-\s*\{x:\s*' + _NUMBER + r',\s*y:\s*' + _NUMBER + r',\s*z:\s*' + _NUMBER + r'\}')
_VEC2 = re.compile(r'^\s*-\s*\{x:\s*' + _NUMBER + r',\s*y:\s*' + _NUMBER + r'\}')
# A MeshPoly serialises as `vertices: <hex>`, four little-endian int32 vertex
# indices packed as a hex string. A triangle repeats its last index.
#   1f00000020000000160000000c020000 -> [31, 32, 22, 524]
_POLY_HEX = re.compile(r'^\s*vertices:\s*([0-9a-fA-F]+)\s*$')
_POLY_MATERIAL = re.compile(r'^\s*-\s*materialNum:\s*(\d+)\s*$')
_OTHER_KEY = re.compile(r'^\s{2}\w')


class Component:
    def __init__(self, line: int):
        self.line = line
        self.num_base = None
        self.num_uv = None
        self.arrays: dict[str, list] = {}

    def __repr__(self):
        return (f'<DAZMesh @line {self.line} base={self.num_base} uv={self.num_uv} '
                f'arrays={ {k: len(v) for k, v in self.arrays.items()} }>')


def parse(path: Path, wanted_counts: set[int] | None = None) -> list[Component]:
    """Stream the prefab, collecting DAZMesh components and their arrays.

    Streamed because the prefab is well over a hundred megabytes; loading it as
    YAML would need many times that in memory for no benefit.
    """
    components: list[Component] = []
    current: Component | None = None
    key: str | None = None
    poly: list[int] | None = None

    with path.open('r', errors='replace') as handle:
        for number, line in enumerate(handle, 1):
            base = _NUM_BASE.match(line)
            if base:
                current = Component(number)
                components.append(current)
                current.num_base = int(base.group(1))
                key = None
                continue
            if current is None:
                continue
            uv = _NUM_UV.match(line)
            if uv:
                current.num_uv = int(uv.group(1))
                continue

            found = _ARRAY_KEY.match(line)
            if found:
                key = found.group(1)
                if found.group(2) == '[]':          # empty inline array
                    current.arrays[key] = []
                    key = None
                else:
                    current.arrays[key] = []
                poly = None
                continue

            if key is None:
                continue

            if key in ('_baseVertices', '_UVVertices'):
                m = _VEC3.match(line)
                if m:
                    current.arrays[key].append((float(m.group(1)), float(m.group(2)), float(m.group(3))))
                    continue
            elif key == '_OrigUV':
                m = _VEC2.match(line)
                if m:
                    current.arrays[key].append((float(m.group(1)), float(m.group(2))))
                    continue
            elif key in ('_basePolyList', '_UVPolyList'):
                if _POLY_MATERIAL.match(line):
                    continue                       # material index, not geometry
                m = _POLY_HEX.match(line)
                if m:
                    blob = bytes.fromhex(m.group(1))
                    current.arrays[key].append(
                        [int.from_bytes(blob[i:i + 4], 'little', signed=True)
                         for i in range(0, len(blob), 4)])
                    continue

            # Any other key at component indentation ends the current array.
            if _OTHER_KEY.match(line):
                key = None
                poly = None
    return components


def polys_to_triangles(polys) -> np.ndarray:
    """MeshPoly is a quad with a repeated index for triangles; fan-triangulate."""
    triangles = []
    for poly in polys:
        corners = [c for c in poly if c >= 0]
        # DAZ marks a triangle by repeating a corner; drop the duplicate so the
        # fan below does not emit a degenerate face.
        deduped = []
        for c in corners:
            if not deduped or c != deduped[-1]:
                deduped.append(c)
        if len(deduped) > 1 and deduped[0] == deduped[-1]:
            deduped.pop()
        corners = deduped
        for i in range(1, len(corners) - 1):
            triangles.append([corners[0], corners[i], corners[i + 1]])
    return np.asarray(triangles, dtype=np.int64)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--prefab', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--only', type=int, action='append',
                        help='only export components with this _numBaseVertices')
    args = parser.parse_args(argv)

    args.out.mkdir(parents=True, exist_ok=True)
    print(f'streaming {args.prefab} ({args.prefab.stat().st_size / 1e6:.0f} MB) ...', flush=True)
    components = parse(args.prefab)
    print(f'{len(components)} DAZMesh components')

    summary = []
    for component in components:
        counts = {k: len(v) for k, v in component.arrays.items() if v}
        print(f'  line {component.line:>9}  base={component.num_base:<7} uv={component.num_uv:<7} {counts}')
        summary.append({'line': component.line, 'num_base': component.num_base,
                        'num_uv': component.num_uv, 'arrays': counts})

        if args.only and component.num_base not in args.only:
            continue
        verts = component.arrays.get('_baseVertices') or []
        if len(verts) != component.num_base:
            print(f'      skipped: got {len(verts)} vertices, header says {component.num_base}')
            continue
        points = np.asarray(verts, dtype=np.float64)
        triangles = polys_to_triangles(component.arrays.get('_basePolyList') or [])
        name = f'daz_{component.num_base}'
        lines = [f'# DAZ-ordered base mesh, {len(points)} vertices']
        lines += [f'v {x:.6f} {y:.6f} {z:.6f}' for x, y, z in points]
        lines += [f'f {a+1} {b+1} {c+1}' for a, b, c in triangles]
        (args.out / f'{name}.obj').write_text('\n'.join(lines) + '\n')
        print(f'      wrote {name}.obj  {len(points)} verts, {len(triangles)} tris')

    (args.out / 'components.json').write_text(json.dumps(summary, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
