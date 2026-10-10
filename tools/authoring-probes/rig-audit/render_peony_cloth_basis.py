"""Compare stock and shared cloth deformers on identical held Peony simulations."""
import hashlib
import json
import os
from pathlib import Path
import time

import unreal


stage = Path(os.environ['CSS_PEONY_GPU_STAGE']).resolve(strict=True)
assert stage.name in {'peony-clothgpu1', 'peony-clothgpu2'}
assert Path(unreal.Paths.project_dir()).resolve() == stage / 'editor'
captures = stage / 'captures'
captures.mkdir()
protected = json.loads((stage / 'inputs.json').read_text())
protected.update(json.loads((stage / 'attempt1/diagnostic-material-inputs.json').read_text()))
protected.update({str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in (stage / 'editor/Content').rglob('*.uasset')})
audit = unreal.CSSRigAuditLibrary
motion = unreal.UGMotionLibrary
materials = unreal.UGMaterialLibrary
shapes = ['PBMGlutesSize', 'PBMHipSize', 'PBMThighsTone', 'PBMWaistWidth', 'FBMBodyTone']
slots = ['Peony_leg_panels_Lower_2_L', 'Peony_leg_panels_Lower_2_R']
cases = [dict(name='neutral', weights={}, cloth=True)]
cases += [dict(name=name, weights={name: 1.}, cloth=True) for name in shapes]
cases += [dict(name='all-half', weights=dict.fromkeys(shapes, .5), cloth=True),
          dict(name='all-max', weights=dict.fromkeys(shapes, 1.), cloth=True),
          dict(name='glute-disabled', weights={shapes[0]: 1.}, cloth=False),
          dict(name='all-disabled', weights=dict.fromkeys(shapes, 1.), cloth=False),
          dict(name='neutral-disabled', weights={}, cloth=False)]


def decode(text):
    if text.startswith('ERROR:'):
        raise RuntimeError(text)
    return json.loads(text)


def diagnostic_material(name):
    # The existing readiness helper allows this private diagnostic namespace.
    result = unreal.load_asset('/Game/CSS/UnholyGenessa/DeformerStudy/' + name)
    assert isinstance(result, unreal.Material)
    return result


