"""Minimal glTF/glb mesh reader: positions, triangles, UVs, normals.

AssetRipper exports meshes as .glb, and this machine has numpy and nothing else,
so rather than pull in a dependency this reads the subset a body mesh needs.

Deliberately not a general glTF implementation. It reads triangle primitives with
float positions and the usual index types, which is what a skinned character mesh
is. Anything else raises rather than returning something plausible and wrong,
because a silently mis-read base mesh would corrupt every morph applied to it.

Sparse accessors are supported because exporters do emit them for morph targets;
a sparse accessor that went unnoticed would leave vertices at their base values.
"""
from __future__ import annotations

import base64
import json
import struct
from pathlib import Path

import numpy as np

# glTF componentType -> numpy dtype
_COMPONENT = {
    5120: np.int8, 5121: np.uint8, 5122: np.int16,
    5123: np.uint16, 5125: np.uint32, 5126: np.float32,
}
_COUNT = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


class GltfError(Exception):
    pass


def _read_glb(path: Path) -> tuple[dict, bytes]:
    blob = path.read_bytes()
    if blob[:4] != b'glTF':
        # plain .gltf with external or embedded buffers
        return json.loads(blob.decode('utf-8')), b''
    version, total = struct.unpack_from('<II', blob, 4)
    if version != 2:
        raise GltfError(f'{path}: glb version {version}, only 2 is supported')
    offset, document, binary = 12, None, b''
    while offset < min(total, len(blob)):
        length, kind = struct.unpack_from('<I4s', blob, offset)
        chunk = blob[offset + 8:offset + 8 + length]
        if kind == b'JSON':
            document = json.loads(chunk.decode('utf-8'))
        elif kind == b'BIN\x00':
            binary = chunk
        offset += 8 + length + (-length % 4)
    if document is None:
        raise GltfError(f'{path}: no JSON chunk')
    return document, binary


def _buffer_bytes(document: dict, binary: bytes, index: int, base: Path) -> bytes:
    buffer = document['buffers'][index]
    uri = buffer.get('uri')
    if uri is None:
        return binary
    if uri.startswith('data:'):
        return base64.b64decode(uri.split(',', 1)[1])
    return (base / uri).read_bytes()


def _accessor(document: dict, binary: bytes, index: int, base: Path) -> np.ndarray:
    acc = document['accessors'][index]
    dtype = _COMPONENT.get(acc['componentType'])
    if dtype is None:
        raise GltfError(f'unsupported componentType {acc["componentType"]}')
    per = _COUNT.get(acc['type'])
    if per is None:
        raise GltfError(f'unsupported accessor type {acc["type"]}')
    count = acc['count']

    if 'bufferView' not in acc:
        values = np.zeros((count, per), dtype=dtype)
    else:
        view = document['bufferViews'][acc['bufferView']]
        raw = _buffer_bytes(document, binary, view.get('buffer', 0), base)
        start = view.get('byteOffset', 0) + acc.get('byteOffset', 0)
        stride = view.get('byteStride')
        item = np.dtype(dtype).itemsize * per
        if stride and stride != item:
            # interleaved: pull each element out by its own offset
            values = np.empty((count, per), dtype=dtype)
            for i in range(count):
                at = start + i * stride
                values[i] = np.frombuffer(raw, dtype=dtype, count=per, offset=at)
        else:
            values = np.frombuffer(raw, dtype=dtype, count=count * per, offset=start)
            values = values.reshape(count, per)
    values = np.array(values, dtype=dtype, copy=True)

    sparse = acc.get('sparse')
    if sparse:
        n = sparse['count']
        idx_info = sparse['indices']
        idx_view = document['bufferViews'][idx_info['bufferView']]
        idx_raw = _buffer_bytes(document, binary, idx_view.get('buffer', 0), base)
        idx_dtype = _COMPONENT[idx_info['componentType']]
        indices = np.frombuffer(
            idx_raw, dtype=idx_dtype, count=n,
            offset=idx_view.get('byteOffset', 0) + idx_info.get('byteOffset', 0))
        val_info = sparse['values']
        val_view = document['bufferViews'][val_info['bufferView']]
        val_raw = _buffer_bytes(document, binary, val_view.get('buffer', 0), base)
        replacements = np.frombuffer(
            val_raw, dtype=dtype, count=n * per,
            offset=val_view.get('byteOffset', 0) + val_info.get('byteOffset', 0)).reshape(n, per)
        values[indices.astype(np.int64)] = replacements

    if acc.get('normalized') and dtype != np.float32:
        info = np.iinfo(dtype)
        values = values.astype(np.float64)
        values = values / info.max if info.min == 0 else np.maximum(values / -info.min, -1.0)
    return values


