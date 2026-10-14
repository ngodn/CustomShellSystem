"""UE 5.6.1: bind and fresh-load the private Black Pearl normal/facial pair."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
WORK = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014/blackpearl-ordered').resolve()
if Path(unreal.Paths.project_dir()).resolve() != WORK/'editor':
    raise ValueError('Use the private ordered Black Pearl project')
MODE = os.environ['CSS_SURFACE_BIND_MODE']
if MODE not in ('create', 'reload'):
    raise ValueError('Invalid operation')
plan = json.loads((WORK/'binding-plan.json').read_text())
output = WORK/('bindings-'+MODE+'.json')
if output.exists():
    raise FileExistsError(output)


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def disk(package):
    return WORK/'editor/Content'/(package.split('.')[0].removeprefix('/Game/')+'.uasset')


def morphs(mesh):
    return sorted(str(m.get_name()) for m in mesh.get_editor_property('morph_targets'))


def bindings(mesh):
    fields = ('skeleton', 'physics_asset', 'shadow_physics_asset', 'post_process_anim_blueprint')
    result = {}
    for field in fields:
        obj = mesh.get_editor_property(field)
        result[field] = obj.get_path_name() if obj else None
    result['materials'] = []
    for slot in mesh.get_editor_property('materials'):
        material = slot.get_editor_property('material_interface')
        result['materials'].append(dict(slot=str(slot.get_editor_property('material_slot_name')),
            material=material.get_path_name() if material else None))
    return result


protected = dict(plan['protected'])
for package, row in plan['sources'].items():
    protected[row['path']] = row['sha256']
    protected[str(disk(package))] = row['sha256']
if any(sha(Path(p)) != digest for p,digest in protected.items()):
    raise ValueError('Protected source changed')
reference = unreal.load_asset(plan['reference_mesh'])
candidate = unreal.load_asset(plan['candidate_mesh'])
if not reference or not candidate:
    raise ValueError('Missing reference or verified candidate')
reference_bindings = bindings(reference)
for key in ('skeleton', 'physics_asset', 'post_process_anim_blueprint'):
    if reference_bindings[key] != plan['references'][key]:
        raise ValueError('Loaded editor reference differs: '+key)
if reference_bindings['materials'] != [{k:r[k] for k in ('slot','material')} for r in plan['references']['materials']]:
    raise ValueError('Loaded reference material differs')
before_bind = unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(candidate)
shared = reference.get_editor_property('skeleton')
shared_before = [unreal.CSSRetargetLibrary.inspect_skeleton(shared),
    unreal.CSSRetargetLibrary.inspect_virtual_bones(shared), unreal.CSSRetargetLibrary.inspect_sockets(shared)]
if len(json.loads(shared_before[1])) != 9:
    raise ValueError('Shared virtual bone inventory changed')
if MODE == 'create':
    for key in ('normal_target','facial_target'):
        if unreal.EditorAssetLibrary.does_asset_exist(plan[key]):
            raise FileExistsError(plan[key])
    normal = unreal.EditorAssetLibrary.duplicate_asset(plan['candidate_mesh'],plan['normal_target'])
    if not normal:
        raise ValueError('Cannot duplicate verified geometry')
    normal.call_method('SetSkeleton', args=(shared,))
    for field in ('materials','physics_asset','shadow_physics_asset','post_process_anim_blueprint',
                  'lod_info','positive_bounds_extension','negative_bounds_extension'):
        normal.set_editor_property(field,reference.get_editor_property(field))
    if not unreal.EditorAssetLibrary.save_loaded_asset(normal,False):
        raise ValueError('Cannot save bound normal mesh')
    facial = unreal.EditorAssetLibrary.duplicate_asset(plan['normal_target'],plan['facial_target'])
    result = unreal.CSSFacialLibrary.add_expression_shapes(facial,plan['facial_input'])
    if not result.startswith('PASS:'):
        raise ValueError(result)
    if not unreal.EditorAssetLibrary.save_loaded_asset(facial,False):
        raise ValueError('Cannot save facial mesh')
else:
    created = json.loads((WORK/'bindings-create.json').read_text())
    for package,digest in created['outputs'].items():
        if sha(disk(package)) != digest:
            raise ValueError('Saved assembly changed')
    normal = unreal.load_asset(plan['normal_target'])
    facial = unreal.load_asset(plan['facial_target'])
    result = 'Fresh-process reload'
if not normal or not facial:
    raise ValueError('Cannot load assembly')
for mesh in (normal,facial):
    if bindings(mesh) != reference_bindings:
        raise ValueError('Bound references changed')
    if unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh) != before_bind:
        raise ValueError('Raw mesh bind pose changed')
    for field in ('lod_info','positive_bounds_extension','negative_bounds_extension'):
        if mesh.get_editor_property(field) != reference.get_editor_property(field):
            raise ValueError('Accepted mesh setting changed: '+field)
expected = {r['name'] for r in json.loads(Path(plan['facial_input']).read_text())['morph_targets']}
if (len(morphs(normal)) != 22 or len(expected) != 115 or
        set(morphs(facial)) != set(morphs(normal)) | expected):
    raise ValueError('Morph inventory differs')
comparison = unreal.CSSFacialLibrary.verify_expression_shapes(normal,facial,plan['facial_input'])
preservation = unreal.CSSFacialLibrary.verify_expression_preservation(normal,facial)
if not comparison.startswith('PASS:') or not preservation.startswith('PASS:'):
    raise ValueError(str((comparison,preservation)))
shared_after = [unreal.CSSRetargetLibrary.inspect_skeleton(shared),
    unreal.CSSRetargetLibrary.inspect_virtual_bones(shared), unreal.CSSRetargetLibrary.inspect_sockets(shared)]
if shared_before != shared_after:
    raise ValueError('Shared skeleton changed')
if any(sha(Path(p)) != digest for p,digest in protected.items()):
    raise ValueError('Protected source changed')
output.write_text(json.dumps(dict(passed=True,mode=MODE,result=result,
    comparison=comparison,preservation=preservation,bindings=reference_bindings,
    raw_bones=len(json.loads(before_bind)),virtual_bones=9,
    normal_morphs=len(morphs(normal)),facial_morphs=len(morphs(facial)),
    outputs={p:sha(disk(p)) for p in (plan['normal_target'],plan['facial_target'])},
    sources_unchanged=True, scope='Private editor assembly and expression preservation. Cooked binding/geometry checks and gameplay remain.'),indent=2)+'\n')
print('BLACKPEARL_BINDINGS_PASS',MODE,comparison,preservation,flush=True)