world = motion.create_preview_world()
assert world
rows = []
try:
    for command in ('r.AllowOcclusionQueries 0', 'r.AntiAliasingMethod 0', 'r.BloomQuality 0'):
        unreal.SystemLibrary.execute_console_command(world, command)
    body = diagnostic_material('M_Body')
    fabric = diagnostic_material('M_Cloth')
    assert decode(materials.finish_materials([body, fabric]))['passed']
    camera = motion.spawn_preview_actor(unreal.SceneCapture2D)
    capture = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
    capture.set_editor_properties(dict(capture_every_frame=False, capture_on_movement=False,
        capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,
        primitive_render_mode=unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST,
        projection_type=unreal.CameraProjectionMode.ORTHOGRAPHIC, ortho_width=120.))
    post = capture.get_editor_property('post_process_settings')
    post.set_editor_properties(dict(override_auto_exposure_method=True,
        auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
        override_auto_exposure_apply_physical_camera_exposure=True,
        auto_exposure_apply_physical_camera_exposure=False))
    capture.set_editor_property('post_process_settings', post)
    target = unreal.RenderingLibrary.create_render_target2d(world, 512, 512,
        unreal.TextureRenderTargetFormat.RTF_RGBA8, unreal.LinearColor(0, 0, 0, 1))
    capture.set_editor_property('texture_target', target)
    location = unreal.Vector(-40, -300, 90)
    camera.set_actor_location_and_rotation(location,
        unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(0, 0, 90)), False, True)
    stock = unreal.load_asset('/DeformerGraph/Deformers/DG_LinearBlendSkin_Morph_Cloth_RecomputeNormals')
    trial = unreal.load_asset('/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphBody')
    assert stock and trial
    for name in ('SK_Peony', 'SK_Peony_Face'):
        mesh = unreal.load_asset('/Game/CSS/eins0fx/EveSB/Peony/Meshes/' + name + '_BasisStudy3')
        assert mesh
        (stage / (name + '-structure.json')).write_text(audit.snapshot_mesh(mesh))
        (stage / (name + '-cloth-morphs.json')).write_text(audit.snapshot_render_sections(mesh, slots, shapes, False))
        for case in cases:
            actor = motion.spawn_preview_actor(unreal.SkeletalMeshActor)
            component = actor.get_component_by_class(unreal.SkeletalMeshComponent)
            component.set_skeletal_mesh_asset(mesh)
            component.set_forced_lod(1)
            for index, material in enumerate(mesh.get_editor_property('materials')):
                slot = str(material.get_editor_property('material_slot_name'))
                component.set_material(index, fabric if slot in slots else body)
                component.show_material_section(index, -1, slot in slots or slot == 'Eve Body', 0)
            component.set_editor_property('material_slots_overlay_material', [None] * len(mesh.get_editor_property('materials')))
            assert audit.set_cloth_study_deformer(component, stock)
            for frame in range(91):
                for shape in shapes:
                    component.set_morph_target(shape, case['weights'].get(shape, 0.), False)
                state = decode(audit.step_cloth_deformer_study(component, frame == 0, frame == 0, not case['cloth']))
                if frame == 0:
                    assert decode(materials.finish_materials([body, fabric]))['passed']
            assert state['simulation_cloths'] == 2
            if case['cloth']:
                assert state['particles'] > 0
            held_hash = state['cloth_state_sha1']
            capture.show_only_actor_components(actor)
            for mode, graph in [('stock', stock), ('trial', trial)]:
                assert audit.set_cloth_study_deformer(component, graph)
                assert decode(materials.finish_materials([body, fabric]))['passed']
                if mode == 'trial':
                    instance = component.get_mesh_deformer_instance()
                    for shape in shapes:
                        assert instance.set_float_variable('CSS_Morph_' + shape, case['weights'].get(shape, 0.))
                state = decode(audit.step_cloth_deformer_study(component, False, False, not case['cloth'], False))
                assert not state['advanced_simulation'] and state['cloth_state_sha1'] == held_hash
                assert state['cloth_disabled'] == (not case['cloth'])
                label = name + '-' + case['name'] + '-' + mode
                geometry = Path(unreal.Paths.project_saved_dir()) / 'DeformerReadback' / (label + '.json')
                assert audit.request_cloth_deformer_readback(component, label)
                state = decode(audit.step_cloth_deformer_study(component, False, False, not case['cloth'], False))
                assert state['cloth_state_sha1'] == held_hash
                for _ in range(8):
                    motion.refresh_preview_rendering()
                    capture.capture_scene()
                    materials.flush_rendering()
                deadline = time.monotonic() + 30
                while not geometry.is_file() and time.monotonic() < deadline:
                    motion.refresh_preview_rendering()
                    capture.capture_scene()
                    materials.flush_rendering()
                    time.sleep(.02)
                assert geometry.is_file(), label
                result = json.loads(geometry.read_text())
                assert result['passed'] and result['vertices'] > 0
                image = label + '.png'
                unreal.RenderingLibrary.export_render_target(world, target, str(captures), image)
                assert (captures / image).is_file()
                rows.append(dict(mesh=name, case=case, mode=mode, state=state, image=image,
                                 gpu_readback=str(geometry), sha256=hashlib.sha256(geometry.read_bytes()).hexdigest()))
                (stage / 'progress.json').write_text(json.dumps(rows, indent=2))
            capture.clear_show_only_components()
            actor.destroy_actor()
finally:
    materials.flush_rendering()
    motion.end_preview_world()
    for path, checksum in protected.items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == checksum, path

assert len(rows) == 44
(stage / 'result.json').write_text(json.dumps(dict(captures=rows, protected=protected,
    scope='Offscreen Vulkan reference-pose cloth with post-process disabled and flat diagnostic materials. '
          'Identical held simulation for each stock/trial pair. Not gameplay, fit or performance acceptance.'), indent=2))
