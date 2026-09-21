"""Create or freshly verify an isolated V44 mesh with saved body collision.

Uses UE 5.6.1 editor Python. No production mesh, shared skeleton or game writes.
"""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT/'CustomShellSystem/work/grip-grounding-v1'
CONTENT = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
OUT = Path(os.environ['CSS_BODY_BIND_DIR']).resolve()
assert OUT.parent == WORK.resolve() and not OUT.exists()
OUT.mkdir()
create = os.environ.get('CSS_BODY_BIND_CREATE') == '1'
source_package = '/Game/CSSAuthoring/DiagnosticReferences/SK_B2GameReferenceMetadata_V2'
package = '/Game/CSSAuthoring/DiagnosticReferences/SK_B2PhysicsBound_V1'
physics_package = '/Game/CSSAuthoring/DiagnosticReferences/PA_B2BodyFit_V3'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
asset_file = lambda p: CONTENT/(p.removeprefix('/Game/')+'.uasset')
write = lambda n,v: (OUT/n).write_text(json.dumps(v, indent=2)+'\n')
prior = json.loads((WORK/'b2-body-hand-query-v2/report.json').read_text())
assert prior['passed']
protected = prior['protected_hashes']
protected[str(asset_file(physics_package))] = prior['fitted_asset_sha256']
if not create:
    previous = json.loads((WORK/'b2-body-binding-v1/report.json').read_text())
    assert previous['passed']
    protected[str(asset_file(package))] = previous['mesh_sha256']
assert all(digest(Path(p)) == h for p,h in protected.items())
source = unreal.load_asset(source_package)
physics = unreal.load_asset(physics_package)
assert source and physics and source.get_editor_property('physics_asset') is None
if create:
    assert not unreal.EditorAssetLibrary.does_asset_exist(package)
    mesh = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
        package.rsplit('/', 1)[1], package.rsplit('/', 1)[0], source)
    assert mesh
    mesh.set_editor_property('physics_asset', physics)
else:
    mesh = unreal.load_asset(package)
assert mesh and mesh.get_editor_property('physics_asset') == physics
for name in ('skeleton', 'post_process_anim_blueprint', 'shadow_physics_asset'):
    assert mesh.get_editor_property(name) == source.get_editor_property(name), name
bind = json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))
assert len(bind) == 379
assert bind == json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(source))
assert [str(m.get_name()) for m in mesh.get_editor_property('morph_targets')] == [
    str(m.get_name()) for m in source.get_editor_property('morph_targets')]
assert len(mesh.get_editor_property('morph_targets')) == 22
slots, old_slots = mesh.get_editor_property('materials'), source.get_editor_property('materials')
assert len(slots) == len(old_slots) == 30
for a,b in zip(slots, old_slots, strict=True):
    for name in ('material_slot_name', 'imported_material_slot_name', 'material_interface'):
        assert a.get_editor_property(name) == b.get_editor_property(name), name
if create:
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
queries = json.loads((WORK/'b2-body-hand-fit-v1/queries.json').read_text())
actual = json.loads(unreal.CSSPhysicsProbeLibrary.probe_body_queries(mesh, physics, json.dumps(queries), True))
write('bound-queries.json', actual)
assert actual['world_destroyed'] and actual['uses_mesh_binding'] and len(actual['phases']) == 2
for phase in actual['phases']:
    assert phase['fitted'] and phase['body_count'] == phase['valid_bodies'] == 22
    assert phase['effective_asset'] == physics.get_path_name()
    for expected, observed in zip(queries, phase['queries'], strict=True):
        assert expected['id'] == observed['id'] and expected['expected'] == observed['hit'], observed
        if 'probe_point' in expected:
            assert observed['point_body'].lower() == expected['probe_bone'].lower()
            distance = observed['point_distance']
            assert 0 <= distance <= .001 if expected['expected'] else distance > 100, observed
assert all(digest(Path(p)) == h for p,h in protected.items())
write('report.json', dict(passed=True, created=create, package=package, physics_package=physics_package,
    mesh_sha256=digest(asset_file(package)), protected_hashes=protected,
    raw_bones=379, materials=30, morphs=22, ray_observations=len(queries)*2,
    point_distance_observations=sum('probe_point' in q for q in queries)*2,
    implicit_binding_and_recreation_verified=True, scope=__doc__, game_verified=False))
print('CSS_BODY_BIND_PASS', package, flush=True)
