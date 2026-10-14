"""Package/read back baked tangents and compare them with the prior paired cook."""
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
from build_eve_tangent_bake import ROOT, WORK

sys.path.insert(0,str(ROOT/'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bounds-repaired',action='store_true')
    args = parser.parse_args()
    cook = WORK/('cook3' if args.bounds_repaired else 'cook')
    if read(cook/'exit.json')!=dict(returncode=0,changed_inputs=[]):
        raise ValueError('Cook did not pass')
    out = WORK/('pack3' if args.bounds_repaired else 'pack')
    out.mkdir(exist_ok=False)
    rows,protected = [],{str(Path(__file__)):sha(Path(__file__))}
    for variant in ('skinsuit','bikini'):
        base = WORK.parent/(variant+'-ordered')
        old_command = read(base/'pack/skin-command.json')['command']
        dotnet,reader,mappings = old_command[0],old_command[1],old_command[3]
        for kind in ('normal','facial'):
            proof_path = WORK/(variant+'-'+kind+('-3' if args.bounds_repaired and kind=='normal' else '-2')+'-verified.json')
            proof = read(proof_path)
            protected[str(proof_path)] = sha(proof_path)
            expected = Path(read(base/'candidate.mesh.receipt.json')['source']) if kind=='normal' else base/'binding-pack/expected-facial.mesh.json'
            rows.append(dict(label=variant+'-'+kind,package=proof['target'],old=proof['source'],
                base=base,kind=kind,expected=expected))
        plan = read(base/'binding-plan.json')
        protected.update(plan['protected'])
    packages = [r['package'] for r in rows]
    if packages!=read(cook/'shipping-allowlist.json'):
        raise ValueError('Shipping set differs from verified editor outputs')
    cooked = cook/'project/Saved/Cooked/Windows/CSSShared/Content'
    stage,payloads = out/'stage',{}
    for package in packages:
        file = cooked/(package.removeprefix('/Game/')+'.uasset')
        if asset_info(file.read_bytes())['package']!=package:
            raise ValueError('Unexpected cooked package identity')
        for source in file.parent.glob(file.stem+'.*'):
            if source.suffix not in ('.uasset','.uexp','.ubulk','.uptnl'):
                raise ValueError('Unexpected cooked payload')
            relative = Path('MortalShell2/Content')/source.relative_to(cooked)
            target = stage/relative
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(source,target)
            payloads[str(relative)] = sha(source)
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc',DEFAULT_REPAK,out)
    container = out/'container'
    container.mkdir()
    toc = container/'CSS_EveTangentBakeTrial_P.utoc'
    converter.run(converter.retoc,'to-zen',stage,toc,'--version','UE5_6','--no-parallel')
    converter.run(converter.retoc,'verify',toc)
    mount = out/'containers'
    converter.base_containers(GAME,mount)
    for p,digest in list(protected.items()):
        path = Path(p)
        if path.suffix in ('.utoc','.ucas'):
            if sha(path)!=digest:
                raise ValueError('Installed dependency changed')
            (mount/path.name).symlink_to(path)
    for path in container.iterdir():
        (mount/path.name).symlink_to(path)
        protected[str(path)] = sha(path)
    filters = [v for p in packages for v in ('-f',p.removeprefix('/Game'))]
    converter.run(converter.retoc,'to-legacy',mount,out/'roundtrip','--version','UE5_6','--no-parallel','--no-shaders',*filters)
    for relative,digest in payloads.items():
        file = out/'roundtrip'/relative
        if file.suffix=='.uasset':
            if asset_info(file.read_bytes())['package']!=asset_info((stage/relative).read_bytes())['package']:
                raise ValueError('Roundtrip changed package name')
        elif sha(file)!=digest:
            raise ValueError('Roundtrip changed cooked payload')
    (out/'roundtrip.json').write_text(json.dumps(dict(passed=True,payloads=payloads,packages=packages),indent=2)+'\n')
    asset_reader = ROOT/'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'
    for p in (Path(reader),Path(mappings),asset_reader,converter.retoc):
        protected[str(p)] = sha(p)
    results = []
    def run(label,command,env=None):
        env = env or {}
        (out/(label+'-command.json')).write_text(json.dumps(dict(command=command,environment=env),indent=2)+'\n')
        with (out/(label+'.log')).open('w') as log:
            result = subprocess.run(command,env={**os.environ,**env},stdout=log,stderr=subprocess.STDOUT)
        row = dict(mode=label,returncode=result.returncode)
        results.append(row)
        (out/(label+'-exit.json')).write_text(json.dumps(row)+'\n')
        print(row,flush=True)
        result.check_returncode()
    listing = out/'meshes.txt'
    listing.write_text('\n'.join(packages)+'\n')
    run('decoded',[dotnet,str(asset_reader),str(mount),mappings,str(listing),str(out/'meshes.json'),'--mesh-surface-digests'])
    current = read(out/'meshes.json')
    comparisons = []
    for row in rows:
        label,package = row['label'],row['package']
        folder = out/label
        folder.mkdir()
        obj = package+'.'+package.rsplit('/',1)[1]
        protected[str(row['expected'])] = sha(row['expected'])
        for mode,env in (('skin',{'CSS_SKIN_AUDIT':'1','CSS_SKIN_SURFACE':'1'}),('morph',{'CSS_MORPH_AUDIT':'1'})):
            run(label+'-'+mode,[dotnet,reader,str(mount),mappings,obj,str(folder/mode)],env)
        for name,arguments in (
            ('geometry',['--source',str(row['expected']),'--skin',str(folder/'skin'),'--morph',str(folder/'morph')]),
            ('surface',['--source',str(row['expected']),'--readback',str(folder/'skin')]),
            ('basis',['--before',str(row['base']/'binding-pack'/row['kind']/'skin'),'--after',str(folder/'skin')]),
            ('fallback',['--readback',str(folder/'skin')])):
            tool = {'geometry':'check_import_cooked_mesh.py','surface':'check_import_cooked_surface.py',
                'basis':'compare_eve_tangent_bake.py','fallback':'classify_eve_tangent_fallbacks.py'}[name]
            run(label+'-'+name,[sys.executable,'-B',str(ROOT/'tools/diagnostics'/tool),*arguments,'--output',str(folder/(name+'.json'))])
        fallback = read(folder/'fallback.json')
        if not fallback['unit_lengths_within_bound'] or fallback['unexpected_nonorthogonal_vertices']:
            raise ValueError('Tangent quality gate failed')
        old_path = row['base']/'binding-pack/meshes.json'
        protected[str(old_path)] = sha(old_path)
        old_exports,new_exports = read(old_path)[row['old']],current[package]
        old_mesh = next(e for e in old_exports if e['Type']=='SkeletalMesh')
        mesh = next(e for e in new_exports if e['Type']=='SkeletalMesh')
        def properties(m,exports,path):
            return normalize({k:v for k,v in m['Properties'].items() if k!='MorphTargets'},exports,path)
        expected = copy.deepcopy(properties(old_mesh,old_exports,row['old']))
        if not expected['LODInfo'][0]['BuildSettings']['bRecomputeTangents']:
            raise ValueError('Unexpected old build setting')
        expected['LODInfo'][0]['BuildSettings']['bRecomputeTangents'] = False
        if properties(mesh,new_exports,package)!=expected:
            raise ValueError('Unexpected property changes beyond tangent storage')
        for field in ('SkeletalMaterials','ImportedBounds'):
            if mesh[field]!=old_mesh[field]:
                raise ValueError('Changed '+field)
        for field in ('reference_skeleton_sha256','morphs'):
            if mesh['OfflineMeshDigest'][field]!=old_mesh['OfflineMeshDigest'][field]:
                raise ValueError('Changed decoded '+field)
        comparisons.append(dict(package=package,properties_preserved_except_recompute_tangents=True,
            materials_bounds_bind_and_morphs_preserved=True,basis=read(folder/'basis.json'),
            known_collapsed_uv_fallbacks=fallback['zero_uv_unit_x_fallback_vertices']))
    changed = [p for p,h in protected.items() if sha(Path(p))!=h]
    passed = not changed
    (out/'verified.json').write_text(json.dumps(dict(passed=passed,results=results,comparisons=comparisons,
        protected=protected,changed_inputs=changed,
        scope='Private four-mesh Windows cook and independent source/basis/binding preservation. Final paths, integrated package and gameplay/performance remain.'),indent=2)+'\n')
    if not passed:
        raise SystemExit(1)


if __name__=='__main__':
    main()
