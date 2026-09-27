"""Fresh-load private Bikini references and check independent section visibility."""
import hashlib
import json
from pathlib import Path
import unreal

work = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
output = work / 'bikini-verified1.json'
assert not output.exists()
mesh = unreal.load_asset('/Game/CSS/EveTest/SK_BFit1')
assert mesh
expected = json.loads((work / 'bikini-prepared1.json').read_text())
slots = mesh.get_editor_property('materials')
assert len(slots) == 32
for path, slot in zip(expected['materials'], slots, strict=True):
    assert slot.material_interface and slot.material_interface.get_path_name() == path
for prop,path in expected['references'].items():
    value = mesh.get_editor_property(prop)
    assert (value.get_path_name() if value else None) == path
assert mesh.get_editor_property('skeleton').get_path_name() == '/Game/CSS/Shared/SKEL_Base.SKEL_Base'
component = unreal.SkeletalMeshComponent()
component.set_skeletal_mesh_asset(mesh)
cases = []
try:
    for name, indices in [('Top', range(16,19)), ('Shorts', range(19,23)),
            ('Shoes', range(23,29)), ('Hair', range(29,31))]:
        component.show_all_material_sections(0)
        for index in indices: component.show_material_section(index,-1,False,0)
        hidden = {index for index in range(32) if not component.is_material_section_shown(index,0)}
        assert hidden == set(indices)
        component.show_all_material_sections(0)
        assert all(component.is_material_section_shown(index,0) for index in range(32))
        cases.append(dict(name=name, sections=sorted(hidden)))
finally:
    component.set_skeletal_mesh_asset(None)
protected = json.loads((work / 'bikini-import1/protected.json').read_text())
assert all(hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest for path,digest in protected.items())
output.write_text(json.dumps(dict(materials_verified=32, references_verified=True,
    protected_unchanged=True, cases=cases,
    scope='Fresh references and transient section visibility only. Does not verify CSS controls, body occlusion, footwear pose toggling, profile persistence or game rendering.'), indent=2)+'\n')
print('BIKINI_REFERENCES_VERIFIED', flush=True)
