"""Move verified companion graphs into the shared package without outfit imports."""
import hashlib
import json
from pathlib import Path
import struct
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
PREFIX = "/Game/CSS/SharedAssets/Astral/Materials"
REPORT = ROOT / "shared-materials.json"
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
if REPORT.exists() or LIB.does_directory_exist(PREFIX):
    raise FileExistsError("Shared material output already exists")
if not unreal.SystemLibrary.get_engine_version().startswith("5.6.1-"):
    raise RuntimeError("Expected UE 5.6.1")
plan = json.loads((ROOT / "shared-material-plan.json").read_text())
protected = plan["protected"]


def check_sources():
    for path, digest in protected.items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


check_sources()
neutral = {}
for name, rgb, srgb, compression in (
    ("Color", (255, 255, 255), True, unreal.TextureCompressionSettings.TC_DEFAULT),
    ("Linear", (255, 255, 255), False, unreal.TextureCompressionSettings.TC_DEFAULT),
    ("Normal", (128, 128, 255), False, unreal.TextureCompressionSettings.TC_NORMALMAP),
):
    path = ROOT / ("neutral-" + name + ".tga")
    with path.open("xb") as stream:
        stream.write(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 24, 32))
        stream.write(bytes(reversed(rgb)) * 16)
    task = unreal.AssetImportTask()
    for key, value in {"filename": str(path), "destination_path": PREFIX,
                       "destination_name": "T_Neutral" + name, "automated": True,
                       "save": False, "replace_existing": False}.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    outputs = task.get_editor_property("imported_object_paths")
    if len(outputs) != 1:
        raise RuntimeError("Neutral texture import failed")
    texture = LIB.load_asset(outputs[0])
    texture.set_editor_property("srgb", srgb)
    texture.set_editor_property("compression_settings", compression)
    if not LIB.save_loaded_asset(texture, only_if_is_dirty=False):
        raise RuntimeError("Neutral texture save failed")
    neutral[name] = texture
cube = LIB.load_asset("/Engine/EngineResources/DefaultTextureCube")
if not isinstance(cube, unreal.TextureCube):
    raise RuntimeError("Missing engine cube")
fixed = {
    "CSS_AstralNoise": "/Game/Sparta/FX/Textures/Noises/BnW/T_noise_0017.T_noise_0017",
    "CSS_EyeNoise": "/Game/Sparta/FX/Textures/Noises/BnW/T_noise_0082.T_noise_0082",
}
rows = []
for spec in plan["materials"]:
    target = PREFIX + "/" + spec["name"]
    material = LIB.duplicate_asset(spec["candidate"], target)
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Cannot duplicate {spec['candidate']}")
    material.set_editor_property("used_with_clothing", True)
    expressions = unreal.CSSMaterialLibrary.get_expressions(material)
    if not expressions:
        raise RuntimeError("Empty graph")
    textures = {}
    for expression in expressions:
        if not isinstance(expression, unreal.MaterialExpressionTextureBase):
            continue
        if not isinstance(expression, unreal.MaterialExpressionTextureSampleParameter):
            raise RuntimeError(f"Unparameterized texture in {target}: {expression}")
        parameter = str(expression.get_editor_property("parameter_name"))
        source = expression.get_editor_property("texture")
        sampler = expression.get_editor_property("sampler_type")
        if source is None:
            raise RuntimeError(f"Missing source texture: {parameter}")
        if isinstance(source, unreal.TextureCube):
            replacement = cube
        elif not isinstance(source, unreal.Texture2D):
            raise RuntimeError(f"Unsupported texture type: {source}")
        elif sampler == unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL:
            replacement = neutral["Normal"]
        elif sampler == unreal.MaterialSamplerType.SAMPLERTYPE_COLOR:
            replacement = neutral["Color"]
        elif sampler in (unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
                         unreal.MaterialSamplerType.SAMPLERTYPE_MASKS):
            replacement = neutral["Linear"]
        else:
            raise RuntimeError(f"Unsupported sampler: {sampler}")
        binding = {"parameter": parameter, "candidate_default": source.get_path_name(),
                   "placeholder": replacement.get_path_name(), "sampler": str(sampler),
                   "binding": "fixed_game_texture" if parameter in fixed else "source_parameter"}
        if parameter in fixed:
            binding["game_texture"] = fixed[parameter]
        if parameter in textures and textures[parameter] != binding:
            raise RuntimeError(f"Conflicting parameter {parameter}")
        textures[parameter] = binding
        expression.set_editor_property("texture", replacement)
    if "CSS_AstralNoise" not in textures:
        raise RuntimeError("Companion lost native noise input")
    # Instances are invisible until their source textures and fade are bound.
    opacity = [x for x in expressions if isinstance(x, unreal.MaterialExpressionScalarParameter)
               and str(x.get_editor_property("parameter_name")) == "CSS_AstralOpacity"]
    if len(opacity) != 1:
        raise RuntimeError("Expected one ghost fade parameter")
    opacity[0].set_editor_property("default_value", 0.)
    EDIT.recompile_material(material)
    if not LIB.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Cannot save {target}")
    rows.append({**spec, "material": material.get_path_name(), "textures": list(textures.values())})

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous([PREFIX], force_rescan=True)
options = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False,
    include_hard_management_references=False)
for row in rows:
    dependencies = sorted(str(x) for x in registry.get_dependencies(row["material"].split(".")[0], options))
    unexpected = [x for x in dependencies if not x.startswith(("/Engine/", "/Script/Engine", PREFIX + "/"))]
    if unexpected:
        raise RuntimeError(f"External package dependencies: {unexpected}")
    row["dependencies"] = dependencies
check_sources()
report = {"engine": unreal.SystemLibrary.get_engine_version(), "materials": rows,
          "neutral_textures": [x.get_path_name() for x in neutral.values()],
          "protected": protected, "source_files_unchanged": True,
          "scope": "Shared authoring assets only. Required texture bindings must be applied before visibility. Windows cook and game validation remain pending."}
with REPORT.open("x") as stream:
    stream.write(json.dumps(report, indent=2) + "\n")
print(f"Created {len(rows)} shared material parents without outfit package dependencies")
