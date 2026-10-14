"""Compare a cooked repaired Eve pair with the installed cloth and rig bindings."""
import argparse
import copy
import json
from pathlib import Path

from build_eve_cloth_transfer import ROOT, WORK
from check_import_cooked_mesh import sha
from check_current_facial_preservation import normalize
from verify_blackpearl_surface_assembly import empty_nanite


def read(path):
    return json.loads(path.read_text())


def cloth(exports, package):
    rows = {}
    for item in exports:
        if item['Type'] not in ('ClothingAssetCommon','ChaosClothConfig','ChaosClothSharedSimConfig'):
            continue
        key = item['Type']+':'+item['Name']
        if key in rows:
            raise ValueError('Duplicate cloth/config identity')
        value = normalize(item, exports, package)
        if item['Type']=='ClothingAssetCommon':
            value['Properties'].pop('AssetGuid')
        rows[key] = value
    if len(rows)!=3:
        raise ValueError('Expected one cloth and two configurations')
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',choices=('planet','knit','alice'),required=True)
    args = parser.parse_args()
    base = WORK.parent/(args.variant+'-ordered')
    out = WORK/(args.variant+'-pack')
    output = out/'bindings-verified.json'
    if output.exists():
        raise FileExistsError(output)
    proof_path = out/'readbacks-verified.json'
    proof = read(proof_path)
    if not proof['passed']:
        raise ValueError('Cooked source checks failed')
    plan_path = base/'binding-plan.json'
    plan = read(plan_path)
    current_path = WORK.parent.parent/'current-inventory-20261014/meshes.json'
    repaired_path = out/'meshes.json'
    old, new = read(current_path),read(repaired_path)
    inputs = dict(proof['protected'])
    for p in (Path(__file__),Path(__file__).with_name('check_current_facial_preservation.py'),
              Path(__file__).with_name('verify_blackpearl_surface_assembly.py'),proof_path,plan_path,current_path,repaired_path):
        inputs[str(p)] = sha(p)
    pairs = read(WORK/(args.variant+'-cook/shipping-allowlist.json'))
    comparisons, digests = [],[]
    for kind,package in zip(('normal','facial'),pairs):
        reference = plan['reference_mesh' if kind=='normal' else 'reference_facial_mesh']
        a,b = old[reference],new[package]
        old_mesh = next(e for e in a if e['Type']=='SkeletalMesh')
        mesh = next(e for e in b if e['Type']=='SkeletalMesh')
        before,after = cloth(a,reference),cloth(b,package)
        if before!=after:
            raise ValueError('Cloth/config fields changed: '+kind)
        expected = normalize({k:v for k,v in old_mesh['Properties'].items() if k!='MorphTargets'},a,reference)
        actual = normalize({k:v for k,v in mesh['Properties'].items() if k!='MorphTargets'},b,package)
        expected = copy.deepcopy(expected)
        if not expected['LODInfo'][0]['BuildSettings']['bRecomputeTangents']:
            raise ValueError('Unexpected reference tangent setting')
        expected['LODInfo'][0]['BuildSettings']['bRecomputeTangents'] = False
        if expected!=actual:
            raise ValueError('Mesh properties changed beyond explicit tangent storage: '+kind)
        for field in ('ImportedBounds','SkeletalMaterials'):
            if old_mesh[field]!=mesh[field]:
                raise ValueError('Changed '+field+': '+kind)
        if empty_nanite(old_mesh['NaniteResources'])!=empty_nanite(mesh['NaniteResources']):
            raise ValueError('Changed empty Nanite resource: '+kind)
        digest = mesh['OfflineMeshDigest']
        if old_mesh['OfflineMeshDigest']['reference_skeleton_sha256']!=digest['reference_skeleton_sha256']:
            raise ValueError('Changed bind reference')
        sections = mesh['LODModels'][0]['Sections']
        bound = [s for s in sections if s['CorrespondClothAssetIndex']>=0]
        old_bound = [s for s in old_mesh['LODModels'][0]['Sections'] if s['CorrespondClothAssetIndex']>=0]
        if len(bound)!=1 or len(old_bound)!=1 or bound[0]['CorrespondClothAssetIndex']!=0 or bound[0]['MaterialIndex']!=old_bound[0]['MaterialIndex'] or bound[0]['NumTriangles']!=old_bound[0]['NumTriangles']:
            raise ValueError('Changed cloth section assignment')
        digests.append(digest)
        comparisons.append(dict(kind=kind,package=package,reference=reference,
            cloth_and_configs_equal_except_guid=True,mesh_properties_equal_except_tangent_storage=True,
            materials_bounds_nanite_bind_preserved=True,cloth_render_vertices=bound[0]['NumVertices'],
            cloth_render_triangles=bound[0]['NumTriangles']))
    if digests[0]['lods']!=digests[1]['lods']:
        raise ValueError('Normal/facial render or mapping buffers differ')
    if len(digests[0]['morphs'])!=22 or len(digests[1]['morphs'])!=137 or any(digests[1]['morphs'].get(n)!=v for n,v in digests[0]['morphs'].items()):
        raise ValueError('Normal/facial morph preservation differs')
    changed = [p for p,h in inputs.items() if sha(Path(p))!=h]
    if changed:
        raise ValueError('Changed protected inputs: '+str(changed))
    output.write_text(json.dumps(dict(passed=True,comparisons=comparisons,inputs=inputs,
        paired_render_and_cloth_mapping_digests_equal=True,
        scope='Cooked decoded cloth/config and mesh binding preservation, paired render/mapping equality. Regenerated mapping is not compared row-for-row to the differently indexed old mesh. Final paths, integrated packaging and runtime remain.'),indent=2)+'\n')
    print('Cooked cloth/config and paired binding checks passed.')


if __name__=='__main__':
    main()