class Mesh:
    """Merged triangle geometry from every primitive in the file."""

    def __init__(self, points: np.ndarray, triangles: np.ndarray,
                 uvs: np.ndarray | None, normals: np.ndarray | None, names: list[str],
                 joints: np.ndarray | None = None, weights: np.ndarray | None = None,
                 bone_names: list[str] | None = None):
        self.points = points
        self.triangles = triangles
        self.uvs = uvs
        self.normals = normals
        self.names = names
        # Skinning, when the file has it. `joints` indexes `bone_names`, and both
        # arrays carry every influence set the file declares (JOINTS_0, JOINTS_1,
        # ...), so a mesh with eight influences per vertex comes back with eight
        # columns rather than being silently truncated to the first four.
        self.joints = joints
        self.weights = weights
        self.bone_names = bone_names or []

    def __repr__(self):
        return (f'<Mesh {len(self.points)} verts, {len(self.triangles)} tris, '
                f'uv={"yes" if self.uvs is not None else "no"}, from {len(self.names)} primitives>')

    @property
    def vertex_count(self) -> int:
        return len(self.points)

    @property
    def is_skinned(self) -> bool:
        return self.joints is not None and len(self.bone_names) > 0

    def influences(self, vertex: int) -> list[tuple[str, float]]:
        """Bone name and weight for one vertex, zero weights dropped."""
        pairs = [(self.bone_names[j], float(w))
                 for j, w in zip(self.joints[vertex], self.weights[vertex]) if w > 0]
        return sorted(pairs, key=lambda pair: -pair[1])


def load(path) -> Mesh:
    """Read a .glb/.gltf and merge its triangle primitives into one Mesh."""
    path = Path(path)
    document, binary = _read_glb(path)
    base = path.parent

    # Skin joints are node indices; the names live on the nodes. glTF allows a
    # file to hold several skins, but a character mesh exported from Unreal has
    # one, and mixing two joint tables would make the indices meaningless.
    nodes = document.get('nodes', [])
    skins = document.get('skins', [])
    bone_names = [nodes[n].get('name', f'node_{n}') for n in skins[0].get('joints', [])] if skins else []

    points_parts, tri_parts, uv_parts, normal_parts, names = [], [], [], [], []
    joint_parts, weight_parts = [], []
    influence_sets = 0
    offset = 0
    for mesh in document.get('meshes', []):
        name = mesh.get('name', '')
        for primitive in mesh.get('primitives', []):
            mode = primitive.get('mode', 4)
            if mode != 4:
                continue                    # triangles only
            attributes = primitive.get('attributes', {})
            if 'POSITION' not in attributes:
                continue
            position = _accessor(document, binary, attributes['POSITION'], base).astype(np.float64)
            count = len(position)

            if 'indices' in primitive:
                indices = _accessor(document, binary, primitive['indices'], base).reshape(-1)
            else:
                indices = np.arange(count, dtype=np.uint32)
            if len(indices) % 3:
                raise GltfError(f'{path}: primitive has {len(indices)} indices, not a multiple of 3')
            triangles = indices.astype(np.int64).reshape(-1, 3) + offset

            uv = (_accessor(document, binary, attributes['TEXCOORD_0'], base).astype(np.float64)
                  if 'TEXCOORD_0' in attributes else np.full((count, 2), np.nan))
            normal = (_accessor(document, binary, attributes['NORMAL'], base).astype(np.float64)
                      if 'NORMAL' in attributes else np.full((count, 3), np.nan))

            sets = 0
            while f'JOINTS_{sets}' in attributes and f'WEIGHTS_{sets}' in attributes:
                sets += 1
            influence_sets = max(influence_sets, sets)
            if sets:
                joint_parts.append(np.hstack([
                    _accessor(document, binary, attributes[f'JOINTS_{i}'], base).astype(np.int64)
                    for i in range(sets)]))
                weight_parts.append(np.hstack([
                    _accessor(document, binary, attributes[f'WEIGHTS_{i}'], base).astype(np.float64)
                    for i in range(sets)]))
            else:
                joint_parts.append(None)
                weight_parts.append(None)

            points_parts.append(position)
            tri_parts.append(triangles)
            uv_parts.append(uv)
            normal_parts.append(normal)
            names.append(name)
            offset += count

    if not points_parts:
        raise GltfError(f'{path}: no triangle primitives with positions')

    points = np.vstack(points_parts)
    triangles = np.vstack(tri_parts)
    uvs = np.vstack(uv_parts)
    normals = np.vstack(normal_parts)

    joints = weights = None
    if influence_sets and bone_names:
        width = influence_sets * 4
        # An unskinned primitive next to skinned ones is padded with weight 0 so
        # the row count still matches `points`; dropping it instead would shift
        # every later vertex's influences onto the wrong vertex.
        joints = np.vstack([np.zeros((len(part), width), dtype=np.int64) if block is None
                            else np.pad(block, ((0, 0), (0, width - block.shape[1])))
                            for part, block in zip(points_parts, joint_parts)])
        weights = np.vstack([np.zeros((len(part), width)) if block is None
                             else np.pad(block, ((0, 0), (0, width - block.shape[1])))
                             for part, block in zip(points_parts, weight_parts)])

    return Mesh(points, triangles,
                None if np.isnan(uvs).all() else uvs,
                None if np.isnan(normals).all() else normals,
                names, joints, weights, bone_names)


