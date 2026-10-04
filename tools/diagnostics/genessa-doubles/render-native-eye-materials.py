"""Capture the reconstructed eyes on the authored mesh, without editing it."""
import hashlib
import json
import os
from pathlib import Path
import re
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
run = os.environ.get("CSS_ASTRAL_EYE_RUN", "1")
if not re.fullmatch(r"[a-zA-Z0-9_]{1,32}", run):
    raise ValueError("Invalid eye capture label")
OUT = ROOT / ("native-eye-renders-" + run)
OUT.mkdir(exist_ok=False)
report = json.loads((ROOT / "native-eye-materials.json").read_text())
protected = dict(report["protected"])
protected.update(json.loads((ROOT / "garment-fixture-sources.json").read_text())["protected"])


def check_sources():
    for path, digest in protected.items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


check_sources()
materials = {(row["kind"], row["role"]): unreal.load_asset(row["path"])
             for row in report["materials"]}
if not all(materials.values()):
    raise RuntimeError("Missing eye materials")
compiled = json.loads(unreal.UGMaterialLibrary.finish_materials(list(materials.values())))
if not compiled["passed"]:
    raise RuntimeError(compiled)
mesh = unreal.load_asset("/Game/CSS/UnholyGenessa/SK_EveW3")
if not isinstance(mesh, unreal.SkeletalMesh):
    raise TypeError("Missing staged Genessa mesh")
slots = list(mesh.get_editor_property("materials"))
indices = {}
for index, slot in enumerate(slots):
    name = str(slot.get_editor_property("material_slot_name"))
    if name in ("UG_Eyes", "UG_EyeSmoke"):
        indices["eye" if name == "UG_Eyes" else "smoke"] = index
    slot.set_editor_property("overlay_material_interface", None)
if set(indices) != {"eye", "smoke"}:
    raise RuntimeError("Missing authored eye sections")
mesh.set_editor_property("materials", slots)
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector())
component = actor.get_component_by_class(unreal.SkeletalMeshComponent)
component.set_skeletal_mesh_asset(mesh)
component.suspend_clothing_simulation()
component.set_editor_property("disable_cloth_simulation", True)
component.set_component_tick_enabled(False)
component.set_forced_lod(1)
bone_names = [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
heads = [name for name in bone_names if name.lower() == "head"]
if len(heads) != 1:
    raise RuntimeError(f"Expected one head bone: {bone_names}")
head = component.get_socket_location(heads[0])
look_at = head + unreal.Vector(0, 0, 7)
where = look_at + unreal.Vector(0, 300, 0)
camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D, where)
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(where, look_at), False)
capture = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
for name, value in {"capture_every_frame": False, "capture_on_movement": False,
                    "capture_source": unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR,
                    "primitive_render_mode": unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST,
                    "fov_angle": 8.}.items():
    capture.set_editor_property(name, value)
settings = capture.get_editor_property("post_process_settings")
for name, value in {"override_auto_exposure_method": True,
                    "auto_exposure_method": unreal.AutoExposureMethod.AEM_MANUAL,
                    "override_auto_exposure_apply_physical_camera_exposure": True,
                    "auto_exposure_apply_physical_camera_exposure": False,
                    "override_auto_exposure_bias": True, "auto_exposure_bias": 0.}.items():
    settings.set_editor_property(name, value)
capture.set_editor_property("post_process_settings", settings)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for command in ("r.AllowOcclusionQueries 0", "r.AntiAliasingMethod 0", "r.BloomQuality 0"):
    unreal.SystemLibrary.execute_console_command(world, command)
resolution = 256
target = unreal.RenderingLibrary.create_render_target2d(world, resolution, resolution,
    unreal.TextureRenderTargetFormat.RTF_RGBA16F, unreal.LinearColor(0, 0, 0, 1))
capture.set_editor_property("texture_target", target)
capture.show_only_actor_components(actor)
rows = []
for kind in ("eye", "smoke"):
    for index in range(len(slots)):
        component.show_material_section(index, -1, index == indices[kind], 0)
    for form, corrupted in (("faithful", 0.), ("stray", 1.)):
        states = ["source", "full", "half", "zero", "removed", "ghost_time",
                  "intensity_zero", "color_red", "color_blue"]
        if kind == "smoke":
            states += ["smoke_time", "smoke_opacity_zero"]
        for state in states:
            material = materials[kind, "source" if state == "source" else "ghost"]
            dynamic = component.create_dynamic_material_instance(indices[kind], material)
            opacity = .5 if state == "half" else 0. if state in ("zero", "removed") else 1.
            dynamic.set_scalar_parameter_value("CSS_AstralOpacity", opacity)
            dynamic.set_scalar_parameter_value("CSS_AstralCorrupted", corrupted)
            dynamic.set_scalar_parameter_value("CSS_AstralUseFixedTime", 1.)
            dynamic.set_scalar_parameter_value("CSS_AstralFixedTime", 1. if state == "ghost_time" else 0.)
            if kind == "smoke":
                dynamic.set_scalar_parameter_value("CSS_EyeUseFixedTime", 1.)
                dynamic.set_scalar_parameter_value("CSS_EyeFixedTime", 1. if state == "smoke_time" else 0.)
            if state == "intensity_zero":
                dynamic.set_scalar_parameter_value("Intensity", 0.)
            if state == "smoke_opacity_zero":
                dynamic.set_scalar_parameter_value("Opacity Multiplier", 0.)
            if state in ("color_red", "color_blue"):
                dynamic.set_vector_parameter_value("Color", unreal.LinearColor(
                    *([.8, .025, .025, 1.] if state == "color_red" else [.025, .025, .8, 1.])))
            component.set_visibility(state != "removed", False)
            unreal.UGMaterialLibrary.flush_component_updates(component)
            for _ in range(8):
                capture.capture_scene()
                unreal.UGMaterialLibrary.flush_rendering()
            name = f"{kind}-{form}-{state}.exr"
            unreal.RenderingLibrary.export_render_target(world, target, str(OUT), name)
            image = OUT / name
            if not image.is_file() or not image.stat().st_size:
                raise RuntimeError(f"Missing capture: {image}")
            rows.append({"kind": kind, "form": form, "state": state,
                         "opacity": opacity, "image": name,
                         "sha256": hashlib.sha256(image.read_bytes()).hexdigest()})
            unreal.log(f"CSS_NATIVE_EYE_CAPTURED {name}")
check_sources()
(OUT / "captures.json").write_text(json.dumps({"images": rows,
    "compilation": compiled, "source_files_unchanged": True, "resolution": resolution,
    "mesh": mesh.get_path_name(), "slots": indices,
    "head_position": [head.x, head.y, head.z],
    "scope": "Vulkan eye and smoke sections in reference pose with original vertex colors. No face occlusion, native image comparison, DX12 or runtime proof."}, indent=2) + "\n")
