"""Capture Peony's existing materials under fixed lights in UE 5.6.1."""
import hashlib
import json
import os
from pathlib import Path

import unreal


stage = Path(os.environ['CSS_LIGHTING_STAGE']).resolve(strict=True)
assert Path(unreal.Paths.project_dir()).resolve() == stage / 'editor'
out = stage / 'captures'
out.mkdir(exist_ok=False)
inputs = json.loads((stage / 'inputs.json').read_text())
sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
assert all(sha(path) == digest for path, digest in inputs.items())
motion, materials = unreal.UGMotionLibrary, unreal.UGMaterialLibrary
edit, assets = unreal.MaterialEditingLibrary, unreal.AssetToolsHelpers.get_asset_tools()
world = motion.create_preview_world()
assert world
rows = []
try:
    for command in ('r.AllowOcclusionQueries 0', 'r.AntiAliasingMethod 0',
                    'r.BloomQuality 0', 'r.TextureStreaming 0'):
        unreal.SystemLibrary.execute_console_command(world, command)
    mesh = unreal.load_asset('/Game/CSS/eins0fx/EveSB/PeonyV2/Meshes/SK_PeonyV2')
    assert mesh
    actor = motion.spawn_preview_actor(unreal.SkeletalMeshActor)
    component = actor.get_component_by_class(unreal.SkeletalMeshComponent)
    component.set_skeletal_mesh_asset(mesh)
    component.set_forced_lod(1)
    component.set_editor_property('disable_post_process_blueprint', True)
    assert component.get_editor_property('disable_post_process_blueprint')
    slots = list(mesh.get_editor_property('materials'))
    wrappers, dynamics = [], []
    shown = set(range(20, 38))
    for index, slot in enumerate(slots):
        component.show_material_section(index, -1, index in shown, 0)
        if index not in shown:
            continue
        source = slot.get_editor_property('material_interface')
        assert source and 'MI_black_white_floral_' in source.get_path_name(), source
        wrapper = assets.create_asset('MI_Capture_' + str(index),
            '/Game/CSS/UnholyGenessa/LightingProbe', unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        assert wrapper
        edit.set_material_instance_parent(wrapper, source)
        edit.update_material_instance(wrapper)
        wrappers.append(wrapper)
        dynamic = component.create_dynamic_material_instance(index, wrapper)
        dynamic.set_scalar_parameter_value('GlowStrength', 0.)
        dynamic.set_scalar_parameter_value('FlowSpeed', 0.)
        dynamics.append(dynamic)
    compilation = json.loads(materials.finish_materials(wrappers))
    assert compilation['passed'], compilation
    materials.flush_component_updates(component)
    camera = motion.spawn_preview_actor(unreal.SceneCapture2D)
    capture = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
    capture.set_editor_properties(dict(capture_every_frame=False, capture_on_movement=False,
        capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,
        primitive_render_mode=unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST,
        projection_type=unreal.CameraProjectionMode.ORTHOGRAPHIC, ortho_width=170.))
    post = capture.get_editor_property('post_process_settings')
    post.set_editor_properties(dict(override_auto_exposure_method=True,
        auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
        override_auto_exposure_apply_physical_camera_exposure=True,
        auto_exposure_apply_physical_camera_exposure=False,
        override_auto_exposure_bias=True, auto_exposure_bias=0.))
    capture.set_editor_property('post_process_settings', post)
    target = unreal.RenderingLibrary.create_render_target2d(world, 640, 640,
        unreal.TextureRenderTargetFormat.RTF_RGBA8, unreal.LinearColor(0, 0, 0, 1))
    capture.set_editor_property('texture_target', target)
    capture.show_only_actor_components(actor)
    lights = []
    for _ in range(2):
        light = motion.spawn_preview_actor(unreal.DirectionalLight)
        light.get_component_by_class(unreal.DirectionalLightComponent).set_mobility(unreal.ComponentMobility.MOVABLE)
        lights.append(light)
    cases = [('front', 3., .15), ('back_weak_fill', .15, 3.), ('soft_fill', 1., 1.)]
    for view, y in [('front', -300.), ('back', 300.)]:
        location = unreal.Vector(0, y, 95)
        camera.set_actor_location_and_rotation(location,
            unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(0, 0, 95)), False, True)
        for light, position in zip(lights, (unreal.Vector(-80, y, 230), unreal.Vector(80, -y, 230))):
            light.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position, unreal.Vector(0, 0, 90)), False)
        for case, key, fill in cases:
            for light, intensity in zip(lights, (key, fill)):
                light.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(intensity)
            for glow in (0., 17.25):
                for dynamic in dynamics:
                    dynamic.set_scalar_parameter_value('GlowStrength', glow)
                materials.flush_component_updates(component)
                for _ in range(16):
                    motion.refresh_preview_rendering()
                    capture.capture_scene()
                    materials.flush_rendering()
                name = f'{view}-{case}-glow{glow:g}.png'
                unreal.RenderingLibrary.export_render_target(world, target, str(out), name)
                assert (out / name).is_file()
                rows.append(dict(image=name, sha256=sha(out / name), view=view,
                                 key_lux=key, fill_lux=fill, glow=glow))
                (stage / 'progress.json').write_text(json.dumps(rows, indent=2) + '\n')
finally:
    materials.flush_rendering()
    motion.end_preview_world()
    assert all(sha(path) == digest for path, digest in inputs.items())
(stage / 'result.json').write_text(json.dumps(dict(images=rows, compilation=compilation,
    inputs_unchanged=True, manual_exposure_bias=0, bloom=False, automatic_exposure=False,
    scope='Existing Peony V2 garment materials, reference pose, Vulkan offscreen. '
          'Body/hair hidden because editor stand-ins do not represent game skin. '
          'No environment, indirect lighting, game exposure or Windows parity claim.'), indent=2) + '\n')
unreal.log('CSS_PEONY_LIGHTING_CAPTURED')
