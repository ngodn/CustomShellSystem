"""Author a private ghost companion for native eye reflection/refraction."""
import hashlib
import json
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_ghost_graph import build_native_ghost, custom, node, wire

ROOT = Path(unreal.Paths.project_dir()).resolve()
PREFIX = "/Game/CSS/UnholyGenessa/AstralRefraction1/"
REPORT = ROOT / "native-refraction-material.json"
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
if REPORT.exists() or LIB.does_directory_exist(PREFIX):
    raise FileExistsError("Refraction companion already exists")
if not unreal.SystemLibrary.get_engine_version().startswith("5.6.1-"):
    raise RuntimeError("Expected UE 5.6.1")
reference = json.loads((ROOT / "native-ghost-material.json").read_text())
protected = dict(reference["protected"])
kernel_path = Path(__file__).with_name("native-refraction-response.hlsl")
for path in (kernel_path, Path(__file__).resolve()):
    protected[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
for path, digest in protected.items():
    if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
        raise RuntimeError(f"Source changed: {path}")
noise = LIB.load_asset(reference["noise_texture"])
cube = LIB.load_asset("/Engine/EngineResources/DefaultTextureCube")
if not isinstance(noise, unreal.Texture2D) or not isinstance(cube, unreal.TextureCube):
    raise RuntimeError("Missing preview textures")
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_EyeRefraction", PREFIX, unreal.Material, unreal.MaterialFactoryNew())
if material is None:
    raise RuntimeError("Could not create refraction companion")
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
material.set_editor_property("refraction_method", unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
material.set_editor_property("automatically_set_usage_in_editor", False)
for usage in ("used_with_skeletal_mesh", "used_with_morph_targets"):
    material.set_editor_property(usage, True)


def scalar(name, value):
    return node(material, "ScalarParameter", parameter_name=name, default_value=value)


kernel = "struct CSSRefractionKernel {\n" + kernel_path.read_text() + "\n};\nCSSRefractionKernel k;\n"
response = custom(material, kernel + """
float3 direction = k.CSSRefractionDirection(Reflection.x,Reflection.y,Reflection.z,
    AngleX,AngleY,AngleZ,Pivot).xyz;
float3 e = max(ReflectionTex.Sample(ReflectionTexSampler,direction).rgb *
    (0.25 * SceneIntensity * Intensity),0.0);
float peak = max(e.r,max(e.g,e.b));
return float4(0.5*(1.0+peak+e),saturate(Opacity));
""", {"Reflection": node(material, "ReflectionVectorWS"),
    "ReflectionTex": node(material, "TextureObjectParameter", parameter_name="ReflectionMap", texture=cube),
    "AngleX": scalar("angle X", 0.), "AngleY": scalar("angle y", 0.),
    "AngleZ": scalar("angle Z", 0.), "Pivot": scalar("pivot", 0.),
    "SceneIntensity": scalar("CSS_AstralReflectionBoost", 1.),
    "Intensity": scalar("ReflectionIntensity", 1.), "Opacity": scalar("Opacity", .1)})
detail = node(material, "ComponentMask", r=True, g=True, b=True, a=False)
alpha = node(material, "ComponentMask", r=False, g=False, b=False, a=True)
wire(response, detail, "")
wire(response, alpha, "")
ghost_color, ghost_alpha = build_native_ghost(material, noise)
emission = node(material, "Multiply")
wire(detail, emission, "A")
wire(ghost_color, emission, "B")
coverage = node(material, "Multiply")
wire(alpha, coverage, "A")
wire(ghost_alpha, coverage, "B")
refraction = custom(material, kernel + """
float ior = 1.0 + k.CSSRefractionDelta(dot(Normal,Camera),IOR) * Fade;
return float4(ior,ior,ior,ior);
""", {"Normal": node(material, "VertexNormalWS"), "Camera": node(material, "CameraVectorWS"),
    "IOR": scalar("IOR", 1.33), "Fade": ghost_alpha})
ior = node(material, "ComponentMask", r=True, g=False, b=False, a=False)
wire(refraction, ior, "")
for expression, prop in ((emission, MP.MP_EMISSIVE_COLOR), (coverage, MP.MP_OPACITY), (ior, MP.MP_REFRACTION)):
    if not EDIT.connect_material_property(expression, "", prop):
        raise RuntimeError("Could not connect refraction companion")
EDIT.recompile_material(material)
if not LIB.save_loaded_asset(material, only_if_is_dirty=False):
    raise RuntimeError("Could not save refraction companion")
REPORT.write_text(json.dumps({"material": material.get_path_name(), "protected": protected,
    "source_files_unchanged": True,
    "scene_parameter": {"collection": "/Game/Sparta/Lighting/Blueprints/Deprecated/MPC_LightScenario",
                        "parameter": "Reflection Boost Intensity", "destination": "CSS_AstralReflectionBoost"},
    "scope": "Native reflection-coordinate and Fresnel refraction inputs under filtered unlit ghost shading. Native lit roughness/specular response is not retained. Runtime must copy the actual ReflectionMap and scene intensity. No render or Windows validation yet."}, indent=2) + "\n")
unreal.log("CSS_NATIVE_REFRACTION_CREATED")
