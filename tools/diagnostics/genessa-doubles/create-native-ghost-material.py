"""Create an isolated UE 5.6.1 material from the native Astral reconstruction.

Requires CSS_ASTRAL_NOISE_PNG from the offline native texture export. Uses the
test-only UnholyGenessa namespace accepted by the existing compilation helper.
"""
import hashlib
import json
import os
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_ghost_graph import build_native_ghost


ROOT = Path(unreal.Paths.project_dir()).resolve()
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
TARGET = "/Game/CSS/UnholyGenessa/AstralNative1/"
report_path = ROOT / "native-ghost-material.json"
if report_path.exists() or LIB.does_directory_exist(TARGET):
    raise FileExistsError("Native ghost experiment already exists")
version = unreal.SystemLibrary.get_engine_version()
if not version.startswith("5.6.1-"):
    raise RuntimeError(f"Expected UE 5.6.1, got {version}")
protected = json.loads((ROOT / "render-sources.json").read_text())
noise_png = Path(os.environ["CSS_ASTRAL_NOISE_PNG"]).resolve(strict=True)
kernel_path = Path(__file__).with_name("native-ghost-response.hlsl")
protected[str(noise_png)] = hashlib.sha256(noise_png.read_bytes()).hexdigest()
protected[str(kernel_path)] = hashlib.sha256(kernel_path.read_bytes()).hexdigest()
graph_path = Path(__file__).with_name("native_ghost_graph.py").resolve()
protected[str(graph_path)] = hashlib.sha256(graph_path.read_bytes()).hexdigest()


def check_sources():
    for path, digest in protected.items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


check_sources()
task = unreal.AssetImportTask()
for name, value in {"filename": str(noise_png), "destination_path": TARGET,
                    "destination_name": "T_NativeNoisePreview", "automated": True,
                    "replace_existing": False, "save": True}.items():
    task.set_editor_property(name, value)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
paths = task.get_editor_property("imported_object_paths")
if len(paths) != 1:
    raise RuntimeError(f"Expected one noise texture: {paths}")
noise_texture = LIB.load_asset(paths[0])
if not isinstance(noise_texture, unreal.Texture2D):
    raise TypeError("Imported native noise is not a Texture2D")
# Native Texture2D does not override SRGB or Filter; exact engine defaults apply.
noise_texture.set_editor_property("srgb", True)
noise_texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
noise_texture.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
if not LIB.save_loaded_asset(noise_texture):
    raise RuntimeError("Could not save preview noise texture")
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_NativeGhost", TARGET, unreal.Material, unreal.MaterialFactoryNew())
if material is None:
    raise RuntimeError("Could not create native ghost material")
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
material.set_editor_property("automatically_set_usage_in_editor", False)
for usage in ("used_with_skeletal_mesh", "used_with_morph_targets", "used_with_clothing"):
    material.set_editor_property(usage, True)

color, alpha = build_native_ghost(material, noise_texture)
for expression, prop in ((color, MP.MP_EMISSIVE_COLOR), (alpha, MP.MP_OPACITY)):
    if not EDIT.connect_material_property(expression, "", prop):
        raise RuntimeError(f"Could not connect {prop}")
EDIT.recompile_material(material)
if not LIB.save_loaded_asset(material, only_if_is_dirty=False):
    raise RuntimeError("Could not save native ghost graph")
instances = []
for form, value in (("Faithful", 0.), ("Stray", 1.)):
    instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "MI_" + form, TARGET, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    if instance is None:
        raise RuntimeError(f"Could not create {form}")
    EDIT.set_material_instance_parent(instance, material)
    for name, scalar in (("CSS_AstralCorrupted", value), ("CSS_AstralOpacity", 1.)):
        # UE 5.6.1's setter returns false even after writing; verify the stored override.
        EDIT.set_material_instance_scalar_parameter_value(instance, name, scalar)
        overrides = {str(row.get_editor_property("parameter_info").get_editor_property("name")):
                     row.get_editor_property("parameter_value")
                     for row in instance.get_editor_property("scalar_parameter_values")}
        if overrides.get(name) != scalar:
            raise RuntimeError(f"Could not set {form} {name}")
    EDIT.update_material_instance(instance)
    if not LIB.save_loaded_asset(instance, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {form}")
    instances.append(instance.get_path_name())
check_sources()
report = {"engine": version, "material": material.get_path_name(), "instances": instances,
          "noise_texture": noise_texture.get_path_name(), "protected": protected,
          "source_files_unchanged": True,
          "limits": ["Graph creation only; compilation and rendering are separate.",
                     "Noise preview uses the decoded cooked top mip; native mip chain is not preserved.",
                     "No outfit coverage, customization or runtime binding is implemented here."]}
report_path.write_text(json.dumps(report, indent=2) + "\n")
unreal.log("CSS_ASTRAL_NATIVE_GRAPH_CREATED")
