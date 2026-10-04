"""Add the audited native constant-color graph to an isolated shared asset stage."""
import hashlib
import json
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "diagnostics/genessa-doubles"))
from native_ghost_graph import build_native_ghost, custom, node, wire

ROOT = Path(unreal.Paths.project_dir()).resolve()
PREFIX = "/Game/CSS/SharedAssets/Astral/Materials/"
SOURCE = "/Game/Sparta/Characters/Enemies/Brigands/HordeShieldBrigand/Art/Mesh/clothdriver"
NAME = "M_ClothDriver"
REPORT = ROOT / "clothdriver-material.json"
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
if REPORT.exists() or LIB.does_asset_exist(PREFIX + NAME):
    raise FileExistsError("Cloth driver companion already exists")
if not unreal.SystemLibrary.get_engine_version().startswith("5.6.1-"):
    raise RuntimeError("Expected UE 5.6.1")

readback = json.loads((ROOT / "clothdriver-source.json").read_text())
entries = [entry for exports in readback.values() for entry in exports
           if entry["Type"] == "Material" and entry["Package"] == SOURCE]
if len(entries) != 1 or entries[0]["Properties"]["StateId"] != "E234C3DC-40580992-053C46BE-0D935230":
    raise ValueError("Expected the independently decoded clothdriver revision")
cached = entries[0]["CachedExpressionData"]
infos = cached["RuntimeEntries[1]"]["ParameterInfoSet"]
if (len(infos) != 1 or infos[0]["Name"] != "Param" or infos[0]["Index"] != -1
        or not infos[0]["Association"].endswith("::GlobalParameter")
        or cached["TextureValues"] or cached["StaticSwitchValues"]
        or cached["VectorPrimitiveDataIndexValues"] != [-1]):
    raise ValueError("Cloth driver uniform contract changed")
color = cached["VectorValues"][0]
manifest = json.loads((ROOT / "shared-materials.json").read_text())
variants = json.loads((ROOT / "shared-material-variants.json").read_text())
if any(row["name"] == NAME for row in manifest["materials"]):
    raise ValueError("Companion is already in the manifest")
protected = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
             for path in (ROOT / "Content/CSS/SharedAssets").rglob("*.uasset")}

noise = LIB.load_asset(PREFIX + "T_NeutralLinear")
if not isinstance(noise, unreal.Texture2D):
    raise RuntimeError("Shared linear placeholder is missing")
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    NAME, PREFIX, unreal.Material, unreal.MaterialFactoryNew())
if not isinstance(material, unreal.Material):
    raise RuntimeError("Could not create cloth driver companion")
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
material.set_editor_property("automatically_set_usage_in_editor", False)
for usage in ("used_with_skeletal_mesh", "used_with_morph_targets", "used_with_clothing"):
    material.set_editor_property(usage, True)
albedo = node(material, "VectorParameter", parameter_name="Param",
              default_value=unreal.LinearColor(*(color[channel] for channel in "RGBA")))
native_color, native_alpha = build_native_ghost(material, noise)
# Native DXIL saturates Param.rgb into base color. Use the same ghost detail
# composition as the Uber body companion, without a synthetic color texture.
detail = custom(material, "return float4(0.5 * (1.0 + saturate(Albedo)), 1.0);", {"Albedo": albedo})
rgb = node(material, "ComponentMask", r=True, g=True, b=True, a=False)
wire(detail, rgb, "")
emission = node(material, "Multiply")
wire(native_color, emission, "A")
wire(rgb, emission, "B")
for expression, prop in ((emission, unreal.MaterialProperty.MP_EMISSIVE_COLOR),
                         (native_alpha, unreal.MaterialProperty.MP_OPACITY)):
    if not EDIT.connect_material_property(expression, "", prop):
        raise RuntimeError("Could not connect cloth driver companion")
EDIT.recompile_material(material)
if not LIB.save_loaded_asset(material, only_if_is_dirty=False):
    raise RuntimeError("Could not save cloth driver companion")

variant = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "MI_" + NAME + "_TwoSided", PREFIX,
    unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
if not isinstance(variant, unreal.MaterialInstanceConstant):
    raise RuntimeError("Could not create culling variant")
EDIT.set_material_instance_parent(variant, material)
overrides = variant.get_editor_property("base_property_overrides")
overrides.set_editor_property("override_two_sided", True)
overrides.set_editor_property("two_sided", True)
variant.set_editor_property("base_property_overrides", overrides)
EDIT.update_material_instance(variant)
if not LIB.save_loaded_asset(variant, only_if_is_dirty=False):
    raise RuntimeError("Could not save culling variant")

for path, digest in protected.items():
    if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
        raise RuntimeError(f"Existing shared asset changed: {path}")
manifest["materials"].append({"name": NAME, "source_parent": SOURCE,
    "material": material.get_path_name(), "textures": [{
        "parameter": "CSS_AstralNoise", "placeholder": noise.get_path_name(),
        "binding": "fixed_game_texture",
        "game_texture": "/Game/Sparta/FX/Textures/Noises/BnW/T_noise_0017.T_noise_0017"}]})
variants["materials"].append({"parent": material.get_path_name(), "parent_two_sided": False,
    "variant": variant.get_path_name(), "variant_two_sided": True})
for filename, value in (("shared-materials.json", manifest), ("shared-material-variants.json", variants)):
    (ROOT / filename).write_text(json.dumps(value, indent=2) + "\n")
REPORT.write_text(json.dumps({"source": SOURCE, "state_id": entries[0]["Properties"]["StateId"],
    "color_default": color, "protected": protected, "existing_assets_unchanged": True,
    "material": material.get_path_name(), "variant": variant.get_path_name(),
    "scope": "Authored companion only. Windows cook, uniform copy and live rendering remain unverified."},
    indent=2) + "\n")
print("Created the constant-color companion and its culling variant")
