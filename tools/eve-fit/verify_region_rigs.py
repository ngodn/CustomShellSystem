"""Fresh-load the private regional rigs without modifying any asset."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path('/home/eins0fx/development/mods/msII')
w = root / 'CustomShellSystem/work/eve26'
output = w / 'region-rigs.json'
assert not output.exists()
source = unreal.load_asset('/Game/CSS/EveTest/SK_Waist')
assert source
library = unreal.CSSRetargetLibrary
source_rig = json.loads(library.inspect_skeleton(source.get_editor_property('skeleton')))
source_bind = json.loads(library.inspect_mesh_bind_pose(source))
rows = []
for suffix in ('Pelv', 'ThighL', 'ThighR'):
    mesh = unreal.load_asset('/Game/CSS/EveTest/SK_C' + suffix)
    assert mesh
    skeleton = mesh.get_editor_property('skeleton')
    assert skeleton.get_path_name() == f'/Game/CSS/EveTest/SKEL_C{suffix}.SKEL_C{suffix}'
    assert json.loads(library.inspect_skeleton(skeleton)) == source_rig
    assert json.loads(library.inspect_mesh_bind_pose(mesh)) == source_bind
    assert not mesh.get_editor_property('physics_asset')
    rows.append(dict(mesh=mesh.get_path_name(), bones=len(source_rig), bind_matches=True,
                     physics_asset_unassigned=True))
protected = json.loads((w / 'panel-motion-before.json').read_text())
actual = {p: hashlib.sha256((root / 'CustomShellSystem' / p).read_bytes()).hexdigest() for p in protected}
assert actual == protected
output.write_text(json.dumps(dict(scope='Saved private rig checks only, not collider or outfit acceptance.',
                                 meshes=rows, protected_assets=actual), indent=2)+'\n')
