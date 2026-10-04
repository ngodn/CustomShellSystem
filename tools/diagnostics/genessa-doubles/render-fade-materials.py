"""Render coverage/fade comparisons in the isolated UE 5.6.1 project.

The original skin is a fixed reference. These renders test garment coverage,
not the native ghost effect, animation, or in-game DX12 shader compatibility.
"""
import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "fade-renders"
OUT.mkdir(exist_ok=False)
EDIT = unreal.MaterialEditingLibrary
LIB = unreal.EditorAssetLibrary
report = json.loads((ROOT / "fade-materials.json").read_text())
adapters = {}
# The existing authoring helper accepts only this project's material namespace.
# These extra copies are test fixtures, not paths intended for shipping.
for row in report["materials"]:
    destination = "/Game/CSS/UnholyGenessa/AstralFadeTest/Parents/" + row["target"].rsplit("/", 1)[1]
    if LIB.does_asset_exist(destination):
        raise RuntimeError(f"Refusing to overwrite {destination}")
    adapters[row["source"]] = LIB.duplicate_asset(row["target"], destination)
assert all(adapters.values())
assert unreal.SystemLibrary.get_engine_version().startswith("5.6.1-")
protected = json.loads((ROOT / "render-sources.json").read_text())


def check_sources():
    for path, digest in protected.items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


def asset_path(asset):
    return asset.get_path_name().split(".")[0]


instances = {}
adapted_paths = []


def adapt(material):
    path = asset_path(material)
    if path in instances:
        return instances[path]
    if path in adapters:
        return adapters[path]
    if not isinstance(material, unreal.MaterialInstanceConstant):
        return None
    parent = material.get_editor_property("parent")
    replacement = adapt(parent) if parent else None
    if replacement is None:
        return None
    destination = "/Game/CSS/UnholyGenessa/AstralFadeTest/Instances/" + path.removeprefix("/Game/").replace("/", "_")
    if LIB.does_asset_exist(destination):
        raise RuntimeError(f"Refusing to overwrite {destination}")
    instance = LIB.duplicate_asset(path, destination)
    assert instance
    EDIT.set_material_instance_parent(instance, replacement)
    EDIT.update_material_instance(instance)
    instances[path] = instance
    adapted_paths.append({"source": path, "copy": destination})
    return instance


check_sources()
world = unreal.EditorLevelLibrary.get_editor_world()
for command in ("r.AllowOcclusionQueries 0", "r.AntiAliasingMethod 0", "r.BloomQuality 0"):
    unreal.SystemLibrary.execute_console_command(world, command)
for pitch, yaw, intensity in ((-35, -60, 4), (-20, 120, 2), (-55, 210, 3)):
    light = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 250),
        unreal.Rotator(pitch=pitch, yaw=yaw, roll=0))
    light.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(intensity)

camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector())
capture = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
capture.set_editor_property("capture_every_frame", False)
capture.set_editor_property("capture_on_movement", False)
capture.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
capture.set_editor_property("fov_angle", 42.)
capture.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
settings = capture.get_editor_property("post_process_settings")
for limit in ("min", "max"):
    settings.set_editor_property("override_auto_exposure_" + limit + "_brightness", True)
    settings.set_editor_property("auto_exposure_" + limit + "_brightness", 1.)
capture.set_editor_property("post_process_settings", settings)
target = unreal.RenderingLibrary.create_render_target2d(
    world, 1000, 1000, unreal.TextureRenderTargetFormat.RTF_RGBA8,
    unreal.LinearColor(.025, .03, .035, 1.))
capture.set_editor_property("texture_target", target)

mesh = unreal.load_asset("/Game/CSS/UnholyGenessa/SK_EveW3")
assert mesh
slots = list(mesh.get_editor_property("materials"))
original_overlays = [slot.get_editor_property("overlay_material_interface") for slot in slots]
# Null component overlays otherwise fall back to the mesh's authored overlays.
# The sandbox makes source assets read-only; this edit exists only in memory.
for slot in slots:
    slot.set_editor_property("overlay_material_interface", None)
