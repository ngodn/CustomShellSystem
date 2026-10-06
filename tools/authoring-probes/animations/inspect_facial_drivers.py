"""Blender 5.2.2: read facial driver dependencies without executing source scripts."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import bpy


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def drivers(owner):
    data = getattr(owner, 'animation_data', None)
    return [dict(path=f.data_path, index=f.array_index, type=f.driver.type,
                 expression=f.driver.expression, mute=f.mute, valid=f.driver.is_valid,
                 variables=[dict(name=v.name, type=v.type, targets=[dict(
                     owner=t.id.name if t.id else None,
                     owner_type=t.id.bl_rna.identifier if t.id else None,
                     path=t.data_path, bone=t.bone_target,
                     transform=t.transform_type, space=t.transform_space,
                     rotation_mode=t.rotation_mode) for t in v.targets])
                     for v in f.driver.variables]) for f in data.drivers] if data else []


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists() or bpy.context.preferences.filepaths.use_scripts_auto_execute:
        raise ValueError('Use a new output path and --disable-autoexec')
    source = args.source.resolve(strict=True)
    before = digest(source)
    bpy.ops.wm.open_mainfile(filepath=str(source), use_scripts=False)
    records = []
    for obj in bpy.data.objects:
        owners = [(obj, 'object')]
        if obj.type == 'ARMATURE':
            owners.append((obj.data, 'armature_data'))
        elif obj.type == 'MESH' and obj.data.shape_keys:
            owners.append((obj.data.shape_keys, 'shape_keys'))
        for owner, kind in owners:
            curves = drivers(owner)
            properties = {name: value for name, value in owner.items()
                          if isinstance(value, (str, int, float, bool))}
            if curves or properties:
                records.append(dict(object=obj.name, owner=owner.name, kind=kind,
                                    properties=properties, drivers=curves))
    if digest(source) != before:
        raise ValueError('Source changed during inspection')
    report = dict(source=str(source), sha256=before, sources_unchanged=True,
                  blender=bpy.app.version_string, records=records,
                  scope='Saved dependency definitions only. No driver evaluation or facial mapping acceptance.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x') as stream:
        json.dump(report, stream, indent=2, allow_nan=False)
        stream.write('\n')
    print('FACIAL_DRIVER_AUDIT', len(records), sum(len(r['drivers']) for r in records), flush=True)


if __name__ == '__main__':
    main()
