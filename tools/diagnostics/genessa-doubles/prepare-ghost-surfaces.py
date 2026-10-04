"""Prepare final surface comparison fixtures under NullRHI before rendering.

Uses a single cube face and HDR readback. These fixtures do not establish full
character sorting, DX12 behavior or performance with multiple active summons.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_ghost_graph import node, wire

ROOT = Path(unreal.Paths.project_dir()).resolve()
run = os.environ.get("CSS_ASTRAL_SURFACE_RUN", "1")
if not re.fullmatch(r"[a-zA-Z0-9_]{1,32}", run):
    raise ValueError("Invalid surface run label")
OUT = ROOT / ("ghost-surface-renders-" + run)
OUT.mkdir(exist_ok=False)
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
PREFIX = "/Game/CSS/UnholyGenessa/AstralGhostSurface_" + run + "/"
report = json.loads((ROOT / "ghost-surfaces.json").read_text())
targets = {row["source"]: row["target"] for row in report["materials"]}
protected = {**report["protected"], **json.loads((ROOT / "render-sources.json").read_text())}
sources = [row["source"] for row in report["materials"] if "/UnholyGenessa/" in row["source"]]
sources += ["/Game/CSS/SeduXtress/Hair/MI_ShellKeeper_Hair_01",
            "/Game/CSS/CommanderWhite/MaterialY/MI_Layer_32"]
coverage_targets = {row["source"]: row["target"] for row in
                    json.loads((ROOT / "ghost-coverage.json").read_text())["materials"]}


def check_sources():
    for path, digest in protected.items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


def duplicate(source, name):
    target = PREFIX + name
    if LIB.does_asset_exist(target):
        raise FileExistsError(target)
    result = LIB.duplicate_asset(source, target)
    if result is None:
        raise RuntimeError(f"Could not duplicate {source}")
    return result


check_sources()
cards = []
interfaces = []
for index, path in enumerate(sources):
    source = LIB.load_asset(path)
    if isinstance(source, unreal.MaterialInstanceConstant):
        overrides = source.get_editor_property("base_property_overrides")
        for name in ("blend_mode", "shading_model", "opacity_mask_clip_value"):
            if overrides.get_editor_property("override_" + name):
                raise RuntimeError(f"Handle effective {name} before adapting {path}")
        parent_path = source.get_editor_property("parent").get_path_name().split(".")[0]
    elif isinstance(source, unreal.Material):
        parent_path = path
    else:
        raise TypeError(path)
    combined = duplicate(targets[parent_path], f"CombinedParent{index}")
    surface = duplicate(targets[parent_path], f"SurfaceParent{index}")
    addition = EDIT.get_material_property_input_node(surface, MP.MP_EMISSIVE_COLOR)
    if not isinstance(addition, unreal.MaterialExpressionAdd):
        raise RuntimeError("Expected surface emission plus native ghost emission")
    zero = node(surface, "Constant", r=0.)
    wire(zero, addition, "B")
    ghost = duplicate(coverage_targets[parent_path], f"GhostParent{index}")
    reference = duplicate(coverage_targets[parent_path], f"CoverageParent{index}")
    white = node(reference, "Constant3Vector", constant=unreal.LinearColor(100., 100., 100., 1.))
    if not EDIT.connect_material_property(white, "", MP.MP_EMISSIVE_COLOR):
        raise RuntimeError("Could not create coverage reference")
    # Keep the same native fade in this reference, so zero coverage has an
    # independent, bright signal even where the authored surface is black.
    family = {}
    for role, parent in (("combined", combined), ("surface_only", surface),
                         ("ghost_only", ghost), ("coverage", reference)):
        parent.set_editor_property("two_sided", False)
        EDIT.recompile_material(parent)
        if isinstance(source, unreal.MaterialInstanceConstant):
            instance = duplicate(path, f"{role}{index}")
        else:
            instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                f"{role}{index}", PREFIX, unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew())
            if instance is None:
                raise RuntimeError("Could not create fixture instance")
        EDIT.set_material_instance_parent(instance, parent)
        EDIT.update_material_instance(instance)
        family[role] = instance
        interfaces.extend((parent, instance))
    vectors = [str(n) for n in EDIT.get_vector_parameter_names(combined) if str(n) != "GlowColor"]
    scalars = [str(n) for n in EDIT.get_scalar_parameter_names(combined)]
    cards.append((path, family, vectors, scalars))

backdrop_material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "Backdrop", PREFIX, unreal.Material, unreal.MaterialFactoryNew())
if backdrop_material is None:
    raise RuntimeError("Could not create backdrop")
backdrop_material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
black = node(backdrop_material, "Constant3Vector", constant=unreal.LinearColor(0., 0., 0., 1.))
if not EDIT.connect_material_property(black, "", MP.MP_EMISSIVE_COLOR):
    raise RuntimeError("Could not connect backdrop")
EDIT.recompile_material(backdrop_material)
interfaces.append(backdrop_material)
for interface in interfaces:
    if not LIB.save_loaded_asset(interface, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save fixture {interface.get_path_name()}")
check_sources()
receipt = {"protected": protected, "source_files_unchanged": True,
    "backdrop": backdrop_material.get_path_name(),
    "interfaces": [m.get_path_name() for m in interfaces],
    "cards": [{"source": path, "family": {role: m.get_path_name() for role, m in family.items()},
               "vectors": vectors, "scalars": scalars} for path, family, vectors, scalars in cards]}
with (ROOT / ("ghost-surface-fixtures-" + run + ".json")).open("x") as stream:
    stream.write(json.dumps(receipt, indent=2) + "\n")
unreal.log("CSS_GHOST_SURFACE_FIXTURES_CREATED")
