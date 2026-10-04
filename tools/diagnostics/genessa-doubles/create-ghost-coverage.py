"""Combine native ghost shading with original garment/hair coverage in private copies.

This pass tests cutouts and visibility. Source palette and lighting composition
remain separate; do not ship these coverage-only parents as completed adapters.
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
TARGET = "/Game/CSS/UnholyGenessa/AstralCoverage1/Parents/"
REPORT = ROOT / "ghost-coverage.json"
if REPORT.exists() or LIB.does_directory_exist(TARGET):
    raise FileExistsError("Ghost coverage experiment already exists")
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
    for expression, property_ in ((color, MP.MP_EMISSIVE_COLOR), (alpha, MP.MP_OPACITY)):
        if not EDIT.connect_material_property(expression, "", property_):
            raise RuntimeError(f"Could not set {property_}")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("dither_opacity_mask", False)
    material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
    material.set_editor_property("automatically_set_usage_in_editor", False)
    for usage in ("used_with_skeletal_mesh", "used_with_morph_targets", "used_with_clothing"):
        material.set_editor_property(usage, True)
    EDIT.recompile_material(material)
    if not LIB.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {target}")
    rows.append({"source": source, "target": target, "source_blend_mode": str(mode),
                 "coverage_expression": coverage.get_class().get_name() if coverage else "ConstantOne"})
check_sources()
REPORT.write_text(json.dumps({"materials": rows, "protected": protected,
    "source_files_unchanged": True, "engine": version,
    "scope": "Native ghost shading times original coverage. Source palette/lighting composition, body and eye adapters, runtime binding and DX12 remain pending."}, indent=2) + "\n")
unreal.log("CSS_ASTRAL_COVERAGE_CREATED")
