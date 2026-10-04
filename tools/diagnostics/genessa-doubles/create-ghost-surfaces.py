"""Preserve authored surface inputs while adding native ghost emission and fade.

Private authoring experiment. Native body/eye adapters and runtime assignment
are separate work; this script never edits the source materials.
"""
import hashlib
import json
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_ghost_graph import build_native_ghost, node, wire


ROOT = Path(unreal.Paths.project_dir()).resolve()
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
TARGET = "/Game/CSS/UnholyGenessa/AstralSurface1/Parents/"
REPORT = ROOT / "ghost-surfaces.json"
if REPORT.exists() or LIB.does_directory_exist(TARGET):
    raise FileExistsError("Ghost surface experiment already exists")
version = unreal.SystemLibrary.get_engine_version()
if not version.startswith("5.6.1-"):
    raise RuntimeError(f"Expected UE 5.6.1, got {version}")
parents = json.loads((ROOT / "material-parents.json").read_text())
if not parents or len(parents) != len(set(parents)):
    raise ValueError("Expected unique source material parents")
if any(not p.startswith("/Game/CSS/") or ".." in p.split("/") for p in parents):
    raise ValueError("Unexpected source parent path")
native = json.loads((ROOT / "native-ghost-material.json").read_text())
protected = native["protected"]
manifest = json.loads((ROOT / "source-manifest.json").read_text())
for row in manifest:
    protected[row["source"]] = row["sha256"]
    protected[str(ROOT / "Content" / row["relative"])] = row["sha256"]
for name in ("native_ghost_graph.py", "native-ghost-response.hlsl"):
    path = Path(__file__).with_name(name).resolve()
    protected[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()


def check_sources():
    for path, digest in protected.items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


check_sources()
noise = LIB.load_asset(native["noise_texture"])
if not isinstance(noise, unreal.Texture2D):
    raise TypeError("Expected the native noise preview texture")
rows = []
for source in parents:
    target = TARGET + source.removeprefix("/Game/CSS/").replace("/", "_")
    if LIB.does_asset_exist(target):
        raise FileExistsError(target)
    material = LIB.duplicate_asset(source, target)
    if not isinstance(material, unreal.Material):
        raise TypeError(f"Expected Material: {source}")
    shading = material.get_editor_property("shading_model")
    if shading not in (unreal.MaterialShadingModel.MSM_DEFAULT_LIT, unreal.MaterialShadingModel.MSM_UNLIT):
        raise ValueError(f"Surface adapter needs explicit support for {shading}: {source}")
    preserved = {}
    for name in ("MP_BASE_COLOR", "MP_METALLIC", "MP_SPECULAR", "MP_ROUGHNESS",
                 "MP_NORMAL", "MP_AMBIENT_OCCLUSION", "MP_WORLD_POSITION_OFFSET"):
        property_ = getattr(MP, name)
        expression = EDIT.get_material_property_input_node(material, property_)
        preserved[name] = (expression, EDIT.get_material_property_input_node_output_name(material, property_))
    original_emission = EDIT.get_material_property_input_node(material, MP.MP_EMISSIVE_COLOR)
    emission_pin = EDIT.get_material_property_input_node_output_name(material, MP.MP_EMISSIVE_COLOR)
    mode = material.get_editor_property("blend_mode")
    coverage = None
    pin = ""
    if mode == unreal.BlendMode.BLEND_MASKED:
        source_coverage = EDIT.get_material_property_input_node(material, MP.MP_OPACITY_MASK)
        if source_coverage is None:
            raise RuntimeError(f"No original mask expression: {source}")
        source_pin = EDIT.get_material_property_input_node_output_name(material, MP.MP_OPACITY_MASK)
        coverage = node(material, "Step", const_y=material.get_editor_property("opacity_mask_clip_value"))
        wire(source_coverage, coverage, "X", source_pin)
    elif mode == unreal.BlendMode.BLEND_TRANSLUCENT:
        coverage = EDIT.get_material_property_input_node(material, MP.MP_OPACITY)
        pin = EDIT.get_material_property_input_node_output_name(material, MP.MP_OPACITY)
        if coverage is None:
            raise RuntimeError(f"No original opacity expression: {source}")
    elif mode != unreal.BlendMode.BLEND_OPAQUE:
        raise ValueError(f"Unsupported blend mode for {source}: {mode}")
    color, ghost_alpha = build_native_ghost(material, noise)
    alpha = node(material, "Multiply", const_a=1.)
    if coverage is not None:
        wire(coverage, alpha, "A", pin)
    wire(ghost_alpha, alpha, "B")
    emission = node(material, "Add", const_a=0.)
    if original_emission is not None:
        wire(original_emission, emission, "A", emission_pin)
    wire(color, emission, "B")
    for expression, property_ in ((emission, MP.MP_EMISSIVE_COLOR), (alpha, MP.MP_OPACITY)):
        if not EDIT.connect_material_property(expression, "", property_):
            raise RuntimeError(f"Could not set {property_}")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    for name, (expression, output) in preserved.items():
        property_ = getattr(MP, name)
        if (EDIT.get_material_property_input_node(material, property_) != expression or
                EDIT.get_material_property_input_node_output_name(material, property_) != output):
            raise RuntimeError(f"Source connection changed: {source} {name}")
    material.set_editor_property("dither_opacity_mask", False)
    material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
    material.set_editor_property("automatically_set_usage_in_editor", False)
    for usage in ("used_with_skeletal_mesh", "used_with_morph_targets", "used_with_clothing"):
        material.set_editor_property(usage, True)
    EDIT.recompile_material(material)
    if not LIB.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {target}")
    rows.append({"source": source, "target": target, "source_blend_mode": str(mode),
                 "coverage_expression": coverage.get_class().get_name() if coverage else "ConstantOne",
                 "source_shading_model": str(shading),
                 "preserved_inputs": {name: {"expression": expression.get_name() if expression else None,
                                             "output": output} for name, (expression, output) in preserved.items()},
                 "source_emission": original_emission.get_name() if original_emission else None,
                 "scalar_parameters": [str(n) for n in EDIT.get_scalar_parameter_names(material)],
                 "vector_parameters": [str(n) for n in EDIT.get_vector_parameter_names(material)]})
check_sources()
REPORT.write_text(json.dumps({"materials": rows, "protected": protected,
    "source_files_unchanged": True, "engine": version,
    "scope": "Original lit surface and emission plus native ghost emission, with source coverage times native fade. Rendered palette, body and eye adapters, runtime binding, performance and DX12 remain pending."}, indent=2) + "\n")
unreal.log("CSS_ASTRAL_SURFACES_CREATED")
