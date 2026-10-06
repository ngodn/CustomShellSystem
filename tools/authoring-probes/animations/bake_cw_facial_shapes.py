"""Blender 5.2.2: map sampled CW facial deformations onto its fitted assembly.

Only accepts unchanged source-to-fitted coordinates at affected vertices.
The sparse schema-2 result requires a separate native import/readback gate.
"""
import argparse
import gzip
import hashlib
import json
import sys
from pathlib import Path

import bpy
import numpy as np


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--samples', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--assembly', type=Path, required=True)
    parser.add_argument('--topology-receipt', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists() or bpy.context.preferences.filepaths.use_scripts_auto_execute:
        raise ValueError('Use a new output directory and --disable-autoexec')
    sample_report = json.loads((args.samples / 'report.json').read_text())
    source_files = [args.source, args.assembly, args.topology_receipt,
                    args.samples / 'report.json', *sorted(args.samples.glob('*.npz'))]
    protected = {str(p.resolve()): digest(p) for p in source_files}
    bpy.ops.wm.open_mainfile(filepath=str(args.source.resolve()), use_scripts=False)
    rig = bpy.data.objects['CW CSS Rig']
    parts = sorted((o for o in bpy.data.objects if o.type == 'MESH' and o.find_armature() == rig), key=lambda o:o.name)
    if len(parts) != 33:
        raise ValueError('Expected the B9 assembly with 33 mesh objects')
    offsets, offset = {}, 0
    for obj in parts:
        offsets[obj.name] = offset
        offset += len(obj.data.vertices)
        for modifier in obj.modifiers:
            if modifier.type != 'ARMATURE':
                modifier.show_viewport = False
    bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    with np.load(args.samples / 'neutral.npz') as data:
        neutral = {name: data[name].copy() for name in data.files}
    positions = {}
    for name, original in neutral.items():
        obj = bpy.data.objects[name]
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        try:
            if len(mesh.vertices) != len(original):
                raise ValueError(f'{name}: fitted topology changed')
            values = np.empty(len(mesh.vertices)*3, dtype=np.float32)
            mesh.vertices.foreach_get('co', values)
            matrix = np.array(evaluated.matrix_world)
            positions[name] = (values.reshape((-1,3)).astype(float) @ matrix[:3,:3].T + matrix[:3,3])*100
        finally:
            evaluated.to_mesh_clear()
    samples, affected = [], {name:set() for name in neutral}
    for row in sample_report['summary']:
        if row['value'] not in (.25,.5,.75,1.0):
            raise ValueError('This bake requires quarter-weight samples')
        label = f'{row["control"]}-{row["value"]:g}'
        with np.load(args.samples / (label+'.npz')) as data:
            deltas = {name:data[name]-neutral[name] for name in neutral}
        for name, values in deltas.items():
            if values.shape != neutral[name].shape or not np.isfinite(values).all():
                raise ValueError('Invalid sampled deformation')
            affected[name].update(np.flatnonzero(np.linalg.norm(values,axis=1)>1e-6).tolist())
        samples.append((row,deltas))
    errors = {}
    for name, ids in affected.items():
        errors[name] = float(np.linalg.norm(positions[name][sorted(ids)]-neutral[name][sorted(ids)],axis=1).max()) if ids else 0
        if errors[name] > .0002:
            raise ValueError(f'{name}: source/fitted facial correspondence differs by {errors[name]} cm')
    vertices = sorted((offsets[name]+index, name, index) for name,ids in affected.items() for index in ids)
    receipt=json.loads(args.topology_receipt.read_text())
    with gzip.open(args.assembly,'rt') as stream:
        assembly=json.load(stream)
    if not receipt.get('other_geometry_unchanged') or not receipt.get('slot_ids_unchanged') or len(assembly['points'])!=receipt['counts']['points']:
        raise ValueError('Missing audited skirt topology insertion')
    # The recorded S7 assembly replaces only the skirt point range. Preserve
    # identities after that range, including the eyelashes and tear surface.
    insertion= len(assembly['points'])-offset
    skirt_start=offsets['Commander White Skirt Mesh']
    skirt_end=offsets['Commander White Top Mesh']
    if any(skirt_start<=i<skirt_end for i,_,_ in vertices):
        raise ValueError('Facial data unexpectedly includes skirt vertices')
    indices=[i+insertion if i>=skirt_end else i for i,_,_ in vertices]
    point_map = {(name,index):i for i,(_,name,index) in enumerate(vertices)}
    shapes = []
    for row,deltas in samples:
        values=[]
        for name,delta in deltas.items():
            for index in sorted(affected[name]):
                if np.linalg.norm(delta[index]) > 1e-6:
                    x,y,z=delta[index]
                    values.append([point_map[name,index],float(x),float(-y),float(z)])
        if not values:
            raise ValueError('No evaluated motion in source control')
        shapes.append(dict(name=f'CSSF_{row["control"].removeprefix("facs_")}_Q{round(row["value"]*4)}',
                           source_control=row['control'],sample_weight=row['value'],deltas=values))
    points = [[float(positions[name][index][0]),float(-positions[name][index][1]),float(positions[name][index][2])]
              for _,name,index in vertices]
    assembly_error=float(np.linalg.norm(np.asarray(assembly['points'])[indices]-np.asarray(points),axis=1).max())
    if assembly_error>.0002:
        raise ValueError(f'Current assembly correspondence differs by {assembly_error} cm')
    payload=dict(schema=2,point_indices=indices,body_points=points,morph_targets=shapes)
    if any(digest(Path(path))!=sha for path,sha in protected.items()):
        raise ValueError('A source changed during baking')
    args.output.mkdir(parents=True)
    target=args.output/'facial-shapes.json'
    target.write_text(json.dumps(payload,separators=(',',':'),allow_nan=False)+'\n')
    report=dict(protected=protected,sources_unchanged=True,blender=bpy.app.version_string,
                offsets=offsets,assembly_points=offset,affected_points=len(vertices),shapes=len(shapes),
                correspondence_error_cm=errors,sha256=digest(target),installed=False,
                topology_insertion_points=insertion,assembly_correspondence_error_cm=assembly_error,
                scope='Verified source/fitted facial point correspondence only; native import, visual and runtime acceptance pending.')
    (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('CW_FACE_BAKE_COMPLETE',len(shapes),len(vertices),errors,flush=True)


if __name__=='__main__':
    main()
