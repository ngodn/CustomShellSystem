"""Cook both culling modes without mutating material flags in a running game."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
manifest = json.loads((root / "shared-materials.json").read_text())
output = root / "shared-material-variants.json"
if output.exists():
    raise FileExistsError(output)
if not unreal.SystemLibrary.get_engine_version().startswith("5.6.1-"):
    raise RuntimeError("Expected UE 5.6.1")
prefix = "/Game/CSS/SharedAssets/Astral/Materials/"
rows = []
for spec in manifest["materials"]:
    if not spec["material"].startswith(prefix):
        raise ValueError("Unexpected companion namespace")
    parent = unreal.EditorAssetLibrary.load_asset(spec["material"])
    if not isinstance(parent, unreal.Material):
        raise TypeError("Expected companion parent")
    base_two_sided = parent.get_editor_property("two_sided")
    name = "MI_" + spec["name"] + ("_SingleSided" if base_two_sided else "_TwoSided")
    if unreal.EditorAssetLibrary.does_asset_exist(prefix + name):
        raise FileExistsError(name)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, prefix, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(material, parent)
    overrides = material.get_editor_property("base_property_overrides")
    overrides.set_editor_property("override_two_sided", True)
    overrides.set_editor_property("two_sided", not base_two_sided)
    material.set_editor_property("base_property_overrides", overrides)
    unreal.MaterialEditingLibrary.update_material_instance(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError("Could not save culling variant")
    rows.append({"parent": spec["material"], "parent_two_sided": base_two_sided,
                 "variant": material.get_path_name(), "variant_two_sided": not base_two_sided})
with output.open("x") as stream:
    stream.write(json.dumps({"materials": rows}, indent=2) + "\n")
print(f"Created {len(rows)} opposite-side variants")