mesh.set_editor_property("materials", slots)
prepared = []
interfaces = []
for i, slot in enumerate(slots):
    original = slot.get_editor_property("material_interface")
    overlay = original_overlays[i]
    adapted = adapt(original)
    adapted_overlay = adapt(overlay) if overlay else None
    if overlay and adapted_overlay is None:
        raise RuntimeError(f"Missing overlay adapter: {asset_path(overlay)}")
    prepared.append((original, adapted, overlay, adapted_overlay))
    interfaces.extend(value for value in prepared[-1] if value and
                      asset_path(value).startswith("/Game/CSS/UnholyGenessa/"))
compiled = json.loads(unreal.UGMaterialLibrary.finish_materials(interfaces))
assert compiled["passed"], compiled
rows = []
coverage = []
for state, opacity in (("original", 1.), ("full", 1.), ("half", .5), ("zero", 0.), ("removed", 0.)):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector())
    component = actor.get_component_by_class(unreal.SkeletalMeshComponent)
    component.set_skeletal_mesh_asset(mesh)
    component.suspend_clothing_simulation()
    component.set_editor_property("disable_cloth_simulation", True)
    component.set_component_tick_enabled(False)
    overlays = []
    for i, (original, adapted, overlay, adapted_overlay) in enumerate(prepared):
        material = original if state == "original" or adapted is None else adapted
        dynamic = component.create_dynamic_material_instance(i, material)
        dynamic.set_scalar_parameter_value("FlowSpeed", 0.)
        dynamic.set_scalar_parameter_value("FlowPhase", .25)
        if adapted:
            dynamic.set_scalar_parameter_value("CSS_AstralOpacity", opacity)
        if state == "removed" and adapted:
            component.show_material_section(i, -1, False, 0)
        if str(slots[i].get_editor_property("material_slot_name")) in ("UG_Eyes", "UG_EyeSmoke"):
            component.show_material_section(i, -1, False, 0)
        overlay_dynamic = None
        if overlay and state != "removed":
            overlay_dynamic = unreal.MaterialLibrary.create_dynamic_material_instance(
                component, overlay if state == "original" else adapted_overlay)
            overlay_dynamic.set_scalar_parameter_value("CSS_AstralOpacity", opacity)
        overlays.append(overlay_dynamic)
        if state == "full":
            coverage.append({"slot": i, "name": str(slots[i].get_editor_property("material_slot_name")),
                             "material": asset_path(original), "adapted": bool(adapted),
                             "overlay": asset_path(overlay) if overlay else None})
    component.set_editor_property("material_slots_overlay_material", overlays)
    unreal.UGMaterialLibrary.flush_component_updates(component)
    capture.show_only_actor_components(actor)
    for view, location in (("front", (150, 570, 130)), ("back", (-150, -570, 130))):
        where = unreal.Vector(*location)
        camera.set_actor_location_and_rotation(where, unreal.MathLibrary.find_look_at_rotation(
            where, unreal.Vector(0, 0, 105)), False, True)
        for _ in range(8):
            capture.capture_scene()
            unreal.UGMaterialLibrary.flush_rendering()
        filename = f"{view}-{state}.png"
        unreal.RenderingLibrary.export_render_target(world, target, str(OUT), filename)
        path = OUT / filename
        assert path.is_file() and path.stat().st_size
        rows.append({"state": state, "view": view, "image": filename,
                     "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    capture.clear_show_only_components()
    unreal.EditorLevelLibrary.destroy_actor(actor)
check_sources()
(OUT / "captures.json").write_text(json.dumps({
    "images": rows, "compilation": compiled, "source_files_unchanged": True,
    "slots": coverage, "instances": adapted_paths,
    "scope": "Vulkan reference-pose garment fade only. Body and eyes are not ghost-adapted. "
             "Native shading, DX12 cook, live customization, animations and performance remain unverified."
}, indent=2) + "\n")
unreal.log("CSS_ASTRAL_FADE_CAPTURED")
