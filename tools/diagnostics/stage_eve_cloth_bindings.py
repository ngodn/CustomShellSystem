"""Stage ordered Eve geometry and accepted references for a separate cloth transfer."""
import argparse
import json
from pathlib import Path
import shutil

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
VARIANTS = {
    'planet': ('Planet', '/Game/CSS/EveTest/SK_PFit14', 'eve26/planet-f14-import/planet.mesh.json'),
    'knit': ('Knit', '/Game/CSS/EveTest/SK_KCloth4', 'eve26/knit-w2-import/knit.mesh.json'),
    'alice': ('Alice', '/Game/CSS/EveTest/SK_ACloth3', 'eve26/alice-import4/alice.mesh.json'),
}
MIGRATION = ROOT / 'work/three-outfit-rig-physics/eve-rig-editor2'


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=VARIANTS, required=True)
    args = parser.parse_args()
    variant_name, old, source = VARIANTS[args.variant]
    WORK = ROOT / 'work/three-outfit-rig-physics/eve-surface-20261014' / (args.variant+'-ordered')
    receipt = read(WORK/'candidate.mesh.receipt.json')
    if Path(receipt['source']).resolve() != (ROOT/'work'/source).resolve() or sha(Path(receipt['source'])) != receipt['source_sha256']:
        raise ValueError('Wrong accepted source for variant')
    out = WORK / 'binding-plan.json'
    if out.exists():
        raise FileExistsError(out)
    if not read(WORK/'pack/verified.json')['passed'] or not read(MIGRATION/'reload.json')['passed']:
        raise ValueError('Candidate or migrated dependencies are unverified')
    if shutil.disk_usage(WORK).free < 2*1024**3:
        raise ValueError('Need at least 2 GiB free for binding and facial work')
    migrated = {r['package']: r for r in read(MIGRATION/'migration.json')['assets']}
    remap = {r['old_package']: r['new_package'] for r in read(MIGRATION/'input.json')['assets']}
    normal = remap[old]
    before = read(MIGRATION/'before-references.json')[old]
    after = migrated[normal]['references']
    inventory_path = ROOT/'work/three-outfit-rig-physics/current-inventory-20261014/inventory.json'
    inventory = read(inventory_path)
    raw_path = Path(inventory['raw_readback']['path'])
    if sha(raw_path) != inventory['raw_readback']['sha256']:
        raise ValueError('Installed mesh inventory changed')
    raw = next(e for e in read(raw_path)[normal] if e['Type'] == 'SkeletalMesh')
    pinned = {str(inventory_path): sha(inventory_path), str(raw_path): sha(raw_path),
              str(Path(__file__)): sha(Path(__file__))}
    for owner in ('CSS_EveStellarBlade_eins0fx_P', 'CSS_SharedAssets_P'):
        for suffix, row in inventory['packages'][owner]['files'].items():
            if owner == 'CSS_SharedAssets_P' and suffix == '.pak':
                continue  # CW's later catalog changed; shared binary assets did not.
            if sha(Path(row['path'])) != row['sha256']:
                raise ValueError('Installed dependency changed: '+row['path'])
            pinned[row['path']] = row['sha256']
    def package(ref):
        return ref['ObjectPath'].rsplit('.', 1)[0] if ref else None
    for source_key, cooked_key in (('skeleton', 'Skeleton'), ('physics_asset', 'PhysicsAsset'),
                                  ('post_process_anim_blueprint', 'PostProcessAnimBlueprint')):
        if after[source_key].split('.')[0] != package(raw['Properties'][cooked_key]):
            raise ValueError('Editor binding differs from current installed mesh')
    if len(raw['Properties'].get('MeshClothingAssets',[]))!=1:
        raise ValueError('Expected one accepted cloth asset')
    pair = next(v['meshes'] for v in inventory['variants'] if v['meshes']['normal']==normal)
    face_reference = pair['facial']
    face_old = next(k for k,v in remap.items() if v==face_reference)
    baseline = ROOT/'work/three-outfit-rig-physics/eve-surface-20261014/cloth-preservation-baseline.json'
    cloth_rows = [r for r in read(baseline)['rows'] if r['package'] in (normal,face_reference)]
    if len(cloth_rows)!=2 or cloth_rows[0]['physical_data_sha256']!=cloth_rows[1]['physical_data_sha256']:
        raise ValueError('Accepted normal/facial cloth baseline differs')
    pinned[str(baseline)] = sha(baseline)
    cooked_materials = raw['SkeletalMaterials']
    if len(cooked_materials) != len(after['materials']):
        raise ValueError('Material slot count changed')
    for old_material, cooked in zip(after['materials'], cooked_materials):
        if (old_material['slot'] != cooked['MaterialSlotName'] or
                old_material['material'].split('.')[0] != package(cooked['Material'])):
            raise ValueError('Material binding changed')
    edges = read(MIGRATION.parent/'eve-rig-dependencies2/dependencies.json')['edges']
    todo = [before[k].split('.')[0] for k in ('skeleton', 'physics_asset', 'post_process_anim_blueprint')]
    todo += [r['material'].split('.')[0] for r in before['materials']]
    todo.append(old)
    todo.append(face_old)
    seen = set()
    while todo:
        p = todo.pop()
        if p in seen or not p.startswith('/Game/'):
            continue
        if p not in edges:
            raise ValueError('Missing dependency graph entry: '+p)
        seen.add(p)
        todo.extend(edges[p])
    sources = {}
    for old_package in sorted(seen):
        p = remap.get(old_package, old_package)
        source = MIGRATION/'Content'/(p.removeprefix('/Game/')+'.uasset')
        digest = sha(source)
        if p in migrated and digest != migrated[p]['sha256']:
            raise ValueError('Migrated source changed: '+p)
        sources[p] = dict(path=str(source.resolve()), sha256=digest, role='editor_dependency_only')
    facial = ROOT/'work/cine-animation-port/facial-author-head-export-v1/facial-shapes.json'
    evidence = read(ROOT/'work/cine-animation-port/shared-faces-jali3-v1/facial-catalog-evidence.json')
    if sha(facial) != evidence['protected'][str(facial)]:
        raise ValueError('Accepted expression input changed')
    proof = read(WORK/'verified.json')
    candidate = read(WORK/'candidate.mesh.receipt.json')['mesh_package']
    for name, digest in proof['editor_assets'].items():
        if sha(WORK/'editor'/name) != digest:
            raise ValueError('Verified geometry asset changed')
        pinned[str((WORK/'editor'/name).resolve())] = digest
    pinned[str(facial.resolve())] = sha(facial)
    plan = dict(reference_mesh=normal, reference_facial_mesh=face_reference, candidate_mesh=candidate,
        normal_target='/Game/CSSAuthoring/EveSurface20261014/SK_'+variant_name+'Bound',
        facial_target='/Game/CSS/AnimLab/SK_Face_'+variant_name+'Ordered', facial_input=str(facial.resolve()),
        references=after, sources=sources, protected=pinned,
        scope='Intermediate editor bindings without cloth. These outputs must receive the accepted cloth through a separately verified native transfer before packaging. Dependencies are not shipping replacements.')
    (WORK/'binding-plan-staging.json').write_text(json.dumps(plan,indent=2)+'\n')
    for p, row in sources.items():
        target = WORK/'editor/Content'/(p.removeprefix('/Game/')+'.uasset')
        if target.exists():
            raise FileExistsError(target)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.symlink_to(row['path'])
        if sha(target) != row['sha256']:
            raise ValueError('Staged dependency differs')
    out.write_text(json.dumps(plan,indent=2)+'\n')
    print('Staged',len(sources),'editor-only dependencies; current installed bindings match')


if __name__ == '__main__':
    main()
