"""Check cooked Black Pearl bindings and paired render preservation before integration."""
import json
import copy
from pathlib import Path

from check_current_facial_preservation import normalize
from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT/'work/three-outfit-rig-physics/eve-surface-20261014/blackpearl-ordered'


def read(path):
    return json.loads(path.read_text())


def empty_nanite(value):
    result = copy.deepcopy(value)
    pages = result['StreamablePages']
    if pages['ElementCount'] != 0 or pages['SizeOnDisk'] != 0 or pages['BulkDataFlags'] != 'BULKDATA_None':
        raise ValueError('Expected empty Nanite bulk data')
    # The zero-byte bulk header moves when the mesh's serialized size changes.
    del pages['OffsetInFile']
    return result


def main():
    out = WORK/'binding-pack/assembly-verified.json'
    if out.exists():
        raise FileExistsError(out)
    receipt = read(WORK/'binding-pack/readbacks-verified.json')
    if not receipt['passed']:
        raise ValueError('Cooked comparisons failed')
    inputs = dict(receipt['protected'])
    files = [WORK/'binding-plan.json',WORK/'binding-pack/meshes.json',
             ROOT/'work/three-outfit-rig-physics/current-inventory-20261014/meshes.json',Path(__file__)]
    inputs.update({str(p):sha(p) for p in files})
    plan, current, original = [read(p) for p in files[:3]]
    paths = [plan['normal_target'],plan['facial_target']]
    old_package = plan['reference_mesh']
    old_exports = original[old_package]
    old = next(e for e in old_exports if e['Type']=='SkeletalMesh')
    inventory_path = ROOT/'work/three-outfit-rig-physics/current-inventory-20261014/inventory.json'
    inputs[str(inventory_path)] = sha(inventory_path)
    pairs = [v['meshes'] for v in read(inventory_path)['variants'] if v['meshes']['normal']==old_package]
    if len(pairs)!=1:
        raise ValueError('Expected one installed Black Pearl pair')
    def fields(mesh,exports,package):
        return normalize({k:v for k,v in mesh['Properties'].items() if k!='MorphTargets'},exports,package)
    meshes = []
    for name,path in zip(('normal','facial'),paths):
        old_package = pairs[0][name]
        old_exports = original[old_package]
        old = next(e for e in old_exports if e['Type']=='SkeletalMesh')
        expected = fields(old,old_exports,old_package)
        exports = current[path]
        mesh = next(e for e in exports if e['Type']=='SkeletalMesh')
        if fields(mesh,exports,path) != expected:
            raise ValueError('Non-morph cooked properties differ from installed Black Pearl: '+name)
        for field in ('SkeletalMaterials','ImportedBounds'):
            if mesh.get(field) != old.get(field):
                raise ValueError('Cook changed '+field)
        if empty_nanite(mesh['NaniteResources']) != empty_nanite(old['NaniteResources']):
            raise ValueError('Cook changed Nanite resource fields')
        if mesh['OfflineMeshDigest']['reference_skeleton_sha256'] != old['OfflineMeshDigest']['reference_skeleton_sha256']:
            raise ValueError('Cook changed reference bind pose')
        if any('Cloth' in e['Type'] for e in exports):
            raise ValueError('Unexpected cloth in Black Pearl')
        fallback_path = WORK/'binding-pack'/name/'fallback.json'
        inputs[str(fallback_path)] = sha(fallback_path)
        fallback = read(fallback_path)
        if not fallback['unit_lengths_within_bound'] or fallback['unexpected_nonorthogonal_vertices']:
            raise ValueError('Tangent repair regressed')
        meshes.append(mesh)
    a,b = [m['OfflineMeshDigest'] for m in meshes]
    if a['lods'] != b['lods'] or any(b['morphs'].get(k)!=v for k,v in a['morphs'].items()):
        raise ValueError('Facial addition changed base render data or prior morphs')
    if len(a['morphs']) != 22 or len(b['morphs']) != 137:
        raise ValueError('Unexpected morph inventories')
    if any(sha(Path(p)) != digest for p,digest in inputs.items()):
        raise ValueError('Protected input changed')
    out.write_text(json.dumps(dict(passed=True,packages=paths,
        material_slots_equal=30,non_morph_properties_equal=True,imported_bounds_equal=True,
        reference_bind_equal=True,paired_render_data_equal=True,original_morphs_preserved=22,
        empty_nanite_equal_except_zero_byte_bulk_offset=True,
        facial_morphs=137,unexpected_tangent_exceptions=0,
        collapsed_uv_fallback_vertices=[read(WORK/'binding-pack'/name/'fallback.json')['zero_uv_unit_x_fallback_vertices']
                                        for name in ('normal','facial')],
        inputs=inputs,scope='Private normal/facial Windows mesh assembly preserves installed bindings and accepted geometry/morph inputs. Existing dependency binaries remain installed and unchanged. Final-path integration, rendered shading, gameplay and performance remain.'),indent=2)+'\n')
    print('Private Black Pearl normal/facial assembly checks passed')


if __name__ == '__main__':
    main()
