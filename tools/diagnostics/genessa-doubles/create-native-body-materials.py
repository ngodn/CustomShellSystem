"""Create ghost body parents for the audited non-VT native Uber permutations.

The ghost detail uses albedo, not the source's environment lighting or blood
effects. Masked coverage follows the extracted native alpha calculation.
"""
import hashlib
import json
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_ghost_graph import build_native_ghost, custom, node, wire

ROOT = Path(unreal.Paths.project_dir()).resolve()
PREFIX = "/Game/CSS/UnholyGenessa/AstralBody1/"
REPORT = ROOT / "native-body-materials.json"
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
if REPORT.exists() or LIB.does_directory_exist(PREFIX):
    raise FileExistsError("Body companions already exist")
if not unreal.SystemLibrary.get_engine_version().startswith("5.6.1-"):
    raise RuntimeError("Expected UE 5.6.1")
reference = json.loads((ROOT / "native-ghost-material.json").read_text())
protected = dict(reference["protected"])
protected[str(Path(__file__).resolve())] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()


def check_sources():
    for path, digest in protected.items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


check_sources()
noise = LIB.load_asset(reference["noise_texture"])
texture = LIB.load_asset("/Engine/EngineResources/WhiteSquareTexture")
if not isinstance(noise, unreal.Texture2D) or not isinstance(texture, unreal.Texture2D):
    raise RuntimeError("Missing body fixture textures")
materials = []
for masked in (False, True):
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_Uber_" + ("masked" if masked else "opaque"), PREFIX,
        unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError("Could not create body companion")
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
    material.set_editor_property("automatically_set_usage_in_editor", False)
    for usage in ("used_with_skeletal_mesh", "used_with_morph_targets"):
        material.set_editor_property(usage, True)
    albedo = node(material, "TextureSampleParameter2D",
        parameter_name="BaseColorMap  non VT", texture=texture)
    native_color, native_alpha = build_native_ghost(material, noise)
    detail = custom(material, "return float4(0.5 * (1.0 + saturate(Albedo)), 1.0);",
                    {"Albedo": albedo})
    rgb = node(material, "ComponentMask", r=True, g=True, b=True, a=False)
    wire(detail, rgb, "")
    emission = node(material, "Multiply")
    wire(native_color, emission, "A")
    wire(rgb, emission, "B")
    coverage = native_alpha
    if masked:
        alpha = node(material, "ComponentMask", r=False, g=False, b=False, a=True)
        wire(albedo, alpha, "", "RGBA")
        # The native graph replicates sampled alpha before its channel dot.
        # Do not interpret a red channel selector as the texture's red channel.
        strength = node(material, "ScalarParameter", parameter_name="Opacity Strength", default_value=1.)
        channel = node(material, "VectorParameter", parameter_name="OpacityMask_Channel",
                       default_value=unreal.LinearColor(0., 0., 0., 1.))
        channel4 = node(material, "AppendVector")
        wire(channel, channel4, "A")
        wire(channel, channel4, "B", "A")
        clip = node(material, "ScalarParameter", parameter_name="CSS_AstralClipValue", default_value=.3333)
        mask = custom(material,
            "float a = Strength * (Alpha * Channel.r + Alpha * Channel.g + Alpha * Channel.b + Alpha * Channel.a);\n"
            "float visible = (a - ClipValue) < 0.0 ? 0.0 : 1.0;\nreturn float4(visible,visible,visible,visible);",
            {"Alpha": alpha, "Strength": strength, "Channel": channel4, "ClipValue": clip})
        scalar_mask = node(material, "ComponentMask", r=True, g=False, b=False, a=False)
        wire(mask, scalar_mask, "")
        coverage = node(material, "Multiply")
        wire(native_alpha, coverage, "A")
        wire(scalar_mask, coverage, "B")
    for expression, prop in ((emission, MP.MP_EMISSIVE_COLOR), (coverage, MP.MP_OPACITY)):
        if not EDIT.connect_material_property(expression, "", prop):
            raise RuntimeError("Could not connect body companion")
    EDIT.recompile_material(material)
    if not LIB.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError("Could not save body companion")
    materials.append({"kind": "masked" if masked else "opaque", "path": material.get_path_name()})
check_sources()
REPORT.write_text(json.dumps({"materials": materials, "protected": protected,
    "source_files_unchanged": True,
    "required_switches": {"USE VIRTUAL TEXTURES": False, "UseBaseColorMap": True,
        "BaseColorAdjust": False, "Opacity from Color Map": True, "Use Opacity Dither": False},
    "composition": "native_filter_albedo",
    "scope": "Ghost albedo and native alpha cutouts. Source lighting, reflection boost, blood/frost and death displacement are not reproduced. Bind effective source texture and clip threshold at runtime; do not use the white fixture default in game."}, indent=2) + "\n")
unreal.log("CSS_NATIVE_BODY_CREATED")
