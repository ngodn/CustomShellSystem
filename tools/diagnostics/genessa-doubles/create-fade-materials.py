"""UE 5.6.1 experiment: preserve authored surface coverage under native-driven fade.

This creates private copies only. It is not the complete Astral ghost effect.
CSS_AstralOpacity is intended to follow the clone's native GlobalOpacity.
"""
import hashlib
import json
from pathlib import Path
import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
EDIT = unreal.MaterialEditingLibrary
LIB = unreal.EditorAssetLibrary
MP = unreal.MaterialProperty
SOURCE = "/Game/CSS/UnholyGenessa/"
TARGET = "/Game/CSS/Astral/Prototype1/"
parent_file = ROOT / "material-parents.json"
PARENTS = (json.loads(parent_file.read_text()) if parent_file.exists() else
           [SOURCE + path for path in ("Mat1/M_Trim", "Mat10/M_Trim", "Mat12/M_Metal",
                                       "Fabric02/M_Fabric", "Fabric02/M_Silk")])
if not PARENTS or len(PARENTS) != len(set(PARENTS)):
    raise ValueError("Expected a nonempty unique material list")
if any(not path.startswith("/Game/CSS/") or ".." in path.split("/") for path in PARENTS):
    raise ValueError("Expected CSS source material paths")
version = unreal.SystemLibrary.get_engine_version()
if not version.startswith("5.6.1-"):
    raise RuntimeError(f"Expected UE 5.6.1, got {version}")
manifest = json.loads((ROOT / "source-manifest.json").read_text())


def check_sources():
    for row in manifest:
        for path in (Path(row["source"]), ROOT / "Content" / row["relative"]):
            if hashlib.sha256(path.read_bytes()).hexdigest() != row["sha256"]:
                raise RuntimeError(f"Source changed: {path}")


def node(material, kind, **properties):
    value = EDIT.create_material_expression(material, getattr(unreal, "MaterialExpression" + kind))
    if value is None:
        raise RuntimeError(f"Could not create {kind}")
    for name, prop in properties.items():
        value.set_editor_property(name, prop)
    return value


def wire(source, target, pin, output=""):
    if not EDIT.connect_material_expressions(source, output, target, pin):
        raise RuntimeError(f"Could not connect {source} to {target}.{pin}")


def output(material, source, property_, pin=""):
    if not EDIT.connect_material_property(source, pin, property_):
        raise RuntimeError(f"Could not connect material output {property_}")


check_sources()
report = {"engine": version, "materials": [], "native_effect_complete": False}
for source_path in PARENTS:
    relative = source_path.removeprefix(SOURCE) if source_path.startswith(SOURCE) else source_path.removeprefix("/Game/")
    destination = TARGET + relative.replace("/", "_")
    if LIB.does_asset_exist(destination):
        raise RuntimeError(f"Refusing to overwrite {destination}")
    material = LIB.duplicate_asset(source_path, destination)
    if material is None:
        raise RuntimeError(f"Could not duplicate {relative}")
    original_mode = material.get_editor_property("blend_mode")
    coverage = None
    coverage_pin = ""
    if original_mode == unreal.BlendMode.BLEND_MASKED:
        mask = EDIT.get_material_property_input_node(material, MP.MP_OPACITY_MASK)
        if mask is None:
            raise RuntimeError(f"Masked material has no coverage expression: {relative}")
        mask_pin = EDIT.get_material_property_input_node_output_name(material, MP.MP_OPACITY_MASK)
        coverage = node(material, "Step", const_y=material.get_editor_property("opacity_mask_clip_value"))
        wire(mask, coverage, "X", mask_pin)
    elif original_mode == unreal.BlendMode.BLEND_TRANSLUCENT:
        coverage = EDIT.get_material_property_input_node(material, MP.MP_OPACITY)
        coverage_pin = EDIT.get_material_property_input_node_output_name(material, MP.MP_OPACITY)
        if coverage is None:
            raise RuntimeError(f"Translucent material has no opacity expression: {relative}")
    elif original_mode != unreal.BlendMode.BLEND_OPAQUE:
        raise RuntimeError(f"Unsupported blend mode: {original_mode}")

    fade = node(material, "ScalarParameter", parameter_name="CSS_AstralOpacity", default_value=0.)
    clamped = node(material, "Saturate")
    wire(fade, clamped, "")
    product = node(material, "Multiply", const_a=1.)
    if coverage is not None:
        wire(coverage, product, "A", coverage_pin)
    wire(clamped, product, "B")
    output(material, product, MP.MP_OPACITY)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("dither_opacity_mask", False)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    for name in ("used_with_skeletal_mesh", "used_with_morph_targets", "used_with_clothing"):
        material.set_editor_property(name, True)
    EDIT.recompile_material(material)
    if not LIB.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {destination}")
    report["materials"].append({"source": source_path, "target": destination,
                               "original_blend": str(original_mode),
                               "fade_default": EDIT.get_material_default_scalar_parameter_value(material, "CSS_AstralOpacity")})
check_sources()
report["source_files_unchanged"] = True
report["limitations"] = ["Graph authoring only until rendered and cooked.",
                         "Native ghost shading, body and eyes are not adapted here.",
                         "Changing blend mode requires visual and performance validation."]
(ROOT / "fade-materials.json").write_text(json.dumps(report, indent=2) + "\n")
unreal.log("CSS_ASTRAL_FADE_MATERIALS_CREATED")
