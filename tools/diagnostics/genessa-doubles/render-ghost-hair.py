"""Check native ghost shading through Eve and Commander White hair coverage.

A cube viewed straight at one face provides UVs and nonzero local bounds. A
flat plane would divide by zero in the native bounds-normalized ghost graph.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_ghost_graph import node

ROOT = Path(unreal.Paths.project_dir()).resolve()
run = os.environ.get("CSS_ASTRAL_GHOST_HAIR_RUN", "1")
if not re.fullmatch(r"[a-zA-Z0-9_]{1,32}", run):
    raise ValueError("Invalid ghost hair run label")
OUT = ROOT / ("ghost-hair-renders-" + run)
OUT.mkdir(exist_ok=False)
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
PREFIX = "/Game/CSS/UnholyGenessa/AstralGhostHair_" + run + "/"
report = json.loads((ROOT / "ghost-coverage.json").read_text())
targets = {row["source"]: row["target"] for row in report["materials"]}
protected = {**report["protected"], **json.loads((ROOT / "render-sources.json").read_text())}
sources = [f"/Game/CSS/SeduXtress/{part}/MI_ShellKeeper_Hair_01" for part in ("Hair", "Tail")]
sources += [f"/Game/CSS/CommanderWhite/MaterialY/MI_Layer_{slot:02}" for slot in (29, 30, 31, 32, 35, 36)]


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
parents = {}
cards = []
interfaces = []
for index, path in enumerate(sources):
    source = LIB.load_asset(path)
    if not isinstance(source, unreal.MaterialInstanceConstant):
        raise TypeError(path)
    overrides = source.get_editor_property("base_property_overrides")
    # These fixtures compare the audited parents. Fail rather than silently
    # ignore a per-instance blend/shading override that needs another adapter.
    for name in ("blend_mode", "shading_model", "opacity_mask_clip_value"):
        if overrides.get_editor_property("override_" + name):
            raise RuntimeError(f"Handle effective {name} before adapting {path}")
    parent_path = source.get_editor_property("parent").get_path_name().split(".")[0]
    if parent_path not in parents:
        reference = duplicate(parent_path, f"CoverageParent{index}")
        if reference.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_TRANSLUCENT:
            raise ValueError(f"Expected audited translucent hair parent: {parent_path}")
        white = node(reference, "Constant3Vector", constant=unreal.LinearColor(100., 100., 100., 1.))
        if not EDIT.connect_material_property(white, "", MP.MP_EMISSIVE_COLOR):
            raise RuntimeError("Could not create coverage reference")
        reference.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        EDIT.recompile_material(reference)
        ghost = LIB.load_asset(targets[parent_path])
        if not isinstance(ghost, unreal.Material):
            raise TypeError(targets[parent_path])
        parents[parent_path] = (reference, ghost)
        interfaces.extend((reference, ghost))
    reference_mi = duplicate(path, f"Coverage{index}")
    ghost_mi = duplicate(path, f"Ghost{index}")
    for instance, parent in zip((reference_mi, ghost_mi), parents[parent_path]):
        EDIT.set_material_instance_parent(instance, parent)
        EDIT.update_material_instance(instance)
    cards.append((path, reference_mi, ghost_mi))
    interfaces.extend((reference_mi, ghost_mi))

native = json.loads((ROOT / "native-ghost-material.json").read_text())
uncut = LIB.load_asset(native["material"])
backdrop_material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "Backdrop", PREFIX, unreal.Material, unreal.MaterialFactoryNew())
if backdrop_material is None:
    raise RuntimeError("Could not create backdrop")
backdrop_material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
black = node(backdrop_material, "Constant3Vector", constant=unreal.LinearColor(0., 0., 0., 1.))
if not EDIT.connect_material_property(black, "", MP.MP_EMISSIVE_COLOR):
    raise RuntimeError("Could not connect backdrop")
EDIT.recompile_material(backdrop_material)
interfaces.extend((uncut, backdrop_material))
compiled = json.loads(unreal.UGMaterialLibrary.finish_materials(interfaces))
if not compiled["passed"]:
    raise RuntimeError(compiled)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for command in ("r.AllowOcclusionQueries 0", "r.AntiAliasingMethod 0", "r.BloomQuality 0"):
    unreal.SystemLibrary.execute_console_command(world, command)
camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 250))
camera.set_actor_rotation(unreal.Rotator(pitch=-90), False)
capture = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
capture.set_editor_property("capture_every_frame", False)
capture.set_editor_property("capture_on_movement", False)
capture.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
capture.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
capture.set_editor_property("fov_angle", 35.)
settings = capture.get_editor_property("post_process_settings")
settings.set_editor_property("override_auto_exposure_method", True)
settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
settings.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
settings.set_editor_property("override_auto_exposure_bias", True)
settings.set_editor_property("auto_exposure_bias", 0.)
capture.set_editor_property("post_process_settings", settings)
target = unreal.RenderingLibrary.create_render_target2d(world, 512, 512,
    unreal.TextureRenderTargetFormat.RTF_RGBA8, unreal.LinearColor(0., 0., 0., 1.))
capture.set_editor_property("texture_target", target)
cube = LIB.load_asset("/Engine/BasicShapes/Cube")
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
component = actor.get_component_by_class(unreal.StaticMeshComponent)
component.set_static_mesh(cube)
backdrop = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -100))
backdrop.set_actor_scale3d(unreal.Vector(10, 10, .1))
backdrop_component = backdrop.get_component_by_class(unreal.StaticMeshComponent)
backdrop_component.set_static_mesh(cube)
backdrop_component.set_material(0, backdrop_material)
capture.show_only_actor_components(actor)
capture.show_only_actor_components(backdrop)
rows = []
for index, (path, reference, ghost) in enumerate(cards):
    for form, corrupted in (("faithful", 0.), ("stray", 1.)):
        for state, opacity, time in (("coverage", 1., 0.), ("uncut", 1., 0.),
                ("full_t0", 1., 0.), ("full_t1", 1., 1.), ("half", .5, 0.),
                ("zero", 0., 0.), ("removed", 0., 0.)):
            unreal.log(f"CSS_GHOST_HAIR_BEGIN index={index} form={form} state={state}")
            parent = reference if state == "coverage" else uncut if state == "uncut" else ghost
            dynamic = component.create_dynamic_material_instance(0, parent)
            dynamic.set_scalar_parameter_value("CSS_AstralOpacity", opacity)
            dynamic.set_scalar_parameter_value("CSS_AstralCorrupted", corrupted)
            dynamic.set_scalar_parameter_value("CSS_AstralUseFixedTime", 1.)
            dynamic.set_scalar_parameter_value("CSS_AstralFixedTime", time)
            component.set_visibility(state != "removed", False)
            unreal.UGMaterialLibrary.flush_component_updates(component)
            for _ in range(8):
                capture.capture_scene()
                unreal.UGMaterialLibrary.flush_rendering()
            name = f"{index:02}-{form}-{state}.png"
            unreal.RenderingLibrary.export_render_target(world, target, str(OUT), name)
            image = OUT / name
            if not image.is_file() or image.stat().st_size == 0:
                raise RuntimeError(f"Empty capture: {image}")
            rows.append({"source": path, "form": form, "state": state,
                "opacity": opacity, "time": time, "image": name,
                "sha256": hashlib.sha256(image.read_bytes()).hexdigest()})
            unreal.log(f"CSS_GHOST_HAIR_END index={index} form={form} state={state}")
check_sources()
(OUT / "captures.json").write_text(json.dumps({"images": rows, "compilation": compiled,
    "source_files_unchanged": True,
    "scope": "Vulkan hair texture coverage with reconstructed ghost shading. Cube fixture, not full hairstyle, source tint composition, DX12 or runtime support."
}, indent=2) + "\n")
unreal.log("CSS_GHOST_HAIR_CAPTURED")
