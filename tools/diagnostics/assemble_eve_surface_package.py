"""Integrate the twelve verified Eve mesh repairs without changing accepted content.

Python 3.14. Prepare, package and verify are separate resumable stages. Nothing
is installed. The new staging tree lives on root, with a link in CSS work.
"""
import argparse
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from check_import_cooked_mesh import sha
from check_current_facial_preservation import normalize
from cook_eve_alice_candidate import filesystem_uuid
from verify_blackpearl_surface_assembly import empty_nanite

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info
from css_facial_resources import encode, externalize, resolve
from css_package import verify as verify_outfit

WORK = ROOT/'work/three-outfit-rig-physics'
SURFACE = WORK/'eve-surface-20261014'
BASELINE = WORK/'peony-v2-motion-package1'
CW = ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/work/walk-final-paths1/assembly'
OUT = ROOT/'build/eve-surface-package1'
LINK = WORK/'eve-surface-package1'
INSTALLED = GAME/'Content/Paks/~mods'
EVE, SHARED = 'CSS_EveStellarBlade_eins0fx_P', 'CSS_SharedAssets_P'
OTHERS = ('CSS_CommanderWhite_eins0fx_P', 'CSS_UnholyGenessa_eins0fx_P')
RETOC = ROOT/'build/retoc-css-target/release/retoc'
FINAL_SETS = ('blackpearl-final','eve-tangent-final','planet-cloth-final','knit-cloth-final','alice-cloth-final')


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2)+'\n')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def unchanged(pins):
    bad = [p for p,h in pins.items() if not Path(p).is_file() or sha(Path(p))!=h]
    require(not bad, 'Protected files changed: '+str(bad))


def run(label, command):
    command = list(map(str, command))
    marker = OUT/(label+'-command.json')
    if marker.exists():
        require(read(marker)==command and read(OUT/(label+'-exit.json'))=={'returncode':0},
                'Existing step is not a successful identical command: '+label)
        return
    write(marker, command)
    with (OUT/(label+'.log')).open('w') as log:
        proc = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
    write(OUT/(label+'-exit.json'), dict(returncode=proc.returncode))
    proc.check_returncode()


def copy_payload(source, target, digest):
    require(sha(source)==digest, 'Source changed: '+str(source))
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists():
        try:
            os.link(source,target)
        except OSError:
            subprocess.run(['cp','--reflink=auto',str(source),str(target)],check=True)
    require(sha(target)==digest, 'Staged payload differs: '+str(target))


