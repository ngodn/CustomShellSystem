"""Fresh-load private Bikini references and check independent section visibility."""
import hashlib
import json
import os
from pathlib import Path
import unreal

work = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
kind = os.environ.get('CSS_FIT_KIND', 'bikini')
assert kind in ('bikini', 'knit')
count = 32 if kind == 'bikini' else 28
revision = int(os.environ.get('CSS_BIKINI_FIT_REVISION', '1'))
assert revision in (1,2)
output = work / f'{kind}-verified{revision}.json'
assert not output.exists()
mesh = unreal.load_asset('/Game/CSS/EveTest/' + ('SK_BFit1' if kind == 'bikini' else 'SK_KFit2'))
assert mesh
expected = json.loads((work / f'{kind}-prepared{revision}.json').read_text())
slots = mesh.get_editor_property('materials')
assert len(slots) == count
for path, slot in zip(expected['materials'], slots, strict=True):
    assert slot.material_interface and slot.material_interface.get_path_name() == path
for prop,path in expected['references'].items():
    value = mesh.get_editor_property(prop)
    assert (value.get_path_name() if value else None) == path
assert mesh.get_editor_property('skeleton').get_path_name() == '/Game/CSS/Shared/SKEL_Base.SKEL_Base'
component = unreal.SkeletalMeshComponent()
component.set_skeletal_mesh_asset(mesh)
cases = []
groups = [('Top', range(16,19)), ('Shorts', range(19,23)), ('Shoes', range(23,29)), ('Hair', range(29,31))]
if kind == 'knit':
    groups = [('Dress', [16]), ('Shoes', range(17,23)), ('Glasses', range(23,25)), ('Hair', range(25,28))]
try:
    for name, indices in groups:
        component.show_all_material_sections(0)
        for index in indices: component.show_material_section(index,-1,False,0)
        hidden = {index for index in range(count) if not component.is_material_section_shown(index,0)}
        assert hidden == set(indices)
        component.show_all_material_sections(0)
        assert all(component.is_material_section_shown(index,0) for index in range(count))
        cases.append(dict(name=name, sections=sorted(hidden)))
finally:
    component.set_skeletal_mesh_asset(None)
protected = json.loads((work / f'{kind}-import1/protected.json').read_text())
assert all(hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest for path,digest in protected.items())
output.write_text(json.dumps(dict(materials_verified=count, references_verified=True,
    protected_unchanged=True, cases=cases,
    scope='Fresh references and transient section visibility only. Does not verify CSS controls, body occlusion, footwear pose toggling, profile persistence or game rendering.'), indent=2)+'\n')
print('BIKINI_REFERENCES_VERIFIED', flush=True)
