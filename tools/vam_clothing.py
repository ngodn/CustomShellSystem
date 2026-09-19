"""Read VaM clothing meshes (.vab) and their item metadata (.vaj/.vam).

A VaM clothing item is three files sharing a stem:

    Item.vam   small JSON: display name, creator, tags, which region it covers
    Item.vaj   JSON: the component list (DAZMesh, DAZSkinWrap, sim settings) and
               the material definitions with their texture filenames
    Item.vab   binary: the geometry

The binary is a C# `BinaryWriter` stream. The layout is taken from VaM's own
`DAZMesh.LoadFromBinaryReader`, recovered by decompiling the shipped assemblies,
so this is the format rather than a guess:

    string "DynamicStore", string "1.0"     outer wrapper
    string "DAZMesh", string "1.0"          section header and schema
    string nodeId, sceneNodeId, geometryId, sceneGeometryId
    int32  numBaseVertices,   then x,y,z float32 each
    int32  numMaterials,      then a name string each
    int32  numBasePolygons,   then basePolyList: materialNum, count, indices
                              then UVPolyList:   materialNum, count, indices
    int32  numUVVertices,     then u,v float32 each (OrigUV)
    int32  numVertexMaps,     then fromvert, tovert, polyindex

Strings use the .NET 7-bit-encoded length prefix, not a fixed-width one.

Note `_UVVertices` is *not* stored. VaM rebuilds it by copying the base vertices
and then, for every vertex map, assigning `UV[tovert] = UV[fromvert]`. That is
reproduced here, otherwise seam vertices come out at the origin.
"""
from __future__ import annotations

import json
import struct
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np


class VabError(Exception):
    pass


class _Reader:
    def __init__(self, blob: bytes):
        self.blob = blob
        self.at = 0

    def _take(self, count: int) -> bytes:
        if self.at + count > len(self.blob):
            raise VabError(f'ran off the end at {self.at} wanting {count} bytes')
        chunk = self.blob[self.at:self.at + count]
        self.at += count
        return chunk

    def int32(self) -> int:
        return struct.unpack('<i', self._take(4))[0]

    def single(self) -> float:
        return struct.unpack('<f', self._take(4))[0]

    def string(self) -> str:
        """.NET BinaryWriter: 7-bit encoded length, then UTF-8."""
        length, shift = 0, 0
        while True:
            byte = self._take(1)[0]
            length |= (byte & 0x7F) << shift
            if not byte & 0x80:
                break
            shift += 7
            if shift > 35:
                raise VabError('string length prefix will not terminate')
        return self._take(length).decode('utf-8', errors='replace')

    def floats(self, count: int, per: int) -> np.ndarray:
        raw = self._take(count * per * 4)
        return np.frombuffer(raw, dtype='<f4', count=count * per).reshape(count, per).astype(np.float64)


@dataclass
class ClothingMesh:
    node_id: str = ''
    scene_node_id: str = ''
    geometry_id: str = ''
    base_vertices: np.ndarray = None
    uv_vertices: np.ndarray = None
    uvs: np.ndarray = None
    material_names: list[str] = field(default_factory=list)
    base_polys: list[tuple[int, list[int]]] = field(default_factory=list)
    uv_polys: list[tuple[int, list[int]]] = field(default_factory=list)
    vertex_maps: list[tuple[int, int, int]] = field(default_factory=list)

    def __repr__(self):
        return (f'<ClothingMesh {self.node_id!r} {len(self.base_vertices)} verts, '
                f'{len(self.base_polys)} polys, {len(self.material_names)} materials>')

    def triangles(self, uv_space: bool = False) -> np.ndarray:
        """Fan-triangulate the polygon list, dropping repeated corners."""
        source = self.uv_polys if uv_space else self.base_polys
        out = []
        for _, corners in source:
            deduped = []
            for c in corners:
                if c >= 0 and (not deduped or c != deduped[-1]):
                    deduped.append(c)
            if len(deduped) > 1 and deduped[0] == deduped[-1]:
                deduped.pop()
            for i in range(1, len(deduped) - 1):
                out.append([deduped[0], deduped[i], deduped[i + 1]])
        return np.asarray(out, dtype=np.int64)

    def material_of_triangle(self, uv_space: bool = False) -> np.ndarray:
        source = self.uv_polys if uv_space else self.base_polys
        out = []
        for material, corners in source:
            deduped = []
            for c in corners:
                if c >= 0 and (not deduped or c != deduped[-1]):
                    deduped.append(c)
            if len(deduped) > 1 and deduped[0] == deduped[-1]:
                deduped.pop()
            out += [material] * max(0, len(deduped) - 2)
        return np.asarray(out, dtype=np.int64)