def prepare():
    require(not OUT.exists() and not LINK.exists() and not LINK.is_symlink(), 'Use a new package staging area')
    require(OUT.parent.resolve()==OUT.parent and filesystem_uuid(OUT.parent)==filesystem_uuid('/'), 'Output must use root filesystem')
    require(shutil.disk_usage(OUT.parent).free>=12*1024**3, 'Need 12 GiB root space for staging, package and readback')
    checkpoint = ROOT/'docs/development/eve-surface-20261014.json'
    checkpoint_bytes=checkpoint.read_bytes()
    checkpoint_hash=sha(checkpoint)
    pins = {str(p):sha(p) for p in (Path(__file__),BASELINE/'assembly.json',CW/'installed.json',
        CW/'verification.json',CW/'validation/offline-validation.json',RETOC,DEFAULT_REPAK,
        ROOT/'tools/css_convert.py',ROOT/'tools/css_package.py',ROOT/'tools/css_facial_resources.py',
        Path(__file__).with_name('check_current_facial_preservation.py'),
        Path(__file__).with_name('verify_blackpearl_surface_assembly.py'))}
    baseline,installed = read(BASELINE/'assembly.json'),read(CW/'installed.json')
    require(baseline['passed'] and installed['installed'] and read(CW/'validation/offline-validation.json')['passed'], 'Baseline verification missing')
    for owner in (EVE,SHARED,*OTHERS):
        for suffix in ('.pak','.utoc','.ucas'):
            name=owner+suffix
            expected=installed['retained'][owner][name] if owner in installed['retained'] else installed['files'][name]
            path=INSTALLED/owner/name
            require(sha(path)==expected,'Installed baseline changed: '+name)
            if owner==EVE:
                require(baseline['files'][owner+'/'+name]==expected,'Eve provenance differs from current installed package')
            pins[str(path)]=expected
    replacements,expected_meshes={},{}
    for name in FINAL_SETS:
        pack=SURFACE/name/'pack';proof=read(pack/'verified.json')
        require(proof['passed'],'Unverified permanent pair: '+name)
        pins[str(pack/'verified.json')]=sha(pack/'verified.json')
        pins[str(pack/'meshes.json')]=sha(pack/'meshes.json')
        for row in proof['comparisons']:
            require(row['exact_decoded_preservation'] and row['target'] not in expected_meshes,'Duplicate or unverified mesh')
            expected_meshes[row['target']]=str(pack/'meshes.json')
        for relative,digest in proof['payloads'].items():
            require(relative in baseline['payloads'] and relative not in replacements,'Unexpected replacement payload')
            source=pack/'stage'/relative
            require(sha(source)==digest,'Repaired payload changed')
            replacements[relative]=dict(path=str(source),sha256=digest)
    require(len(expected_meshes)==12 and len(replacements)==24,'Expected twelve two-payload mesh packages')
    sources={h:Path(p) for p,h in baseline['inputs'].items() if Path(p).is_file()}
    payloads={}
    for relative,digest in baseline['payloads'].items():
        row=replacements.get(relative)
        source=Path(row['path']) if row else sources.get(digest)
        require(source is not None,'Missing accepted payload: '+relative)
        wanted=row['sha256'] if row else digest
        require(sha(source)==wanted,'Accepted payload changed: '+relative)
        pins[str(source)]=wanted
        payloads[relative]=dict(path=str(source),sha256=wanted,previous_sha256=digest)
    require({n for n,r in payloads.items() if r['sha256']!=r['previous_sha256']}==set(replacements),'Wrong replacement set')
    reader=read(SURFACE/'alice-cloth-final/pack/readback-command.json')
    imports=read(CW/'validation/imports-command.json')
    for p in (Path(reader[0]),Path(reader[1]),Path(reader[3]),Path(imports[1]),Path(imports[3]),
              ROOT/'build/host/css_animation_library_tests',BASELINE/'variants.json',CW/'validation/variants.json',
              Path(imports[4]),Path(imports[5])):
        pins[str(p)]=sha(p)
    require(sha(checkpoint)==checkpoint_hash,'Checkpoint changed during preparation')
    OUT.mkdir();LINK.symlink_to(OUT,target_is_directory=True)
    (OUT/'input-checkpoint.json').write_bytes(checkpoint_bytes)
    pins[str(OUT/'input-checkpoint.json')]=checkpoint_hash
    write(OUT/'plan.json',dict(protected=pins,payloads=payloads,replaced=sorted(replacements),
        expected_meshes=expected_meshes,reader=reader,imports=imports,installed=False,
        work_link=str(LINK),physical_path=str(OUT),scope='Offline full Eve integration; stable release and gameplay gates remain.'))
    for relative,row in payloads.items():
        copy_payload(Path(row['path']),OUT/'stage'/relative,row['sha256'])
    write(OUT/'prepared.json',dict(passed=True,plan_sha256=sha(OUT/'plan.json'),payload_count=len(payloads),mesh_count=len(expected_meshes)))
    print('Prepared current Eve payloads with exactly twelve repaired meshes',flush=True)


