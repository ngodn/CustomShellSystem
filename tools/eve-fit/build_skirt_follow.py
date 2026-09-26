"""Create a private rig that preserves animation-follow before cloth dynamics."""
import hashlib
import json
from pathlib import Path
import sys
import unreal

root=Path('/home/eins0fx/development/mods/msII')
work=root/'CustomShellSystem/work/eve26'
helpers=root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools'
sys.path.insert(0,str(helpers))
from build_controlrig_chain_probe import Graph,Pin

package='/Game/CSS/EveTest/CR_HolidayFollow'
report=work/'skirt-follow-build.json'
assert not report.exists() and not unreal.EditorAssetLibrary.does_asset_exist(package)
mapping=json.loads((work/'skirt-follow.json').read_text())['drivers']
mesh=unreal.load_asset('/Game/CSS/EveTest/SK_HolidayBones')
assert mesh
rig=unreal.AssetToolsHelpers.get_asset_tools().create_asset(package.rsplit('/',1)[1],package.rsplit('/',1)[0],unreal.ControlRigBlueprint,unreal.ControlRigBlueprintFactory())
assert rig
rig.set_auto_vm_recompile(False)
imported=rig.get_hierarchy_controller().import_bones_from_skeletal_mesh(mesh,'None')
assert len(imported)==379
rig.set_preview_mesh(mesh,False)
g=Graph(rig)
execution=Pin(g.unit('RigUnit_BeginExecution')+'.ExecuteContext')
for target,driver in mapping.items():
    def get(name,initial):
        node=g.unit('RigUnit_GetTransform',Space='GlobalSpace',bInitial=initial)
        g.value(node+'.Item.Type','Bone');g.value(node+'.Item.Name',name)
        return Pin(node+'.Transform')
    offset=g.math('TransformMakeRelative',output='Local',Global=get(target,True),Parent=get(driver,True))
    value=g.math('TransformMakeAbsolute',output='Global',Local=offset,Parent=get(driver,False))
    node=g.unit('RigUnit_SetTransform',Space='GlobalSpace',bInitial=False,bPropagateToChildren=False,Weight=1)
    g.value(node+'.Item.Type','Bone');g.value(node+'.Item.Name',target)
    g.value(node+'.Value',value)
    g.link(execution,node+'.ExecuteContext')
    execution=Pin(node+'.ExecuteContext')
rig.recompile_vm()
assert unreal.EditorAssetLibrary.save_loaded_asset(rig,only_if_is_dirty=False)
protected=json.loads((work/'motion-protected.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in protected.items())
report.write_text(json.dumps({'package':package,'nodes':len(g.controller.get_graph().get_nodes()),
    'drivers':mapping,'imported_private_mesh_bones':len(imported),
    'scope':'Private animation-follow rig only. No spring, collision or production graph integration. Shared skeleton unchanged.'},indent=2)+'\n')
print('SKIRT_FOLLOW_BUILT')
