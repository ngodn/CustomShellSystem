"""Verify four final mesh paths without mounting competing original Eve meshes."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from check_current_facial_preservation import normalize
from check_import_cooked_mesh import sha
from verify_blackpearl_surface_assembly import empty_nanite

ROOT = Path(__file__).resolve().parents[2]
BASE = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014/tangent-bake').resolve()
WORK = BASE.with_name('eve-tangent-final')
sys.path.insert(0,str(ROOT/'tools'))
from css import GAME
from css_convert import Converter,DEFAULT_REPAK,asset_info


def read(path):
    return json.loads(path.read_text())


def main():
    if read(WORK/'cook/exit.json')!=dict(returncode=0,changed_inputs=[]):
        raise ValueError('Final cook failed')
    plan,proof = read(WORK/'plan.json'),read(WORK/'reload.json')
    protected = dict(plan['protected'])
    protected[str(Path(__file__))] = sha(Path(__file__))
    prior = BASE/'pack3/meshes.json'
    protected[str(prior)] = sha(prior)
    if not read(BASE/'pack3/verified.json')['passed']:
        raise ValueError('Prior assembly is unverified')
    if any(sha(Path(p))!=h for p,h in protected.items()):
        raise ValueError('Input changed')
    out = WORK/'pack'
    out.mkdir(exist_ok=False)
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc',DEFAULT_REPAK,out)
    stage = out/'stage'
    cooked = WORK/'cook/project/Saved/Cooked/Windows/CSSShared/Content'
    payloads = {}
    for package in plan['mapping'].values():
        header = cooked/(package.removeprefix('/Game/')+'.uasset')
        if asset_info(header.read_bytes())['package']!=package:
            raise ValueError('Unexpected cooked package identity')
        for path in header.parent.glob(header.stem+'.*'):
            if path.suffix not in ('.uasset','.uexp','.ubulk','.uptnl'):
                raise ValueError('Unexpected payload')
            relative = Path('MortalShell2/Content')/path.relative_to(cooked)
            target = stage/relative
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(path,target)
            payloads[str(relative)] = sha(path)
    container = out/'container'
    container.mkdir()
    toc = container/'CSS_EveTangentFinal_P.utoc'
    converter.run(converter.retoc,'to-zen',stage,toc,'--version','UE5_6','--no-parallel')
    converter.run(converter.retoc,'verify',toc)
    # Only direct dependencies enter this reader fixture. Original mesh providers
    # stay out so they cannot conceal a missing or incorrectly mounted candidate.
    deps = sorted({p for refs in proof['dependencies'].values() for p in refs if p.startswith('/Game/CSS/eins0fx/EveSB/')})
    if not deps or set(deps)&set(plan['mapping'].values()):
        raise ValueError('Unexpected direct dependency set')
    original_mount = BASE/'pack3/containers'
    converter.run(converter.retoc,'to-legacy',original_mount,out/'reader-dependencies',
        '--version','UE5_6','--no-parallel','--no-shaders',*[v for p in deps for v in ('-f',p.removeprefix('/Game'))])
    exported = [asset_info(p.read_bytes())['package'] for p in (out/'reader-dependencies').rglob('*.uasset')]
    if sorted(exported)!=deps:
        raise ValueError('Reader fixture contains unexpected packages')
    fixture = out/'fixture'
    fixture.mkdir()
    converter.run(converter.retoc,'to-zen',out/'reader-dependencies',fixture/'CSS_ReaderDependencies_P.utoc',
        '--version','UE5_6','--no-parallel')
    mount = out/'containers'
    converter.base_containers(GAME,mount)
    for p in protected:
        path=Path(p)
        if path.name.startswith('CSS_SharedAssets_P') and path.suffix in ('.utoc','.ucas'):
            (mount/path.name).symlink_to(path)
    for folder in (fixture,container):
        for path in folder.iterdir():
            (mount/path.name).symlink_to(path)
            protected[str(path)]=sha(path)
    converter.run(converter.retoc,'to-legacy',mount,out/'roundtrip','--version','UE5_6','--no-parallel',
        '--no-shaders',*[v for p in plan['mapping'].values() for v in ('-f',p.removeprefix('/Game'))])
    for relative,h in payloads.items():
        path=out/'roundtrip'/relative
        if path.suffix=='.uasset':
            if asset_info(path.read_bytes())['package']!=asset_info((stage/relative).read_bytes())['package']:
                raise ValueError('Roundtrip changed identity')
        elif sha(path)!=h:
            raise ValueError('Roundtrip changed export payload')
    old_command = read(BASE/'pack3/decoded-command.json')['command']
    listing = out/'meshes.txt'
    listing.write_text('\n'.join(plan['mapping'].values())+'\n')
    command = [*old_command[:2],str(mount),old_command[3],str(listing),str(out/'meshes.json'),'--mesh-surface-digests']
    (out/'readback-command.json').write_text(json.dumps(command,indent=2)+'\n')
    with (out/'readback.log').open('w') as log:
        result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
    (out/'readback-exit.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')
    result.check_returncode()
    original,current = read(prior),read(out/'meshes.json')
    comparisons=[]
    for old,new in plan['mapping'].items():
        a=next(e for e in original[old] if e['Type']=='SkeletalMesh')
        b=next(e for e in current[new] if e['Type']=='SkeletalMesh')
        if a['OfflineMeshDigest']!=b['OfflineMeshDigest']:
            raise ValueError('Final-path cook changed decoded render, bind or morph data')
        if normalize(a['Properties'],original[old],old)!=normalize(b['Properties'],current[new],new):
            raise ValueError('Final-path cook changed mesh properties')
        for field in ('SkeletalMaterials','ImportedBounds'):
            if a[field]!=b[field]:
                raise ValueError('Final-path cook changed '+field)
        if empty_nanite(a['NaniteResources'])!=empty_nanite(b['NaniteResources']):
            raise ValueError('Final-path cook changed empty Nanite resource')
        comparisons.append(dict(source=old,target=new,exact_decoded_preservation=True,
            vertices=b['OfflineMeshDigest']['lods'][0]['vertices'],morphs=len(b['OfflineMeshDigest']['morphs'])))
    changed=[p for p,h in protected.items() if sha(Path(p))!=h]
    (out/'verified.json').write_text(json.dumps(dict(passed=not changed,comparisons=comparisons,
        payloads=payloads,reader_fixture_packages=deps,protected=protected,changed_inputs=changed,
        scope='Final-path four-mesh cook/roundtrip and exact independent decoded comparison with verified Skin Suit/Bikini private pairs. Reader fixture has only direct dependencies and is not a playable/release package. Full Eve integration and gameplay remain.'),indent=2)+'\n')
    if changed:
        raise ValueError('Inputs changed')
    print('Final-path Skin Suit/Bikini pairs match the verified private cook')


if __name__=='__main__':
    main()
