"""Blender 5.2.2: sample the original CW facial joints and corrective shapes.

Runs with embedded scripts disabled and never saves the Blender source.
These are source-space samples, not accepted fitted/runtime facial mappings.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

import bpy
import numpy as np


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def coordinates(vertices):
    values = np.empty(len(vertices) * 3, dtype=np.float32)
    vertices.foreach_get('co', values)
    return values.reshape((-1, 3)).astype(np.float64)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--control', action='append', required=True)
    parser.add_argument('--levels', nargs='+', type=float, default=[.25,.5,.75,1.0])
    parser.add_argument('--combined', action='store_true', help='Also evaluate all requested controls together')
    parser.add_argument('--combinations', type=Path, help='Explicit JSON list of labeled control combinations')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists() or bpy.context.preferences.filepaths.use_scripts_auto_execute:
        raise ValueError('Use a new output directory and --disable-autoexec')
    source = args.source.resolve(strict=True)
    before = digest(source)
    bpy.ops.wm.open_mainfile(filepath=str(source), use_scripts=False)
    rig = bpy.data.objects['Commander White 8.1_RIGIFY']
    deform = bpy.data.objects['Commander White 8.1']
    parts = [bpy.data.objects[name] for name in (
        'Commander White 8.1 Mesh', 'Genesis 8.1 Female Eyelashes Mesh',
        'Genesis 8.1 Female Tear Mesh')]
    if len(set(args.control)) != len(args.control):
        raise ValueError('Duplicate source control')
    if (len(set(args.levels))!=len(args.levels) or
        any(not np.isfinite(level) or not 0<level<=1 for level in args.levels)):
        raise ValueError('Sample levels must be distinct finite values in (0,1]')
    for name in args.control:
        if not name.startswith('facs_') or name not in rig or abs(float(rig[name])) > 1e-6:
            raise ValueError(f'Unknown or active source control: {name}')
    original = {name: rig[name] for name in args.control}
    plan=[]
    plan_hash=digest(args.combinations) if args.combinations else None
    if args.combinations:
        plan=json.loads(args.combinations.read_text())
        if not isinstance(plan,list) or not 1<=len(plan)<=512 or args.combined:
            raise ValueError('Use one bounded combination plan or --combined')
        labels=set()
        for item in plan:
            if (not isinstance(item,dict) or set(item)!={'label','controls'} or
                not isinstance(item['label'],str) or not re.fullmatch(r'mix_[A-Za-z0-9_]+',item['label']) or
                item['label'] in labels or not isinstance(item['controls'],dict) or not item['controls']):
                raise ValueError('Invalid combination entry')
            labels.add(item['label'])
            for name,value in item['controls'].items():
                if (name not in original or type(value) not in (int,float) or
                    not np.isfinite(value) or not 0<=value<=1):
                    raise ValueError('Invalid combination control')
    for obj in parts:
        obj.hide_set(False)
        obj.hide_viewport = False
        for modifier in obj.modifiers:
            if modifier.type != 'ARMATURE':
                modifier.show_viewport = False
    args.output.mkdir(parents=True)
    records = []

    def sample(label):
        rig.update_tag()
        rig.data.update_tag()
        bpy.context.view_layer.update()
        graph = bpy.context.evaluated_depsgraph_get()
        arrays, shapes = {}, {}
        for obj in parts:
            evaluated = obj.evaluated_get(graph)
            mesh = evaluated.to_mesh()
            try:
                if len(mesh.vertices) != len(obj.data.vertices):
                    raise ValueError(f'{obj.name}: evaluated topology changed')
                local = coordinates(mesh.vertices)
                matrix = np.array(evaluated.matrix_world)
                arrays[obj.name] = (local @ matrix[:3, :3].T + matrix[:3, 3]) * 100
                if not np.isfinite(arrays[obj.name]).all():
                    raise ValueError('Nonfinite source geometry')
            finally:
                evaluated.to_mesh_clear()
            keys = obj.data.shape_keys
            shapes[obj.name] = {k.name: float(k.value) for k in keys.key_blocks
                               if k.value != 0} if keys else {}
        np.savez_compressed(args.output / (label + '.npz'), **arrays)
        bones = {b.name: [list(row) for row in deform.matrix_world @ b.matrix]
                 for b in deform.pose.bones}
        records.append(dict(label=label, controls={name: float(rig[name]) for name in args.control},
                            shapes=shapes, bones=bones))
        print('CW_FACE_SAMPLE', label, flush=True)
        return arrays

    baseline = sample('neutral')
    summary = []
    for name in args.control:
        for level in args.levels:
            rig[name] = level
            label = f'{name}-{level:g}'
            actual = sample(label)
            measured = {part: dict(max_delta_cm=float(np.linalg.norm(a-baseline[part], axis=1).max()),
                                  affected_points=int(np.count_nonzero(np.linalg.norm(a-baseline[part], axis=1)>1e-5)))
                        for part, a in actual.items()}
            summary.append(dict(control=name, value=level, parts=measured))
        rig[name] = original[name]
        restored = sample(name + '-restored')
        if any(np.max(np.abs(restored[p]-baseline[p])) > 1e-5 for p in baseline):
            raise ValueError(f'{name}: source restoration changed geometry')
    combinations=[]
    if args.combined:
        plan=[dict(label=f'combined-{level:g}',controls={name:level for name in args.control}) for level in args.levels]
    if plan:
        for item in plan:
            for name,value in original.items():
                rig[name]=item['controls'].get(name,value)
            sample(item['label'])
            combinations.append(item)
        for name,value in original.items():
            rig[name]=value
        restored=sample('combined-restored')
        if any(np.max(np.abs(restored[p]-baseline[p]))>1e-5 for p in baseline):
            raise ValueError('Combined source restoration changed geometry')
    if digest(source) != before:
        raise ValueError('Source file changed')
    if args.combinations and digest(args.combinations)!=plan_hash:
        raise ValueError('Combination plan changed')
    report = dict(source=str(source), sha256=before, sources_unchanged=True,
                  blender=bpy.app.version_string, records=records, summary=summary,combinations=combinations,
                  combination_plan_sha256=plan_hash,
                  scope='Original source controls and evaluated geometry; fitted/runtime mapping unverified.')
    (args.output / 'report.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
    print('CW_FACE_SAMPLING_COMPLETE', len(summary), flush=True)


if __name__ == '__main__':
    main()