def package(plan):
    require(read(OUT/'prepared.json')['plan_sha256']==sha(OUT/'plan.json'),'Prepared plan changed')
    if (OUT/'packaged.json').exists():
        raise FileExistsError('Package already verified')
    for relative,row in plan['payloads'].items():
        require(sha(OUT/'stage'/relative)==row['sha256'],'Staged payload changed')
    (OUT/'pack'/EVE).mkdir(parents=True,exist_ok=True)
    run('convert',[RETOC,'to-zen',OUT/'stage',OUT/'pack'/EVE/(EVE+'.utoc'),'--version','UE5_6','--no-parallel'])
    run('container',[RETOC,'verify',OUT/'pack'/EVE/(EVE+'.utoc')])
    manifests,manifest_paths,metadata_files={},{},{}
    for owner in (EVE,SHARED):
        original=OUT/'metadata-original'/owner
        metadata=OUT/'metadata'/owner
        original.parent.mkdir(parents=True,exist_ok=True)
        run(owner+'-unpack',[DEFAULT_REPAK,'unpack',INSTALLED/owner/(owner+'.pak'),'--output',original])
        if not metadata.exists():shutil.copytree(original,metadata)
        paths=list(original.rglob('manifest.json'))
        require(len(paths)==1,'Expected one embedded manifest')
        manifest_paths[owner]=metadata/paths[0].relative_to(original);manifests[owner]=read(paths[0])
        metadata_files[owner]={str(p.relative_to(original)):sha(p)
            for p in original.rglob('*') if p.is_file()}
    before=copy.deepcopy(manifests)
    for suffix in ('.utoc','.ucas'):
        p=OUT/'pack'/EVE/(EVE+suffix)
        manifests[EVE]['containers'][suffix]=dict(file=p.name,bytes=p.stat().st_size,sha256=sha(p))
        source=INSTALLED/SHARED/(SHARED+suffix)
        copy_payload(source,OUT/'pack'/SHARED/source.name,plan['protected'][str(source)])
    shared=manifests[SHARED]
    resources={r['file']:(manifest_paths[SHARED].parent/r['file']).read_bytes() for r in shared['facial_resources']}
    inline=resolve(shared,resources);original_inline=copy.deepcopy(inline)
    library=inline['catalog']['animation_library'];updates=[]
    groups=[library['adapters'],*[f['adapters'] for f in library['faces']]]
    old_groups=[original_inline['catalog']['animation_library']['adapters'],
                *[f['adapters'] for f in original_inline['catalog']['animation_library']['faces']]]
    for gi,adapters in enumerate(groups):
        for ai,adapter in enumerate(adapters):
            if adapter['source_outfit']==manifests[EVE]['id']:
                adapter['source_toc_sha256']=manifests[EVE]['containers']['.utoc']['sha256']
                adapter['source_bulk_bytes']=manifests[EVE]['containers']['.ucas']['bytes']
                updates.append([gi,ai])
    require(len(updates)==10,'Expected ten Eve compatibility adapters')
    manifests[SHARED],external=externalize(inline)
    require(external==resources,'Facial resources changed')
    restored=copy.deepcopy(inline);lib=restored['catalog']['animation_library']
    restore_groups=[lib['adapters'],*[f['adapters'] for f in lib['faces']]]
    for gi,ai in updates:
        for key in ('source_toc_sha256','source_bulk_bytes'):
            restore_groups[gi][ai][key]=old_groups[gi][ai][key]
    require(restored==original_inline,'Shared changes extend beyond Eve fingerprints')
    expected_eve=copy.deepcopy(before[EVE]);expected_eve['containers']=manifests[EVE]['containers']
    require(manifests[EVE]==expected_eve,'Eve metadata changed beyond container identity')
    for owner in (EVE,SHARED):
        manifest_paths[owner].write_bytes(encode(manifests[owner]))
        root=OUT/'metadata'/owner
        for relative,digest in metadata_files[owner].items():
            if root/relative!=manifest_paths[owner]:
                require(sha(root/relative)==digest,'Non-manifest metadata changed')
        run(owner+'-metadata',[DEFAULT_REPAK,'pack',root,OUT/'pack'/owner/(owner+'.pak'),'--version','V8B'])
        (OUT/'metadata-readback').mkdir(exist_ok=True)
        run(owner+'-readback',[DEFAULT_REPAK,'unpack',OUT/'pack'/owner/(owner+'.pak'),'--output',OUT/'metadata-readback'/owner])
        for p in root.rglob('*'):
            if p.is_file():require(sha(p)==sha(OUT/'metadata-readback'/owner/p.relative_to(root)),'Metadata roundtrip differs')
    require(verify_outfit(OUT/'pack'/EVE)==manifests[EVE],'Packaged Eve manifest differs')
    for owner in OTHERS:
        folder=OUT/'pack'/owner;folder.mkdir(exist_ok=True)
        for suffix in ('.pak','.utoc','.ucas'):
            target=folder/(owner+suffix);source=INSTALLED/owner/target.name
            if not target.exists():target.symlink_to(source)
            require(target.is_symlink() and target.resolve()==source.resolve(),'Other outfit provider changed')
    unchanged(plan['protected'])
    files={str(p.relative_to(OUT/'pack')):sha(p) for p in (OUT/'pack').rglob('*') if p.is_file()}
    write(OUT/'packaged.json',dict(passed=True,installed=False,files=files,shared_adapter_updates=updates,
        source_catalogs_preserved=True,facial_resources_preserved=True,plan_sha256=sha(OUT/'plan.json')))
    print('Eve container and current shared metadata packaged; independent validation remains',flush=True)