def write_obj(path, points: np.ndarray, triangles: np.ndarray, uvs: np.ndarray | None = None):
    """Write a Wavefront OBJ, for eyeballing a result in any viewer."""
    path = Path(path)
    lines = [f'# {len(points)} vertices, {len(triangles)} triangles']
    lines += [f'v {x:.6f} {y:.6f} {z:.6f}' for x, y, z in points]
    if uvs is not None and not np.isnan(uvs).all():
        lines += [f'vt {u:.6f} {v:.6f}' for u, v in uvs]
        lines += [f'f {a+1}/{a+1} {b+1}/{b+1} {c+1}/{c+1}' for a, b, c in triangles]
    else:
        lines += [f'f {a+1} {b+1} {c+1}' for a, b, c in triangles]
    path.write_text('\n'.join(lines) + '\n')
    return path


def weld(points: np.ndarray, triangles: np.ndarray, tolerance: float = 1e-6):
    """Merge vertices that share a position, keeping first-occurrence order.

    glTF splits a vertex wherever its UV or normal differs, so an exported mesh
    has more vertices than the asset it came from. VaM morphs index the unsplit
    order, so the split has to be undone before a delta means anything.

    First-occurrence order is kept deliberately: an exporter appends the seam
    duplicates after the original run, so the first time each position appears is
    its original index. That is an assumption, not a guarantee, which is why
    `morph_lands_where_expected` exists to check it against real data rather than
    trusting it.

    Returns (welded_points, remapped_triangles, original_to_welded).
    """
    quantised = np.round(points / tolerance).astype(np.int64)
    _, first_index, inverse = np.unique(quantised, axis=0, return_index=True, return_inverse=True)
    order = np.argsort(first_index)               # unique() sorts; restore first-seen order
    rank = np.empty(len(order), dtype=np.int64)
    rank[order] = np.arange(len(order))
    mapping = rank[inverse.reshape(-1)]
    welded = np.zeros((len(order), points.shape[1]))
    welded[mapping] = points
    return welded, mapping[triangles], mapping


def morph_lands_where_expected(points: np.ndarray, indices, seed: int = 0,
                               trials: int = 16, margin: float = 0.6) -> dict:
    """Check a region morph's vertices form one cluster, not scattered noise.

    This is the test that catches a wrong vertex order, and it is the only
    honest one available: there is no other way to tell whether index 4471 on
    this mesh is the vertex the morph author meant. A real region morph (a
    breast, a navel) touches vertices that sit together on the body. If the mesh
    is ordered differently than the morph expects, those same index numbers pick
    out vertices scattered across the whole body.

    A fixed spread threshold does not work, because a legitimate region can be
    anything from a nipple to a whole thigh. So the region is compared against
    random samples of the same size drawn from the same mesh, which is what
    "scattered" actually looks like here. A region counts as clustered when its
    spread is below `margin` times the random baseline.
    """
    idx = np.unique(np.asarray(list(indices), dtype=np.int64))
    if not len(idx):
        raise ValueError('empty region')
    if idx.max() >= len(points):
        raise ValueError(f'region indexes vertex {idx.max()}, mesh has {len(points)}')

    def spread(sample: np.ndarray) -> float:
        # Mean distance to centroid: robust to a stray vertex in a way a bounding
        # box diagonal is not.
        return float(np.linalg.norm(sample - sample.mean(0), axis=1).mean())

    picked = points[idx]
    region_spread = spread(picked)
    rng = np.random.default_rng(seed)
    baseline = float(np.mean([
        spread(points[rng.choice(len(points), len(idx), replace=False)])
        for _ in range(trials)]))

    ratio = region_spread / baseline if baseline else 0.0
    return {
        'vertices': int(len(idx)),
        'region_spread': region_spread,
        'random_spread': baseline,
        'spread_ratio': ratio,
        'clustered': bool(ratio < margin),
        'centre': picked.mean(0).tolist(),
    }
