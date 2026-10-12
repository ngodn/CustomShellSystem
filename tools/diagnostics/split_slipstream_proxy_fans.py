"""Separate disconnected simulation vertex fans without changing triangle geometry."""
from collections import defaultdict
import argparse
import copy
import json
from pathlib import Path
import sys

import numpy as np

from run_slipstream_constraint_isolation import ROOT, EVE, sha
sys.path.insert(0, str(EVE / 'tools'))
from audit_slipstream_proxy_topology import audit, components

SOURCE = EVE / 'work/slipstream-cloth-proxy3/proxy.json'
OUT = ROOT / 'work/three-outfit-rig-physics/slipstream-splitfans1'
SLOT = 'Slipstream_skirt_Fabric'


def split_fans(positions, faces):
    triangles = np.asarray(faces, dtype=int).reshape(-1, 3).copy()
    donors = list(range(len(positions)))
    splits = []
    for vertex in range(len(positions)):
        incident = np.flatnonzero((triangles == vertex).any(axis=1))
        adjacent = {int(i): set() for i in incident}
        edges = defaultdict(list)
        for face in incident:
            for neighbor in triangles[face]:
                if neighbor != vertex:
                    edges[int(neighbor)].append(int(face))
        for group in edges.values():
            for face in group:
                adjacent[face].update(set(group) - {face})
        for group in components(adjacent)[1:]:
            duplicate = len(donors)
            donors.append(vertex)
            for face in group:
                triangles[face, triangles[face] == vertex] = duplicate
            splits.append(dict(source=vertex, duplicate=duplicate, faces=group))
    return np.asarray(donors), triangles, splits


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--discard-small-islands', action='store_true')
    args = parser.parse_args()
    if args.discard_small_islands:
        OUT = OUT.with_name('slipstream-splitfans2')
    OUT.mkdir(exist_ok=False)
    # A bow-tie requires distinct particles even when its two corners coincide.
    test_points = [[0, 0, 0], [1, 0, 0], [0, 1, 0], [-1, 0, 0], [0, -1, 0]]
    donors, triangles, _ = split_fans(test_points, [[0, 1, 2], [0, 3, 4]])
    assert len(donors) == 6 and audit(np.asarray(test_points)[donors], triangles)['manifold_vertices']
    original = json.loads(SOURCE.read_text())
    panel = original['slots'][SLOT]
    points = np.asarray(panel['positions'])
    old_triangles = np.asarray(panel['indices']).reshape(-1, 3)
    before = audit(points, old_triangles)
    assert before['manifold_edges'] and not before['manifold_vertices']
    donors, triangles, splits = split_fans(points, old_triangles)
    assert np.array_equal(points[donors][triangles], points[old_triangles])
    candidate = copy.deepcopy(original)
    changed = candidate['slots'][SLOT]
    fields = ('positions', 'normals', 'weights', 'max_distances', 'backstop_distances', 'backstop_radii')
    for field in fields:
        assert len(panel[field]) == len(points)
        changed[field] = [panel[field][int(i)] for i in donors]
    changed['indices'] = triangles.ravel().tolist()
    after = audit(changed['positions'], changed['indices'])
    assert after['manifold_edges'] and after['manifold_vertices']
    links = defaultdict(set)
    for face in triangles:
        for a in face:
            links[int(a)].update(int(b) for b in face if a != b)
    groups = []
    for group in components(links):
        mask = np.isin(triangles, group).all(axis=1)
        tri = points[donors][triangles[mask]]
        area = np.linalg.norm(np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]), axis=1).sum() / 2
        groups.append(dict(vertices=group, source_vertices=donors[group].tolist(), triangles=int(mask.sum()),
            area_cm2=float(area), pins=int((np.asarray(changed['max_distances'])[group] < .1).sum()),
            minimum_cm=points[donors][group].min(axis=0).tolist(),
            maximum_cm=points[donors][group].max(axis=0).tolist()))
    morph_source = SOURCE.with_name('proxy-morphs.json')
    morphs = json.loads(morph_source.read_text())
    for name, values in morphs.items():
        assert len(values) == len(points)
        morphs[name] = [values[int(i)] for i in donors]
    removed = []
    if args.discard_small_islands:
        unsupported = [g for g in groups if g['pins'] == 0]
        assert len(unsupported) == 2 and all(g['area_cm2'] < .2 for g in unsupported)
        ids = [i for group in unsupported for i in group['vertices']]
        keep = np.ones(len(donors), dtype=bool)
        keep[ids] = False
        face_keep = ~np.isin(triangles, ids).any(axis=1)
        assert int((~face_keep).sum()) == 3
        removed = unsupported
        remap = np.full(len(donors), -1, dtype=int)
        remap[keep] = np.arange(keep.sum())
        triangles = remap[triangles[face_keep]]
        donors = donors[keep]
        for field in fields:
            changed[field] = [row for row, yes in zip(changed[field], keep, strict=True) if yes]
        changed['indices'] = triangles.ravel().tolist()
        for name, values in morphs.items():
            morphs[name] = [row for row, yes in zip(values, keep, strict=True) if yes]
        after = audit(changed['positions'], triangles)
        assert after['manifold_edges'] and after['manifold_vertices']
        assert np.array_equal(points[donors][triangles], points[old_triangles[face_keep]])
        groups = [dict(g, vertices=remap[g['vertices']].tolist()) for g in groups if g['pins'] > 0]
    (OUT / 'proxy.json').write_text(json.dumps(candidate, separators=(',', ':')) + '\n')
    (OUT / 'proxy-morphs.json').write_text(json.dumps(morphs, separators=(',', ':')) + '\n')
    report = dict(source=str(SOURCE), source_sha256=sha(SOURCE), morph_source_sha256=sha(morph_source),
        recipe_sha256=sha(Path(__file__)), before=before, after=after, splits=splits, groups=groups,
        unchanged_retained_triangle_geometry=True, copied_particle_fields=list(fields), removed_islands=removed,
        unanchored_components=sum(g['pins'] == 0 for g in groups),
        scope='Topology-only private candidate; visible garment unchanged. No native binding or simulation acceptance.')
    (OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(dict(particles_before=len(points), particles_after=len(donors),
        invalid_fans_before=len(before['invalid_vertex_fans']), invalid_fans_after=len(after['invalid_vertex_fans']),
        connected_components=len(groups), unanchored=[g for g in groups if g['pins'] == 0]), indent=2))


if __name__ == '__main__':
    main()