def validate(plan):
    packaged=read(OUT/'packaged.json')
    require(packaged['passed'] and packaged['plan_sha256']==sha(OUT/'plan.json'),'Packaged plan differs')
    for relative,digest in packaged['files'].items():require(sha(OUT/'pack'/relative)==digest,'Packaged file changed')
    if (OUT/'verified.json').exists():raise FileExistsError('Validation already completed')
    variants=read(BASELINE/'variants.json')
    cw=[r for r in read(CW/'validation/variants.json') if r['outfit']=='eins0fx.commanderwhite']
    require(len(cw)==1 and len(variants)==10,'Incomplete catalog fixtures')
    variants=[cw[0] if r['outfit']=='eins0fx.commanderwhite' else r for r in variants]
    write(OUT/'variants.json',variants)
    run('catalog',[ROOT/'build/host/css_animation_library_tests','--audited-peony-pack',OUT/'pack',OUT/'variants.json'])
    converter=Converter(RETOC,DEFAULT_REPAK,OUT)
    mount=OUT/'containers'
    if not mount.exists():
        converter.base_containers(GAME,mount)
        for owner in (EVE,SHARED,*OTHERS):
            for p in (OUT/'pack'/owner).iterdir():(mount/p.name).symlink_to(p.resolve())
    require((mount/(EVE+'.utoc')).resolve()==(OUT/'pack'/EVE/(EVE+'.utoc')).resolve(),'Original Eve provider shadows candidate')
    imports=plan['imports'];packages=set(Path(imports[4]).read_text().splitlines());references=set(Path(imports[5]).read_text().splitlines())
    for relative in plan['payloads']:
        if relative.endswith('.uasset'):packages.add('/Game/'+relative.removeprefix('MortalShell2/Content/').removesuffix('.uasset'))
    def collect(value):
        if isinstance(value,dict):
            for key,v in value.items():
                if key!='aliases':collect(v)
        elif isinstance(value,list):
            for v in value:collect(v)
        elif isinstance(value,str) and value.startswith('/Game/') and '.' in value:references.add(value)
    for owner in (EVE,SHARED):
        manifest,=list((OUT/'metadata'/owner).rglob('manifest.json'));collect(read(manifest))
    (OUT/'packages.txt').write_text('\n'.join(sorted(packages))+'\n')
    (OUT/'references.txt').write_text('\n'.join(sorted(references))+'\n')
    run('imports',[*imports[:2],mount,imports[3],OUT/'packages.txt',OUT/'references.txt',OUT/'imports.json'])
    require(read(OUT/'imports.json')['passed'],'Package imports failed')
    run('roundtrip',[RETOC,'to-legacy',mount,OUT/'roundtrip','--version','UE5_6','--no-parallel','--no-shaders','-f','/CSS/eins0fx/EveSB/'])
    for relative,row in plan['payloads'].items():
        p=OUT/'roundtrip'/relative
        if p.suffix=='.uasset':require(asset_info(p.read_bytes())['package']==asset_info((OUT/'stage'/relative).read_bytes())['package'],'Roundtrip identity changed')
        else:require(sha(p)==row['sha256'],'Roundtrip changed export/bulk payload: '+relative)
    # retoc also exports the container's global script table outside asset payloads.
    auxiliary=OUT/'roundtrip/scriptobjects.bin'
    require(auxiliary.is_file() and not auxiliary.is_symlink() and auxiliary.stat().st_size>0,'Missing global script table')
    require({str(p.relative_to(OUT/'roundtrip')) for p in (OUT/'roundtrip').rglob('*') if p.is_file()}==set(plan['payloads'])|{'scriptobjects.bin'},'Roundtrip payload set differs')
    write(OUT/'roundtrip-auxiliary.json',dict(path='scriptobjects.bin',sha256=sha(auxiliary),bytes=auxiliary.stat().st_size,
        source='retoc_cli/src/main.rs action_to_legacy_inner exports global script objects separately from filtered assets'))
    listing=OUT/'meshes.txt';listing.write_text('\n'.join(plan['expected_meshes'])+'\n')
    reader=plan['reader']
    run('meshes',[*reader[:2],mount,reader[3],listing,OUT/'meshes.json','--mesh-surface-digests'])
    current=read(OUT/'meshes.json');rows=[];cloth_types={'ClothingAssetCommon','ChaosClothConfig','ChaosClothSharedSimConfig'}
    for package,source in plan['expected_meshes'].items():
        a,b=read(Path(source))[package],current[package]
        old,new=[next(e for e in exports if e['Type']=='SkeletalMesh') for exports in (a,b)]
        require(old['OfflineMeshDigest']==new['OfflineMeshDigest'],'Combined mesh digest changed: '+package)
        require(normalize(old['Properties'],a,package)==normalize(new['Properties'],b,package),'Combined mesh properties changed')
        for key in ('SkeletalMaterials','ImportedBounds'):require(old[key]==new[key],'Combined '+key+' changed')
        require(empty_nanite(old['NaniteResources'])==empty_nanite(new['NaniteResources']),'Nanite data changed')
        require(normalize([e for e in a if e['Type'] in cloth_types],a,package)==normalize([e for e in b if e['Type'] in cloth_types],b,package),'Combined cloth/config changed')
        rows.append(dict(package=package,exact_decoded_preservation=True,morphs=len(new['OfflineMeshDigest']['morphs'])))
    unchanged(plan['protected'])
    write(OUT/'verified.json',dict(passed=True,installed=False,comparisons=rows,
        unchanged_payloads=len(plan['payloads'])-len(plan['replaced']),changed_payloads=plan['replaced'],
        catalog_passed=True,imports_passed=True,roundtrip_payloads=len(plan['payloads']),
        files=packaged['files'],plan_sha256=sha(OUT/'plan.json'),
        scope='Combined Eve and current shared metadata pass offline. Gameplay, performance, upgrade and stable-release gates remain.'))
    print('Combined Eve payload, catalog, import and twelve-mesh preservation checks passed',flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage',choices=('prepare','package','verify'))
    args=parser.parse_args()
    if args.stage=='prepare':prepare();return
    require(LINK.is_symlink() and LINK.resolve()==OUT,'Work link changed')
    plan=read(OUT/'plan.json');unchanged(plan['protected'])
    (package if args.stage=='package' else validate)(plan)


if __name__=='__main__':
    main()
