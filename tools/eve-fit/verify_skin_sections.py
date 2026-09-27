"""Fresh-load Skin F12 references and exercise material visibility on a transient component."""
import hashlib,json
from pathlib import Path
import unreal

w=Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
out=w/'skin-f12-sections.json';assert not out.exists()
mesh=unreal.load_asset('/Game/CSS/EveTest/SK_SFit12');assert mesh
expected=json.loads((w/'skin-f12-prepared.json').read_text())
slots=mesh.get_editor_property('materials');assert len(slots)==24
for row,slot in zip(expected['materials'],slots,strict=True):
    assert str(slot.material_slot_name)==row['name']
    assert slot.material_interface.get_path_name()==row['material']
for prop,path in expected['references'].items():
    value=mesh.get_editor_property(prop)
    assert (value.get_path_name() if value else None)==path
assert mesh.get_editor_property('skeleton').get_path_name()=='/Game/CSS/Shared/SKEL_Base.SKEL_Base'
component=unreal.SkeletalMeshComponent()
component.set_skeletal_mesh_asset(mesh)
controls=json.loads((w/'skin-lining6/controls.json').read_text())
covered=set(controls[0]['occludes_sections']);cases=[]
try:
    for control in controls:
        component.show_all_material_sections(0)
        hidden=(covered-set(control.get('occludes_sections',[])))|set(control['sections'])
        for slot in hidden:component.show_material_section(slot,-1,False,0)
        actual={slot for slot in range(24) if not component.is_material_section_shown(slot,0)}
        assert actual==hidden,(control['id'],actual,hidden)
        for slot in control['sections']:component.show_material_section(slot,-1,True,0)
        for slot in control.get('occludes_sections',[]):component.show_material_section(slot,-1,False,0)
        restored={slot for slot in range(24) if not component.is_material_section_shown(slot,0)}
        assert restored==covered
        cases.append(dict(control=control['id'],hidden=sorted(actual),restored=sorted(restored)))
finally:
    component.set_skeletal_mesh_asset(None)
protected=json.loads((w/'skin-protected-before.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==sha for p,sha in protected.items())
out.write_text(json.dumps(dict(materials_verified=24,references_verified=True,cases=cases,
    protected_unchanged=True,scope='Transient component visibility and fresh references. Not rendered visibility, CSS profile persistence, UI or game acceptance.'),indent=2)+'\n')
unreal.log('SKIN_F12_SECTIONS_VERIFIED')
