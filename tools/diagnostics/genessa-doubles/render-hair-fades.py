"""Compare source hair alpha and fade on flat UV cards, without outfit changes."""
import hashlib
import json
import os
from pathlib import Path
import re
import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
selection = os.environ.get("CSS_ASTRAL_HAIR_INDEX")
if selection is not None and (not selection.isdecimal() or not 0 <= int(selection) < 8):
    raise ValueError("CSS_ASTRAL_HAIR_INDEX must be 0..7")
suffix = "" if selection is None else "-" + selection
run = os.environ.get("CSS_ASTRAL_HAIR_RUN", "")
if run and not re.fullmatch(r"[a-zA-Z0-9_]{1,32}", run):
    raise ValueError("CSS_ASTRAL_HAIR_RUN must use 1..32 letters, digits or underscores")
if run:
    suffix += "-" + run
background = os.environ.get("CSS_ASTRAL_HAIR_BACKGROUND", "0")
if background not in ("0", "1"):
    raise ValueError("CSS_ASTRAL_HAIR_BACKGROUND must be 0 or 1")
opacities = {"original": 1., "full": 1., "half": .5, "zero": 0., "removed": 0.}
states = os.environ.get("CSS_ASTRAL_HAIR_STATES", ",".join(opacities)).split(",")
if not states or len(states) != len(set(states)) or any(state not in opacities for state in states):
    raise ValueError("CSS_ASTRAL_HAIR_STATES must contain unique original,full,half,zero,removed states")
OUT = ROOT / ("hair-renders" + suffix)
OUT.mkdir(exist_ok=False)
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
assert unreal.SystemLibrary.get_engine_version().startswith("5.6.1-")
protected = json.loads((ROOT / "render-sources.json").read_text())
manifest = json.loads((ROOT / "fade-materials.json").read_text())
targets = {row["source"]: row["target"] for row in manifest["materials"]}
sources = [f"/Game/CSS/SeduXtress/{section}/MI_ShellKeeper_Hair_01" for section in ("Hair", "Tail")]
sources += [f"/Game/CSS/CommanderWhite/MaterialY/MI_Layer_{slot:02}" for slot in (29, 30, 31, 32, 35, 36)]
# The existing compilation helper restricts its input to this fixture namespace.
fixture = "/Game/CSS/UnholyGenessa/AstralHairTest" + suffix.replace("-", "_") + "/"
if selection is not None:
    sources = [sources[int(selection)]]


def check_sources():
    for path, digest in protected.items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest, path


def duplicate(source, name):
    destination = fixture + name
    if LIB.does_asset_exist(destination):
        raise FileExistsError(destination)
    result = LIB.duplicate_asset(source, destination)
    assert result, source
    return result


check_sources()
parents = {}
cards = []
interfaces = []
for index, path in enumerate(sources):
    source = unreal.load_asset(path)
    assert isinstance(source, unreal.MaterialInstanceConstant)
    parent = source.get_editor_property("parent").get_path_name().split(".")[0]
    if parent not in parents:
        parents[parent] = (duplicate(parent, f"OriginalParent{index}"),
                           duplicate(targets[parent], f"FadeParent{index}"))
        interfaces.extend(parents[parent])
    original = duplicate(path, f"Original{index}")
    faded = duplicate(path, f"Fade{index}")
    for instance, shader in zip((original, faded), parents[parent]):
        EDIT.set_material_instance_parent(instance, shader)
        EDIT.update_material_instance(instance)
    cards.append((path, original, faded))
    interfaces.extend((original, faded))

backdrop_material = None
if background == "1":
    backdrop_material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_Backdrop", fixture, unreal.Material, unreal.MaterialFactoryNew())
    assert backdrop_material
    backdrop_material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = EDIT.create_material_expression(backdrop_material, unreal.MaterialExpressionConstant3Vector)
    color.set_editor_property("constant", unreal.LinearColor(.04, .04, .04, 1.))
    assert EDIT.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    EDIT.recompile_material(backdrop_material)
    interfaces.append(backdrop_material)
compiled = json.loads(unreal.UGMaterialLibrary.finish_materials(interfaces))
assert compiled["passed"], compiled
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for command in ("r.AllowOcclusionQueries 0", "r.AntiAliasingMethod 0", "r.BloomQuality 0"):
    unreal.SystemLibrary.execute_console_command(world, command)
light = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.DirectionalLight, unreal.Vector(0, 0, 200), unreal.Rotator(pitch=-70, yaw=15))
light.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(4.)
camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 150))
camera.set_actor_rotation(unreal.Rotator(pitch=-90), False)
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
target = unreal.RenderingLibrary.create_render_target2d(world, 512, 512,
    unreal.TextureRenderTargetFormat.RTF_RGBA8, unreal.LinearColor(.04, .04, .04, 1.))
capture.set_editor_property("texture_target", target)
plane = unreal.load_asset("/Engine/BasicShapes/Plane")
assert plane
backdrop = None
if backdrop_material:
    backdrop = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(0, 0, -1))
    backdrop.set_actor_scale3d(unreal.Vector(10, 10, 1))
    backdrop_component = backdrop.get_component_by_class(unreal.StaticMeshComponent)
    backdrop_component.set_static_mesh(plane)
    backdrop_component.set_material(0, backdrop_material)
rows = []
for index, (path, original, faded) in enumerate(cards):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(plane)
    capture.show_only_actor_components(actor)
    if backdrop:
        capture.show_only_actor_components(backdrop)
    for state in states:
        opacity = opacities[state]
        unreal.log(f"CSS_ASTRAL_HAIR_BEGIN source={path} state={state}")
        dynamic = component.create_dynamic_material_instance(0, original if state == "original" else faded)
        dynamic.set_scalar_parameter_value("CSS_AstralOpacity", opacity)
        component.set_visibility(state != "removed", False)
        unreal.UGMaterialLibrary.flush_component_updates(component)
        for _ in range(8):
            capture.capture_scene()
            unreal.UGMaterialLibrary.flush_rendering()
        name = f"{index:02}-{state}.png"
        unreal.RenderingLibrary.export_render_target(world, target, str(OUT), name)
        image = OUT / name
        assert image.is_file() and image.stat().st_size
        rows.append({"source": path, "state": state, "image": name,
                     "sha256": hashlib.sha256(image.read_bytes()).hexdigest()})
        unreal.log(f"CSS_ASTRAL_HAIR_END source={path} state={state}")
    capture.clear_show_only_components()
    unreal.EditorLevelLibrary.destroy_actor(actor)
check_sources()
(OUT / "captures.json").write_text(json.dumps({
    "images": rows, "compilation": compiled, "source_files_unchanged": True,
    "requested_states": states, "selection": selection, "run": run,
    "opaque_backdrop": background == "1",
    "scope": "Flat-card Vulkan hair coverage test only. Not full hairstyles, native ghost shading or DX12."
}, indent=2) + "\n")
unreal.log("CSS_ASTRAL_HAIR_CAPTURED")