def read_vab(path) -> ClothingMesh:
    reader = _Reader(Path(path).read_bytes())
    wrapper = reader.string()
    if wrapper != 'DynamicStore':
        raise VabError(f'{path}: expected DynamicStore, found {wrapper!r}')
    reader.string()                                  # wrapper schema
    section = reader.string()
    if section != 'DAZMesh':
        raise VabError(f'{path}: expected a DAZMesh section, found {section!r}')
    schema = reader.string()
    if schema != '1.0':
        raise VabError(f'{path}: DAZMesh schema {schema!r} is not 1.0')

    mesh = ClothingMesh()
    mesh.node_id = reader.string()
    mesh.scene_node_id = reader.string()
    mesh.geometry_id = reader.string()
    reader.string()                                  # sceneGeometryId

    count = reader.int32()
    mesh.base_vertices = reader.floats(count, 3)

    mesh.material_names = [reader.string() for _ in range(reader.int32())]

    polygons = reader.int32()
    for target in (mesh.base_polys, mesh.uv_polys):
        for _ in range(polygons):
            material = reader.int32()
            corners = [reader.int32() for _ in range(reader.int32())]
            target.append((material, corners))

    uv_count = reader.int32()
    mesh.uvs = reader.floats(uv_count, 2)

    # UVVertices are derived, not stored: start from the base positions, then
    # every seam vertex copies the one it split from.
    uv_vertices = np.zeros((uv_count, 3))
    shared = min(uv_count, len(mesh.base_vertices))
    uv_vertices[:shared] = mesh.base_vertices[:shared]
    for _ in range(reader.int32()):
        from_vert, to_vert, poly_index = reader.int32(), reader.int32(), reader.int32()
        mesh.vertex_maps.append((from_vert, to_vert, poly_index))
        if 0 <= to_vert < uv_count and 0 <= from_vert < uv_count:
            uv_vertices[to_vert] = uv_vertices[from_vert]
    mesh.uv_vertices = uv_vertices
    return mesh


def read_item(stem) -> dict:
    """Read the .vam/.vaj beside a .vab: name, tags, materials and textures."""
    stem = Path(stem).with_suffix('')
    info: dict = {'stem': str(stem)}
    vam = stem.with_suffix('.vam')
    if vam.exists():
        try:
            info['item'] = json.loads(vam.read_text(encoding='utf-8-sig'))
        except Exception as exc:
            info['item_error'] = str(exc)
    vaj = stem.with_suffix('.vaj')
    if vaj.exists():
        try:
            document = json.loads(vaj.read_text(encoding='utf-8-sig'))
            info['components'] = [c.get('type') for c in document.get('components', [])]
            materials = []
            for storable in document.get('storables', []):
                if 'id' in storable and any(k.endswith('Url') for k in storable):
                    materials.append({k: v for k, v in storable.items()
                                      if k == 'id' or k.endswith('Url')})
            info['materials'] = materials
        except Exception as exc:
            info['vaj_error'] = str(exc)
    return info


def write_obj(path, points: np.ndarray, triangles: np.ndarray, uvs: np.ndarray | None = None):
    lines = [f'# {len(points)} vertices, {len(triangles)} triangles']
    lines += [f'v {x:.6f} {y:.6f} {z:.6f}' for x, y, z in points]
    if uvs is not None and len(uvs) == len(points):
        lines += [f'vt {u:.6f} {v:.6f}' for u, v in uvs]
        lines += [f'f {a+1}/{a+1} {b+1}/{b+1} {c+1}/{c+1}' for a, b, c in triangles]
    else:
        lines += [f'f {a+1} {b+1} {c+1}' for a, b, c in triangles]
    Path(path).write_text('\n'.join(lines) + '\n')
    return path


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description='Read VaM clothing meshes.')
    parser.add_argument('paths', nargs='+', help='.vab files or directories holding them')
    parser.add_argument('--out', type=Path, help='write each mesh as OBJ here')
    parser.add_argument('--uv-space', action='store_true',
                        help='export the UV-split mesh instead of the base mesh')
    args = parser.parse_args(argv)

    targets: list[Path] = []
    for entry in args.paths:
        path = Path(entry)
        targets += sorted(path.rglob('*.vab')) if path.is_dir() else [path]

    if args.out:
        args.out.mkdir(parents=True, exist_ok=True)
    for path in targets:
        try:
            mesh = read_vab(path)
        except VabError as exc:
            print(f'  {path.name[:46]:<48} FAILED: {exc}')
            continue
        info = read_item(path)
        name = (info.get('item') or {}).get('displayName', path.stem)
        triangles = mesh.triangles(args.uv_space)
        points = mesh.uv_vertices if args.uv_space else mesh.base_vertices
        print(f'  {name[:38]:<40} {len(points):>6} verts {len(triangles):>6} tris  '
              f'materials={",".join(mesh.material_names)[:40]}')
        if args.out:
            safe = ''.join(c if c.isalnum() or c in ' _.-' else '_' for c in path.stem)[:70]
            write_obj(args.out / f'{safe}.obj', points, triangles,
                      mesh.uvs if args.uv_space else None)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
