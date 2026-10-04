"""Author private Genessa eye/smoke reconstructions and their ghost companions."""
import hashlib
import json
import os
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_ghost_graph import build_native_ghost, custom, node, wire

ROOT = Path(unreal.Paths.project_dir()).resolve()
PREFIX = "/Game/CSS/UnholyGenessa/AstralEyes1/"
REPORT = ROOT / "native-eye-materials.json"
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
if REPORT.exists() or LIB.does_directory_exist(PREFIX):
    raise FileExistsError("Native eye experiment already exists")
if not unreal.SystemLibrary.get_engine_version().startswith("5.6.1-"):
    raise RuntimeError("Expected UE 5.6.1")
reference = json.loads((ROOT / "native-ghost-material.json").read_text())
protected = dict(reference["protected"])
kernel_path = Path(__file__).with_name("native-eye-response.hlsl").resolve()
noise_png = Path(os.environ["CSS_ASTRAL_EYE_NOISE_PNG"]).resolve(strict=True)
for path in (kernel_path, noise_png, Path(__file__).resolve()):
    protected[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()


def check_sources():
    for path, digest in protected.items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


check_sources()
task = unreal.AssetImportTask()
for name, value in {"filename": str(noise_png), "destination_path": PREFIX,
                    "destination_name": "T_EyeNoisePreview", "automated": True,
                    "replace_existing": False, "save": True}.items():
    task.set_editor_property(name, value)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
paths = task.get_editor_property("imported_object_paths")
if len(paths) != 1:
    raise RuntimeError("Expected one imported eye noise texture")
noise = LIB.load_asset(paths[0])
if not isinstance(noise, unreal.Texture2D):
    raise TypeError("Eye noise is not a Texture2D")
noise.set_editor_property("srgb", True)
noise.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
noise.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
if not LIB.save_loaded_asset(noise):
    raise RuntimeError("Could not save preview eye noise")
ghost_noise = LIB.load_asset(reference["noise_texture"])
if not isinstance(ghost_noise, unreal.Texture2D):
    raise TypeError("Missing native ghost preview noise")
kernel = "struct CSSGenessaKernel {\n" + kernel_path.read_text() + "\n};\nCSSGenessaKernel k;\n"
materials = []
for kind in ("eye", "smoke"):
    for role in ("source", "ghost"):
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            f"M_{kind}_{role}", PREFIX, unreal.Material, unreal.MaterialFactoryNew())
        if material is None:
            raise RuntimeError("Could not create eye fixture material")
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE
            if kind == "eye" and role == "source" else unreal.BlendMode.BLEND_TRANSLUCENT)
        material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
        material.set_editor_property("automatically_set_usage_in_editor", False)
        for usage in ("used_with_skeletal_mesh", "used_with_morph_targets"):
            material.set_editor_property(usage, True)

        def scalar(name, value):
            return node(material, "ScalarParameter", parameter_name=name, default_value=value)

        inputs = {
            "NormalWS": node(material, "VertexNormalWS"),
            "CameraVectorWS": node(material, "CameraVectorWS"),
            "Color": node(material, "VectorParameter", parameter_name="Color",
                          default_value=unreal.LinearColor(.598958, .7813, 1., 1.)),
            "Intensity": scalar("Intensity", 1.5 if kind == "eye" else 2.),
            "Adaptation": node(material, "EyeAdaptation"),
        }
        if kind == "eye":
            inputs["Power"] = scalar("Fresnel Power", 6.)
            code = "return k.CSSGenessaEyeResponse(dot(NormalWS, CameraVectorWS), Power, Color.r, Color.g, Color.b, Intensity, Adaptation);"
        else:
            selected_time = node(material, "LinearInterpolate")
            wire(node(material, "Time", ignore_pause=False), selected_time, "A")
            wire(scalar("CSS_EyeFixedTime", 0.), selected_time, "B")
            wire(scalar("CSS_EyeUseFixedTime", 0.), selected_time, "Alpha")
            inputs.update({"UV": node(material, "TextureCoordinate", coordinate_index=0),
                "VertexColor": node(material, "VertexColor"), "Seconds": selected_time,
                "Tiling": scalar("Tiling", 1.), "MaskPower": scalar("Mask Power", 2.),
                "Opacity": scalar("Opacity Multiplier", 2.5),
                "NoiseTex": node(material, "TextureObjectParameter", parameter_name="CSS_EyeNoise", texture=noise)})
            code = """
float2 uv = k.CSSGenessaSmokeUV(UV.xy, VertexColor.g, Tiling, Seconds);
float a = NoiseTex.SampleBias(NoiseTexSampler, uv, View.MaterialTextureMipBias).r;
float b = NoiseTex.SampleBias(NoiseTexSampler, uv + 0.01f, View.MaterialTextureMipBias).r;
float c = NoiseTex.SampleBias(NoiseTexSampler, uv - 0.02f, View.MaterialTextureMipBias).r;
return k.CSSGenessaSmokeResponse(a,b,c,UV.y,VertexColor.r,dot(NormalWS,CameraVectorWS),
    MaskPower,Opacity,Color.r,Color.g,Color.b,Intensity,Adaptation);
"""
        source = custom(material, kernel + code, inputs)
        color = node(material, "ComponentMask", r=True, g=True, b=True, a=False)
        alpha = node(material, "ComponentMask", r=False, g=False, b=False, a=True)
        wire(source, color, "")
        wire(source, alpha, "")
        if role == "ghost":
            native_color, native_alpha = build_native_ghost(material, ghost_noise)
            factor = custom(material, "float3 e=max(Emission,0.0); float peak=max(e.r,max(e.g,e.b)); return float4(0.5*(1.0+peak+e),1.0);",
                            {"Emission": color})
            factor_rgb = node(material, "ComponentMask", r=True, g=True, b=True, a=False)
            wire(factor, factor_rgb, "")
            emission = node(material, "Multiply")
            wire(factor_rgb, emission, "A")
            wire(native_color, emission, "B")
            coverage = node(material, "Multiply")
            wire(alpha, coverage, "A")
            wire(native_alpha, coverage, "B")
        else:
            emission, coverage = color, alpha
        if not EDIT.connect_material_property(emission, "", MP.MP_EMISSIVE_COLOR):
            raise RuntimeError("Could not connect eye emission")
        if kind == "smoke" or role == "ghost":
            if not EDIT.connect_material_property(coverage, "", MP.MP_OPACITY):
                raise RuntimeError("Could not connect eye coverage")
        EDIT.recompile_material(material)
        if not LIB.save_loaded_asset(material, only_if_is_dirty=False):
            raise RuntimeError("Could not save eye fixture")
        materials.append({"kind": kind, "role": role, "path": material.get_path_name()})
check_sources()
REPORT.write_text(json.dumps({"materials": materials, "protected": protected,
    "source_files_unchanged": True, "eye_noise": noise.get_path_name(),
    "scope": "Reconstructed eye/smoke inputs and filtered ghost companions. Preview PNG noise lacks native compressed mip chain. Full rendering and runtime remain unverified."}, indent=2) + "\n")
unreal.log("CSS_NATIVE_EYES_CREATED")
