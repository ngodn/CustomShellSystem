"""UE 5.6.1: save the verified Black Pearl pair at its existing release paths."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
WORK = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014/blackpearl-final').resolve()
if Path(unreal.Paths.project_dir()).resolve() != WORK/'editor':
    raise ValueError('Use the private final-path project')
mode = os.environ['CSS_SURFACE_FINAL_MODE']
if mode not in ('create','recover','reload'):
    raise ValueError('Invalid operation')
plan = json.loads((WORK/'plan.json').read_text())
report = WORK/(mode+'.json')
if report.exists():
    raise FileExistsError(report)


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def disk(package):
    return WORK/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')


protected = dict(plan['protected'])
for package,row in plan['assets'].items():
    protected[str(disk(package))] = row['sha256']
if any(sha(Path(p)) != digest for p,digest in protected.items()):
    raise ValueError('Protected input changed')
if mode=='reload':
    saved = WORK/('recover.json' if (WORK/'recover.json').exists() else 'create.json')
    for package,digest in json.loads(saved.read_text())['outputs'].items():
        if sha(disk(package))!=digest:
            raise ValueError('Saved final-path mesh changed')
outputs,comparisons = {},[]
for index,(old,new) in enumerate(plan['mapping'].items()):
    source = unreal.load_asset(old)
    if not source:
        raise ValueError('Missing source')
    if mode=='create':
        if unreal.EditorAssetLibrary.does_asset_exist(new):
            raise FileExistsError(new)
        target = unreal.EditorAssetLibrary.duplicate_asset(old,new)
        if not target or not unreal.EditorAssetLibrary.save_loaded_asset(target,False):
            raise ValueError('Cannot save final-path mesh')
    else:
        target = unreal.load_asset(new)
    if not target:
        raise ValueError('Cannot reload final-path mesh')
    for field in ('materials','skeleton','physics_asset','shadow_physics_asset',
                  'post_process_anim_blueprint','lod_info','positive_bounds_extension','negative_bounds_extension'):
        if source.get_editor_property(field)!=target.get_editor_property(field):
            raise ValueError('Final-path move changed '+field)
    if unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(source)!=unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(target):
        raise ValueError('Final-path move changed bind pose')
    # The verifier requires its diagnostic namespace; this clone is excluded from cooking.
    diagnostic = unreal.EditorAssetLibrary.duplicate_asset(new,'/Game/CSS/AnimLab/SK_Face_FinalPathCheck'+str(index)+mode)
    result = unreal.CSSFacialLibrary.verify_expression_preservation(source,diagnostic)
    if not result.startswith('PASS:') or '0 added morphs excluded' not in result:
        raise ValueError('Final-path geometry preservation failed: '+result)
    comparisons.append(dict(source=old,target=new,result=result))
    outputs[new] = sha(disk(new))
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
registry.scan_files_synchronous([str(disk(p)) for p in outputs],True)
registry.wait_for_completion()
options = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,
    include_soft_package_references=True,include_searchable_names=False,
    include_hard_management_references=False,include_soft_management_references=False)
dependencies = {p:sorted(str(d) for d in registry.get_dependencies(p,options)) for p in outputs}
for refs in dependencies.values():
    if any(p.startswith(('/Game/CSSAuthoring/','/Game/CSS/AnimLab/')) for p in refs):
        raise ValueError('Final mesh still references a diagnostic package')
if any(sha(Path(p))!=digest for p,digest in protected.items()):
    raise ValueError('Protected source changed')
report.write_text(json.dumps(dict(passed=True,mode=mode,outputs=outputs,comparisons=comparisons,
    dependencies=dependencies,scope='Final-path editor duplication and exact mesh-description/built-section preservation. Windows cook/readback and integrated outfit package remain.'),indent=2)+'\n')
print('BLACKPEARL_FINAL_PATHS_PASS',mode,flush=True)
