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
    parser.add_argument('--extended-levels', action='store_true',
                        help='Allow explicit signed/overshoot samples within [-4,4]')
    parser.add_argument('--combined', action='store_true', help='Also evaluate all requested controls together')
    parser.add_argument('--combinations', type=Path, help='Explicit JSON list of labeled control combinations')
    parser.add_argument('--tongue-fk', action='store_true',
                        help='Sample tongue joint controls with source tongue IK disabled in memory')
    parser.add_argument('--eye-gaze-range', action='store_true',
                        help='Bake horizontal eye motion within 45 degrees; restore source limits afterward')
    parser.add_argument('--signed-mouth-range', action='store_true',
                        help='Bake signed cheek and tongue-width actions; restore source limits afterward')
    parser.add_argument('--omit-bone-records', action='store_true',
                        help='Keep evaluated geometry and control records without repeating all bone matrices')
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
        any(not np.isfinite(level) or level==0 or
            not (-4<=level<=4 if args.extended_levels else 0<level<=1)
            for level in args.levels)):
        raise ValueError('Use distinct nonzero finite levels in the selected sampling range')
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
                    not np.isfinite(value) or
                    not (-4<=value<=4 if args.extended_levels else 0<=value<=1)):
                    raise ValueError('Invalid combination control')
    for obj in parts:
        obj.hide_set(False)
        obj.hide_viewport = False
        for modifier in obj.modifiers:
            if modifier.type != 'ARMATURE':
                modifier.show_viewport = False
    args.output.mkdir(parents=True)
    records = []
    eye_driver_error = 0.0

    def sample(label):
        nonlocal eye_driver_error
        rig.update_tag()
        rig.data.update_tag()
        bpy.context.view_layer.update()
        if args.eye_gaze_range:
            for row in eye_ranges:
                data = bpy.data.armatures[row['armature']]
                control = row['property'][:-5]
                difference = abs(float(data[row['property']]) - float(rig[control]))
                eye_driver_error = max(eye_driver_error, difference)
                # Imported Bezier identity drivers round values near zero.
                # This bound is under 0.0031 degrees for the measured eye axes.
                if difference > 1e-4:
                    raise ValueError(f'Eye driver altered {control}: requested {rig[control]}, '
                                     f'{data.name} returned {data[row["property"]]}')
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
        bones = {} if args.omit_bone_records else {
            b.name: [list(row) for row in deform.matrix_world @ b.matrix] for b in deform.pose.bones}
        records.append(dict(label=label, controls={name: float(rig[name]) for name in args.control},
                            shapes=shapes, bones=bones))
        print('CW_FACE_SAMPLE', label, flush=True)
        return arrays

    tongue_mode = None
    eye_limits = []
    eye_ranges = []
    original_neutral = sample('original-neutral') if (
        args.tongue_fk or args.eye_gaze_range or args.signed_mouth_range) else None
    if args.eye_gaze_range:
        for bone, expected in [('eye.L', (-40, 30)), ('eye.R', (-30, 40))]:
            constraints = [c for c in rig.pose.bones[bone].constraints if c.type == 'LIMIT_ROTATION']
            if len(constraints) != 1:
                raise ValueError('Expected one audited eye limit: ' + bone)
            constraint = constraints[0]
            if (constraint.mute or constraint.influence != 1 or constraint.owner_space != 'LOCAL' or
                    not constraint.use_limit_z or
                    not np.allclose(np.degrees([constraint.min_z, constraint.max_z]), expected, atol=1e-5)):
                raise ValueError('Eye limit no longer matches source audit: ' + bone)
            eye_limits.append(dict(bone=bone, name=constraint.name,
                                   min_z=float(constraint.min_z), max_z=float(constraint.max_z)))
            constraint.min_z, constraint.max_z = np.radians([-45, 45])
        for data in (rig.data, deform.data):
            for side in ('Left', 'Right'):
                for axis in ('Side-Side', 'Up-Down'):
                    name = f'facs_ctrl_EyeLook{axis}{side}(fin)'
                    ui = data.id_properties_ui(name)
                    limits = ui.as_dict()
                    if limits['min'] != -1 or limits['max'] != 1:
                        raise ValueError('Unexpected intermediate eye driver range: ' + name)
                    eye_ranges.append(dict(armature=data.name, property=name, limits=limits))
                    ui.update(min=-1.5, max=1.5)
    if args.tongue_fk:
        if 'MhaTongueIk' not in rig or not np.isfinite(float(rig['MhaTongueIk'])):
            raise ValueError('Source lacks the audited tongue IK control')
        tongue_mode = float(rig['MhaTongueIk'])
        rig['MhaTongueIk'] = 0.0
    signed_ranges, signed_shapes = [], []
    if args.signed_mouth_range:
        for data in (rig.data, deform.data):
            for name in ('facs_bs_CheekPuffLeft_div2(fin)', 'facs_bs_CheekPuffRight_div2(fin)',
                         'facs_ctrl_CheekPuff(fin)', 'facs_bs_TongueNarrow(fin)'):
                ui = data.id_properties_ui(name)
                limits = ui.as_dict()
                if limits['min'] != 0 or limits['max'] != 1:
                    raise ValueError('Signed mouth source range changed: ' + name)
                signed_ranges.append(dict(armature=data.name, name=name, limits=limits))
                ui.update(min=-1, max=1)
            data.update_tag()
        expected_shapes = {'facs_bs_CheekPuffLeft_div2', 'facs_bs_CheekPuffRight_div2', 'facs_bs_TongueNarrow'}
        for obj in parts:
            if obj.data.shape_keys:
                for key in obj.data.shape_keys.key_blocks:
                    if key.name in expected_shapes:
                        expected_minimum = -1 if obj.name == 'Genesis 8.1 Female Tear Mesh' else 0
                        if key.slider_min != expected_minimum:
                            raise ValueError('Signed mouth shape range changed: ' + key.name)
                        signed_shapes.append(dict(object=obj.name, name=key.name, minimum=key.slider_min))
                        key.slider_min = -1
                obj.data.shape_keys.update_tag()
        if len(signed_shapes) != 5 or {row['name'] for row in signed_shapes} != expected_shapes:
            raise ValueError('Expected the audited body and tear mouth shapes')
    baseline = sample('neutral')
    neutral_shift = ({name: float(np.linalg.norm(baseline[name]-original_neutral[name], axis=1).max())
                      for name in baseline} if original_neutral is not None else {})
    summary = []
    for name in args.control:
        for level in args.levels:
            rig[name] = level
            if float(rig[name])!=level:
                raise ValueError('Source clamped the requested control: '+name)
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
                if float(rig[name])!=item['controls'].get(name,value):
                    raise ValueError('Source clamped a combined control: '+name)
            sample(item['label'])
            combinations.append(item)
        for name,value in original.items():
            rig[name]=value
        restored=sample('combined-restored')
        if any(np.max(np.abs(restored[p]-baseline[p]))>1e-5 for p in baseline):
            raise ValueError('Combined source restoration changed geometry')
    if args.tongue_fk:
        rig['MhaTongueIk'] = tongue_mode
    for row in eye_limits:
        constraint = rig.pose.bones[row['bone']].constraints[row['name']]
        constraint.min_z, constraint.max_z = row['min_z'], row['max_z']
    for row in eye_ranges:
        bpy.data.armatures[row['armature']].id_properties_ui(row['property']).update(**row['limits'])
    for row in signed_ranges:
        data = bpy.data.armatures[row['armature']]
        data.id_properties_ui(row['name']).update(**row['limits'])
        data.update_tag()
    for row in signed_shapes:
        keys = bpy.data.objects[row['object']].data.shape_keys
        keys.key_blocks[row['name']].slider_min = row['minimum']
        keys.update_tag()
    if original_neutral is not None:
        restored = sample('original-neutral-restored')
        if any(np.max(np.abs(restored[p]-original_neutral[p])) > 1e-5 for p in baseline):
            raise ValueError('Original source modes did not restore geometry')
    if digest(source) != before:
        raise ValueError('Source file changed')
    if args.combinations and digest(args.combinations)!=plan_hash:
        raise ValueError('Combination plan changed')
    report = dict(source=str(source), sha256=before, sources_unchanged=True,
                  blender=bpy.app.version_string, records=records, summary=summary,combinations=combinations,
                  combination_plan_sha256=plan_hash,
                  omitted_bone_records=args.omit_bone_records,
                  signed_mouth_range=args.signed_mouth_range,
                  original_signed_ranges=signed_ranges, original_signed_shapes=signed_shapes,
                  extended_levels=args.extended_levels,
                  eye_gaze_range=args.eye_gaze_range, original_eye_limits=eye_limits,
                  original_eye_ranges=eye_ranges,
                  max_eye_driver_error=eye_driver_error,
                  tongue_fk=args.tongue_fk, original_tongue_ik=tongue_mode,
                  tongue_mode_neutral_shift_cm=neutral_shift,
                  scope='Evaluated CW source-rig geometry; reported temporary bake settings restored. Fitted/runtime mapping unverified.')
    (args.output / 'report.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
    print('CW_FACE_SAMPLING_COMPLETE', len(summary), flush=True)


if __name__ == '__main__':
    main()
