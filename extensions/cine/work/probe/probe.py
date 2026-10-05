"""Live probes for CINE 1.1.0 (clean scene, action takes, breakdown). Python 3.14.

Answers the INFERRED items in docs/SCENE-RESEARCH.md against the running game through the CSSX
dev channel. Every step restores what it changed, also on error. Needs: game running, loaded
into the world, the game window visible (for the screenshots), and for the enemy steps one
ordinary enemy within about 40 m that is not a boss.

Usage: python3 probe.py list | python3 probe.py all | python3 probe.py STEP [STEP...]
Results: ../export-probe/results-<time>.json and screenshots beside it (git-ignored).
"""
import json
import math
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / 'core/tools'))
import cssx  # noqa: E402

OUT = HERE.parent / 'export-probe'
OUT.mkdir(exist_ok=True)
STAMP = time.strftime('%H%M%S')
results = {}
ctx = {}
IDENTITY = {'Rotation': {'X': 0, 'Y': 0, 'Z': 0, 'W': 1}, 'Translation': {'X': 0, 'Y': 0, 'Z': 0},
            'Scale3D': {'X': 1, 'Y': 1, 'Z': 1}}


def eng(request, timeout=20):
    reply = cssx.request(cssx.GAME, {'op': 'engine', 'request': request}, timeout)
    if not reply.get('ok'):
        raise RuntimeError(reply.get('error', 'request failed'))
    return reply.get('result')


def is_obj(value):
    return isinstance(value, dict) and '$object' in value


def call(target, function, args=None):
    return eng({'op': 'call', 'target': target, 'function': function, 'args': args or {}})


def ret(value):
    return value['ReturnValue'] if isinstance(value, dict) and 'ReturnValue' in value else value


def get(target, name):
    return eng({'op': 'get', 'target': target, 'property': name})


def put(target, name, value):
    return eng({'op': 'set', 'target': target, 'property': name, 'value': value})


def find(path):
    return eng({'op': 'find', 'path': path})


def load(path):
    value = eng({'op': 'load', 'path': path})
    if not is_obj(value):
        raise RuntimeError(f'cannot load {path}')
    return value


def note(step, key, value):
    results.setdefault(step, {})[key] = value
    text = json.dumps(value) if not isinstance(value, str) else value
    print(f'  {key}: {text[:300]}')


def grab(name):
    """Screenshot of the game window, only if it is really on screen."""
    try:
        clients = json.loads(subprocess.check_output(['hyprctl', '-j', 'clients']))
        monitors = json.loads(subprocess.check_output(['hyprctl', '-j', 'monitors']))
        game = [c for c in clients if c['class'] == 'steam_app_2584270' and c['title'].strip() == 'MortalShell2']
        shown = {m['activeWorkspace']['name'] for m in monitors}
        if not game or game[0]['workspace']['name'] not in shown:
            print(f'  (no screenshot {name}: game window not on screen)')
            return None
        (x, y), (w, h) = game[0]['at'], game[0]['size']
        path = OUT / f'{STAMP}-{name}.png'
        subprocess.run(['grim', '-g', f'{x},{y} {w}x{h}', str(path)], check=True)
        print(f'  screenshot: {path.name}')
        return str(path)
    except Exception as error:   # a missing screenshot never fails a probe
        print(f'  (no screenshot {name}: {error})')
        return None


def vec(v):
    return (v['X'], v['Y'], v['Z'])


def location(actor):
    return vec(ret(call(actor, 'K2_GetActorLocation')))


def actors_of(cls):
    return call(ctx['statics'], 'GetAllActorsOfClass',
                {'WorldContextObject': ctx['world'], 'ActorClass': cls}).get('OutActors', [])


def tag_count(asc, tag):
    return ret(call(asc, 'GetGameplayTagCount', {'GameplayTag': {'TagName': tag}}))


# ---------------------------------------------------------------- steps
def step_ctx():
    p = eng({'op': 'player'})
    for key in ('pawn', 'controller', 'world'):
        if not is_obj(p.get(key)):
            raise RuntimeError('no player in the world')
        ctx[key] = p[key]
    ctx['statics'] = find('/Script/Engine.Default__GameplayStatics')
    ctx['kismet'] = find('/Script/Engine.Default__KismetSystemLibrary')
    ctx['materials'] = find('/Script/Engine.Default__KismetMaterialLibrary')
    ctx['mesh'] = get(ctx['pawn'], 'Mesh')
    ctx['here'] = location(ctx['pawn'])
    note('ctx', 'shell', p.get('shell'))
    note('ctx', 'position', ctx['here'])
    capsule = get(ctx['pawn'], 'CapsuleComponent')
    ctx['capsule'] = capsule
    note('ctx', 'capsule', {'radius': get(capsule, 'CapsuleRadius'), 'half_height': get(capsule, 'CapsuleHalfHeight')})
    ctx['asc'] = get(ctx['pawn'], 'AbilitySystemComponent')
    note('ctx', 'player_asc', is_obj(ctx['asc']))


def step_census():
    classes = {
        'ai': '/Game/Sparta/Core/AI/BP_AICharacter.BP_AICharacter_C',
        'npc': '/Game/Sparta/Core/AI/BP_NPC.BP_NPC_C',
        'trigger': '/Game/Sparta/Characters/SpawnSystem/BP_SpawnTrigger.BP_SpawnTrigger_C',
        'spawner': '/Game/Sparta/Characters/SpawnSystem/BP_Spawner.BP_Spawner_C',
        'proximity': '/Game/Sparta/Characters/SpawnSystem/BP_Spawner_Proximity.BP_Spawner_Proximity_C',
    }
    started = time.monotonic()
    for key, path in classes.items():
        ctx['class_' + key] = load(path)
        ctx['all_' + key] = actors_of(ctx['class_' + key])
        note('census', key, len(ctx['all_' + key]))
    note('census', 'query_seconds', round(time.monotonic() - started, 2))
    rows = []
    for actor in ctx['all_ai']:
        try:
            pos = location(actor)
            team = get(actor, 'Team')
            health = get(actor, 'HealthComponent')
            dead = ret(call(health, 'IsDeadOrDying')) if is_obj(health) else None
            rows.append({'actor': actor, 'name': actor.get('name', '')[-60:], 'cls': actor.get('class', '').split('.')[-1],
                         'dist': round(math.dist(pos, ctx['here'])), 'team': team.get('TagName') if isinstance(team, dict) else team,
                         'dead': dead, 'hidden': get(actor, 'bHidden')})
        except Exception as error:
            rows.append({'name': actor.get('name', '')[-60:], 'error': str(error)})
    rows.sort(key=lambda r: r.get('dist', 1e9))
    note('census', 'nearest', [{k: v for k, v in r.items() if k != 'actor'} for r in rows[:8]])
    hostile = [r for r in rows if 'actor' in r and str(r.get('team', '')).startswith('Team.Hostile') and not r['dead']
               and r['dist'] < 6000]
    if hostile:
        ctx['enemy'] = hostile[0]['actor']
        note('census', 'chosen_enemy', {k: v for k, v in hostile[0].items() if k != 'actor'})
    else:
        note('census', 'chosen_enemy', 'none within 60 m')


def need_enemy():
    if 'enemy' not in ctx:
        raise RuntimeError('no enemy chosen (run census near an enemy)')
    return ctx['enemy']


def enemy_parts(enemy):
    parts = {name: get(enemy, name) for name in ('Mesh', 'CharacterMovement', 'HitTraceComponent', 'VoxComponent',
                                                'FoleyComponent', 'AbilitySystemComponent', 'Controller', 'BPC_LockOnTarget')}
    brain = get(parts['Controller'], 'BrainComponent') if is_obj(parts['Controller']) else None
    parts['Brain'] = brain
    return parts


def pause_state(parts):
    state = {'bPauseAnims': get(parts['Mesh'], 'bPauseAnims'),
             'mesh_tick': ret(call(parts['Mesh'], 'IsComponentTickEnabled')),
             'move_tick': ret(call(parts['CharacterMovement'], 'IsComponentTickEnabled'))}
    if is_obj(parts['Brain']):
        state['brain_running'] = ret(call(parts['Brain'], 'IsRunning'))
    return state


def step_pause():
    enemy = need_enemy()
    parts = enemy_parts(enemy)
    note('pause', 'parts', {k: is_obj(v) for k, v in parts.items()})
    before = pause_state(parts)
    note('pause', 'before', before)
    call(enemy, 'Pause')
    try:
        time.sleep(0.3)
        note('pause', 'paused', pause_state(parts))
        a = location(enemy); time.sleep(2.5); b = location(enemy)
        note('pause', 'moved_while_paused_cm', round(math.dist(a, b), 1))
        grab('pause')
    finally:
        call(enemy, 'Resume')
    time.sleep(0.5)
    note('pause', 'resumed', pause_state(parts))


def step_hide():
    """Pause, hide (with attached actors), collision off; check nothing falls; restore in reverse."""
    enemy = need_enemy()
    parts = enemy_parts(enemy)
    attached = call(enemy, 'GetAttachedActors', {'bResetArray': True, 'bRecursivelyIncludeAttachedActors': True}).get('OutActors', [])
    note('hide', 'attached_actors', [a.get('class', '').split('.')[-1] for a in attached])
    hidden_before = {json.dumps(a): get(a, 'bHidden') for a in [enemy, *attached]}
    collision_before = ret(call(enemy, 'GetActorEnableCollision'))
    note('hide', 'collision_before', collision_before)
    grab('hide-0-before')
    call(enemy, 'Pause')
    try:
        for actor in [enemy, *attached]:
            call(actor, 'SetActorHiddenInGame', {'bNewHidden': True})
        z0 = location(enemy)[2]
        call(enemy, 'SetActorEnableCollision', {'bNewActorEnableCollision': False})
        time.sleep(2.5)
        note('hide', 'z_drift_cm', round(location(enemy)[2] - z0, 2))
        grab('hide-1-hidden')
    finally:
        call(enemy, 'SetActorEnableCollision', {'bNewActorEnableCollision': bool(collision_before)})
        for actor in [enemy, *attached]:
            call(actor, 'SetActorHiddenInGame', {'bNewHidden': bool(hidden_before[json.dumps(actor)])})
        call(enemy, 'Resume')
    time.sleep(0.7)
    grab('hide-2-restored')
    note('hide', 'restored_hidden', get(enemy, 'bHidden'))


def step_ge():
    """Gameplay effects by class through the bridge: apply, read the tag, remove by handle."""
    asc = ctx['asc']
    out = {}
    effects = {'invulnerable': ('/Game/Sparta/Core/Effects/GE_Invulnerability.GE_Invulnerability_C', 'GameplayEffect.Immunity.Damage'),
               'fall': ('/Game/Sparta/Core/Effects/GE_FallDamageImmunity.GE_FallDamageImmunity_C', None),
               'undetectable': ('/Game/Sparta/Core/Effects/GE_Undetectable.GE_Undetectable_C', 'State.Undetectable')}
    for key, (path, tag) in effects.items():
        cls = load(path)
        before = tag_count(asc, tag) if tag else None
        handle = ret(call(asc, 'BP_ApplyGameplayEffectToSelf', {'GameplayEffectClass': cls, 'Level': 0.0, 'EffectContext': {}}))
        during = tag_count(asc, tag) if tag else None
        removed = ret(call(asc, 'RemoveActiveGameplayEffect', {'Handle': handle, 'StacksToRemove': -1}))
        after = tag_count(asc, tag) if tag else None
        out[key] = {'handle': handle, 'tag': tag, 'before': before, 'during': during, 'after': after, 'removed': removed}
    note('ge', 'player', out)
    if 'enemy' in ctx:
        enemy_asc = get(ctx['enemy'], 'AbilitySystemComponent')
        out = {}
        for key, (path, tag) in {'invisible': ('/Game/Sparta/Core/Effects/GE_AI_Invisible.GE_AI_Invisible_C', 'AI.State.Invisible'),
                                 'immune': ('/Game/Sparta/Core/Effects/GE_HealthDamageImmunity.GE_HealthDamageImmunity_C', None)}.items():
            cls = load(path)
            before = tag_count(enemy_asc, tag) if tag else None
            handle = ret(call(asc, 'BP_ApplyGameplayEffectToTarget',
                              {'GameplayEffectClass': cls, 'Target': enemy_asc, 'Level': 0.0, 'Context': {}}))
            during = tag_count(enemy_asc, tag) if tag else None
            removed = ret(call(enemy_asc, 'RemoveActiveGameplayEffect', {'Handle': handle, 'StacksToRemove': -1}))
            after = tag_count(enemy_asc, tag) if tag else None
            out[key] = {'handle': handle, 'tag': tag, 'before': before, 'during': during, 'after': after, 'removed': removed}
        note('ge', 'enemy', out)


def step_trigger():
    triggers = ctx.get('all_trigger') or []
    if not triggers:
        note('trigger', 'result', 'no spawn trigger loaded')
        return
    far = max(triggers, key=lambda t: math.dist(location(t), ctx['here']))
    before = ret(call(far, 'GetActorEnableCollision'))
    call(far, 'SetActorEnableCollision', {'bNewActorEnableCollision': False})
    try:
        during = ret(call(far, 'GetActorEnableCollision'))
    finally:
        call(far, 'SetActorEnableCollision', {'bNewActorEnableCollision': bool(before)})
    note('trigger', 'collision', {'before': before, 'during': during, 'after': ret(call(far, 'GetActorEnableCollision'))})
    spawners = ctx.get('all_spawner') or []
    if spawners:
        s = spawners[0]
        note('trigger', 'spawner_fields', {'UnitProxy': is_obj(get(s, 'UnitProxy')), 'addons': len(get(s, 'UnitProxyAddons') or []),
                                           'hidden': get(s, 'bHidden')})


def step_lockon():
    pawn, controller = ctx['pawn'], ctx['controller']
    lock = get(pawn, 'LockOnComponent')
    note('lockon', 'locked', get(lock, 'LockedTarget') if is_obj(lock) else 'no component')
    soft = get(pawn, 'Targeting Component')
    note('lockon', 'soft', get(soft, 'LastSoftTargetActor') if is_obj(soft) else 'no component')
    tracker = get(controller, 'Combat Tracker Component')
    if is_obj(tracker):
        note('lockon', 'tracker', {'enemies': len(get(tracker, 'AllValidEnemies') or []), 'in_combat': get(tracker, 'bInCombat')})
    if 'enemy' in ctx:
        marker = get(ctx['enemy'], 'BPC_LockOnTarget')
        if is_obj(marker):
            note('lockon', 'tracking_location', call(marker, 'Get Tracking Location'))
            note('lockon', 'actor_location', location(ctx['enemy']))


def spawn_camera(offset=(260.0, 120.0, 40.0)):
    here = location(ctx['pawn'])
    yaw = ret(call(ctx['pawn'], 'K2_GetActorRotation'))['Yaw']
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    at = (here[0] + offset[0] * c - offset[1] * s, here[1] + offset[0] * s + offset[1] * c, here[2] + offset[2])
    look_yaw = math.degrees(math.atan2(here[1] - at[1], here[0] - at[0]))
    look_pitch = math.degrees(math.atan2(here[2] + 30 - at[2], math.dist(at[:2], here[:2])))
    xf = dict(IDENTITY, Translation={'X': at[0], 'Y': at[1], 'Z': at[2]})
    cls = load('/Script/Engine.CameraActor')
    camera = ret(call(ctx['statics'], 'BeginDeferredActorSpawnFromClass',
                      {'WorldContextObject': ctx['controller'], 'ActorClass': cls, 'SpawnTransform': xf,
                       'CollisionHandlingOverride': 1, 'Owner': None, 'TransformScaleMethod': 1}))
    call(ctx['statics'], 'FinishSpawningActor', {'Actor': camera, 'SpawnTransform': xf, 'TransformScaleMethod': 1})
    call(camera, 'K2_SetActorLocationAndRotation', {'NewLocation': {'X': at[0], 'Y': at[1], 'Z': at[2]},
                                                     'NewRotation': {'Pitch': look_pitch, 'Yaw': look_yaw, 'Roll': 0},
                                                     'bSweep': False, 'bTeleport': True})
    component = get(camera, 'CameraComponent')
    put(component, 'bConstrainAspectRatio', False)
    put(component, 'FieldOfView', 55.0)
    return camera, component, math.dist(at, here)


def view(target, blend=0.0):
    call(ctx['controller'], 'SetViewTargetWithBlend',
         {'NewViewTarget': target, 'BlendTime': blend, 'BlendFunc': 0, 'BlendExp': 0.0, 'bLockOutgoing': False})


def step_camera():
    manager = get(ctx['controller'], 'PlayerCameraManager')
    try:
        note('camera', 'state_priority', get(eng({'op': 'class_default', 'class': 'CSCameraState_CameraActor'}), 'Priority'))
    except Exception as error:
        note('camera', 'state_priority', f'error: {error}')
    camera, component, distance = spawn_camera()
    try:
        view(camera)
        time.sleep(1.0)
        for name in ('ActiveCameraActor',):
            try:
                value = get(manager, name)
                note('camera', name, value.get('class') if is_obj(value) else value)
            except Exception as error:
                note('camera', name, f'error: {error}')
        grab('camera-1-ours')
        put(component, 'PostProcessBlendWeight', 1.0)
        try:
            put(component, 'PostProcessSettings', {'bOverride_DepthOfFieldFstop': True, 'DepthOfFieldFstop': 1.2,
                                                   'bOverride_DepthOfFieldFocalDistance': True, 'DepthOfFieldFocalDistance': distance,
                                                   'bOverride_DepthOfFieldSensorWidth': True, 'DepthOfFieldSensorWidth': 120.0})
            note('camera', 'dof_set', 'ok')
        except RuntimeError as error:
            # The bridge writes the fields, then fails echoing a struct of more than 256 fields.
            note('camera', 'dof_set', f'written, echo failed: {error}')
        time.sleep(1.0)
        grab('camera-2-dof')
        for key, path in {'sobel': '/Game/Sparta/Core/Player/Ability/Materials/M_PP_Sobel.M_PP_Sobel',
                          'depth': '/MovieRenderPipeline/Materials/MovieRenderQueue_WorldDepth.MovieRenderQueue_WorldDepth'}.items():
            try:
                material = load(path)
                call(component, 'AddOrUpdateBlendable', {'InBlendableObject': material, 'InWeight': 1.0})
                time.sleep(0.8)
                grab(f'camera-3-pp-{key}')
                call(component, 'RemoveBlendable', {'InBlendableObject': material})
                note('camera', f'blendable_{key}', 'applied')
            except Exception as error:
                note('camera', f'blendable_{key}', f'error: {error}')
    finally:
        view(ctx['pawn'])
        call(camera, 'K2_DestroyActor')
    note('camera', 'restored', True)


def step_time():
    settings = ret(call(ctx['world'], 'K2_GetWorldSettings'))
    note('time', 'settings_class', settings.get('class') if is_obj(settings) else settings)
    before = get(settings, 'CinematicTimeDilation')
    note('time', 'cinematic_before', before)
    note('time', 'global', ret(call(ctx['statics'], 'GetGlobalTimeDilation', {'WorldContextObject': ctx['world']})))

    def clock():
        return ret(call(ctx['statics'], 'GetTimeSeconds', {'WorldContextObject': ctx['world']})), time.monotonic()

    g0, r0 = clock(); time.sleep(1.5); g1, r1 = clock()
    note('time', 'normal_rate', round((g1 - g0) / (r1 - r0), 3))
    put(settings, 'CinematicTimeDilation', 0.3)
    try:
        g0, r0 = clock(); time.sleep(2.0); g1, r1 = clock()
        note('time', 'slow_rate', round((g1 - g0) / (r1 - r0), 3))
        note('time', 'global_during', ret(call(ctx['statics'], 'GetGlobalTimeDilation', {'WorldContextObject': ctx['world']})))
    finally:
        put(settings, 'CinematicTimeDilation', float(before))
    note('time', 'cinematic_after', get(settings, 'CinematicTimeDilation'))


def add_component(class_path):
    component = ret(call(ctx['pawn'], 'AddComponentByClass', {'Class': load(class_path), 'bManualAttachment': True,
                                                             'RelativeTransform': IDENTITY, 'bDeferredFinish': False}))
    if not is_obj(component):
        raise RuntimeError(f'AddComponentByClass returned {component}')
    return component


def attach(component, bone):
    return ret(call(component, 'K2_AttachToComponent', {'Parent': ctx['mesh'], 'SocketName': bone, 'LocationRule': 2,
                                                        'RotationRule': 2, 'ScaleRule': 2, 'bWeldSimulatedBodies': False}))


def step_shapes():
    """Wire capsule on a bone, the pawn's own capsule shown, and a mesh on a bone."""
    made = []
    capsule_hidden = get(ctx['capsule'], 'bHiddenInGame')
    try:
        capsule = add_component('/Script/Engine.CapsuleComponent'); made.append(capsule)
        call(capsule, 'SetCollisionEnabled', {'NewType': 0})
        note('shapes', 'attach_head', attach(capsule, 'head'))
        call(capsule, 'SetCapsuleSize', {'InRadius': 11.0, 'InHalfHeight': 18.0, 'bUpdateOverlaps': False})
        put(capsule, 'ShapeColor', {'R': 0, 'G': 255, 'B': 255, 'A': 255})
        call(capsule, 'SetLineThickness', {'Thickness': 1.5})
        call(capsule, 'SetHiddenInGame', {'NewHidden': False, 'bPropagateToChildren': False})
        box = add_component('/Script/Engine.BoxComponent'); made.append(box)
        call(box, 'SetCollisionEnabled', {'NewType': 0})
        attach(box, 'hand_r')
        call(box, 'SetBoxExtent', {'InBoxExtent': {'X': 8, 'Y': 3, 'Z': 5}, 'bUpdateOverlaps': False})
        put(box, 'ShapeColor', {'R': 255, 'G': 160, 'B': 0, 'A': 255})
        call(box, 'SetHiddenInGame', {'NewHidden': False, 'bPropagateToChildren': False})
        call(ctx['capsule'], 'SetHiddenInGame', {'NewHidden': False, 'bPropagateToChildren': False})
        time.sleep(0.6)
        note('shapes', 'wire_shot', grab('shapes-1-wire'))
        mesh = add_component('/Script/Engine.StaticMeshComponent'); made.append(mesh)
        call(mesh, 'SetCollisionEnabled', {'NewType': 0})
        note('shapes', 'set_mesh', ret(call(mesh, 'SetStaticMesh', {'NewMesh': load('/Engine/BasicShapes/Cone.Cone')})))
        material = load('/Game/Developers/Zack/Materials/Mat_TranslucentColor_NDT.Mat_TranslucentColor_NDT')
        mid = ret(call(ctx['materials'], 'CreateDynamicMaterialInstance',
                       {'WorldContextObject': ctx['world'], 'Parent': material, 'OptionalName': 'None', 'CreationFlags': 0}))
        call(mid, 'SetVectorParameterValue', {'ParameterName': 'Color', 'Value': {'R': 1.0, 'G': 0.8, 'B': 0.05, 'A': 1.0}})
        call(mesh, 'SetMaterial', {'ElementIndex': 0, 'Material': mid})
        call(mesh, 'SetCastShadow', {'NewCastShadow': False})
        attach(mesh, 'spine_03')
        call(mesh, 'SetRelativeScale3D', {'NewScale3D': {'X': 0.08, 'Y': 0.08, 'Z': 0.25}})
        time.sleep(0.6)
        note('shapes', 'mesh_shot', grab('shapes-2-mesh'))
    finally:
        call(ctx['capsule'], 'SetHiddenInGame', {'NewHidden': bool(capsule_hidden), 'bPropagateToChildren': False})
        for component in made:
            try:
                call(component, 'K2_DestroyComponent', {'Object': component})
            except Exception as error:
                print(f'  destroy failed: {error}')
    note('shapes', 'cleaned', len(made))


def step_skeleton():
    mesh = ctx['mesh']
    started = time.monotonic()
    count = ret(call(mesh, 'GetNumBones'))
    names = [ret(call(mesh, 'GetBoneName', {'BoneIndex': i})) for i in range(count)]
    note('skeleton', 'bones', count)
    note('skeleton', 'seconds_per_call', round((time.monotonic() - started) / max(1, count + 1), 3))
    note('skeleton', 'virtual', [n for n in names if n.startswith('VB ')])
    note('skeleton', 'ik', [n for n in names if n.lower().startswith('ik_')])
    note('skeleton', 'parents', {n: ret(call(mesh, 'GetParentBone', {'BoneName': n})) for n in names[1:6]})
    note('skeleton', 'bone_transform', ret(call(mesh, 'GetBoneTransform', {'InBoneName': 'head', 'TransformSpace': 0})))
    asset = ret(call(mesh, 'GetSkinnedAsset'))
    physics = get(asset, 'PhysicsAsset')
    override = get(mesh, 'PhysicsAssetOverride')
    note('skeleton', 'physics_asset', {'asset': physics.get('name') if is_obj(physics) else physics,
                                       'override': override.get('name') if is_obj(override) else override})
    setups = get(override if is_obj(override) else physics, 'SkeletalBodySetups')
    note('skeleton', 'bodies', len(setups))
    sample = []
    for setup in setups[:3]:
        geom = get(setup, 'AggGeom')
        sample.append({'bone': get(setup, 'BoneName'), 'sphyl': geom.get('SphylElems', [])[:1], 'box': geom.get('BoxElems', [])[:1],
                       'sphere': geom.get('SphereElems', [])[:1]})
    note('skeleton', 'sample', sample)
    components = call(ctx['pawn'], 'K2_GetComponentsByClass', {'ComponentClass': load('/Script/Engine.SkeletalMeshComponent')})
    listed = ret(components) or []
    note('skeleton', 'skeletal_components', [c.get('name', '').split('.')[-1] for c in listed])


def step_clay():
    """Swap every slot of the main mesh to the game's flat test material, then restore."""
    mesh = ctx['mesh']
    count = ret(call(mesh, 'GetNumMaterials'))
    saved = [ret(call(mesh, 'GetMaterial', {'ElementIndex': i})) for i in range(count)]
    note('clay', 'slots', count)
    parent = load('/Game/Sparta/Cutscenes/01_Intro/TestSpheres/M_BaseTestMat.M_BaseTestMat')
    mid = ret(call(ctx['materials'], 'CreateDynamicMaterialInstance',
                   {'WorldContextObject': ctx['world'], 'Parent': parent, 'OptionalName': 'None', 'CreationFlags': 0}))
    call(mid, 'SetVectorParameterValue', {'ParameterName': 'BaseColor', 'Value': {'R': 0.42, 'G': 0.42, 'B': 0.42, 'A': 1.0}})
    try:
        for i in range(count):
            call(mesh, 'SetMaterial', {'ElementIndex': i, 'Material': mid})
        time.sleep(0.8)
        note('clay', 'shot', grab('clay'))
    finally:
        for i, material in enumerate(saved):
            call(mesh, 'SetMaterial', {'ElementIndex': i, 'Material': material})
    note('clay', 'restored', True)


def step_wire():
    """Zero-asset wireframe: the component's bForceWireframe bit on the dynamic draw path."""
    mesh = ctx['mesh']
    command = lambda text: call(ctx['kismet'], 'ExecuteConsoleCommand',
                                {'WorldContextObject': ctx['world'], 'Command': text, 'SpecificPlayer': None})
    before = get(mesh, 'bForceWireframe')
    shadow = get(mesh, 'CastShadow')
    command('r.SkeletalMesh.UseCachedMDCs 0')
    try:
        put(mesh, 'bForceWireframe', True)
        # SetCastShadow marks the render state dirty, which rebuilds the scene proxy.
        call(mesh, 'SetCastShadow', {'NewCastShadow': not shadow})
        call(mesh, 'SetCastShadow', {'NewCastShadow': bool(shadow)})
        time.sleep(1.0)
        note('wire', 'shot', grab('wire'))
    finally:
        put(mesh, 'bForceWireframe', bool(before))
        command('r.SkeletalMesh.UseCachedMDCs 1')
        call(mesh, 'SetCastShadow', {'NewCastShadow': not shadow})
        call(mesh, 'SetCastShadow', {'NewCastShadow': bool(shadow)})
    note('wire', 'restored', True)


def step_pak():
    """CINE's own cooked materials (needs the CSSX_CINE_eins0fx_P pak installed)."""
    root = '/Game/CSSX/CINE/'
    names = {'wire': 'M_Cine_Wire', 'clay': 'M_Cine_Clay', 'xray': 'M_Cine_XRay', 'solid': 'M_Cine_Solid',
             'post': 'PP_Cine_Buffer_Post', 'pre': 'PP_Cine_Buffer_Pre'}
    mesh = ctx['mesh']

    class Mats(dict):
        # Nothing references a freshly loaded asset, so the engine may collect it between two
        # requests: load again right before each use.
        def __getitem__(self, key):
            return load(root + f'Materials/{names[key]}.{names[key]}')
    mats = Mats()

    def mid(parent):
        return ret(call(ctx['materials'], 'CreateDynamicMaterialInstance',
                        {'WorldContextObject': ctx['world'], 'Parent': parent, 'OptionalName': 'None', 'CreationFlags': 0}))

    def collection(name, value):
        call(ctx['materials'], 'SetScalarParameterValue',
             {'WorldContextObject': ctx['world'], 'Collection': load(root + 'MPC_Cine.MPC_Cine'), 'ParameterName': name,
              'ParameterValue': float(value)})

    def wipe(start, end, seconds):
        now = ret(call(ctx['statics'], 'GetTimeSeconds', {'WorldContextObject': ctx['world']}))
        for name, value in (('WipeFrom', start), ('WipeTo', end), ('WipeDur', seconds), ('WipeT0', now)):
            collection(name, value)

    overlay_before = get(mesh, 'OverlayMaterial')
    made = []
    camera = None
    try:
        wipe(1, 1, 0.01)
        # Wireframe and clay as overlays on the shaded mesh, then mid-wipe.
        call(mesh, 'SetOverlayMaterial', {'NewOverlayMaterial': mid(mats['wire'])})
        time.sleep(0.8); note('pak', 'wire_overlay', grab('pak-1-wire-overlay'))
        call(mesh, 'SetOverlayMaterial', {'NewOverlayMaterial': mid(mats['clay'])})
        time.sleep(0.8); note('pak', 'clay_overlay', grab('pak-2-clay-overlay'))
        wipe(0, 1, 4.0); time.sleep(2.0); note('pak', 'clay_mid_wipe', grab('pak-3-clay-mid-wipe'))
        time.sleep(2.5)
        call(mesh, 'SetOverlayMaterial', {'NewOverlayMaterial': overlay_before})
        wipe(1, 1, 0.01)
        # See-through and solid shapes on bones.
        for bone, key, colour in (('head', 'xray', (1.0, 0.8, 0.05)), ('hand_l', 'solid', (0.1, 1.0, 0.3))):
            shape = add_component('/Script/Engine.StaticMeshComponent'); made.append(shape)
            call(shape, 'SetCollisionEnabled', {'NewType': 0})
            call(shape, 'SetStaticMesh', {'NewMesh': load('/Engine/BasicShapes/Sphere.Sphere')})
            instance = mid(mats[key])
            call(instance, 'SetVectorParameterValue', {'ParameterName': 'Color', 'Value': {'R': colour[0], 'G': colour[1], 'B': colour[2], 'A': 1.0}})
            call(shape, 'SetMaterial', {'ElementIndex': 0, 'Material': instance})
            call(shape, 'SetCastShadow', {'NewCastShadow': False})
            attach(shape, bone)
            call(shape, 'SetRelativeScale3D', {'NewScale3D': {'X': 0.12, 'Y': 0.12, 'Z': 0.12}})
        time.sleep(0.8); note('pak', 'shapes', grab('pak-4-xray-solid'))
        # Buffer passes through our own camera, both blend locations.
        camera, component, _ = spawn_camera()
        view(camera); time.sleep(0.8)
        for key in ('post', 'pre'):
            instance = mid(mats[key])
            try:
                call(component, 'AddOrUpdateBlendable', {'InBlendableObject': instance, 'InWeight': 1.0})
            except RuntimeError as error:
                note('pak', f'{key}_blendable', f'needs the newer CSSX core: {error}')
                continue
            for layer in ('W_BaseColor', 'W_Normal', 'W_Depth', 'W_Roughness') + (('W_Lighting',) if key == 'pre' else ()):
                call(instance, 'SetScalarParameterValue', {'ParameterName': layer, 'Value': 1.0})
                time.sleep(0.7); note('pak', f'{key}_{layer}', grab(f'pak-5-{key}-{layer}'))
                call(instance, 'SetScalarParameterValue', {'ParameterName': layer, 'Value': 0.0})
            call(instance, 'SetScalarParameterValue', {'ParameterName': 'W_BaseColor', 'Value': 1.0})
            wipe(0, 1, 3.0); time.sleep(1.5); note('pak', f'{key}_mid_wipe', grab(f'pak-6-{key}-mid-wipe'))
            time.sleep(1.8)
            wipe(1, 1, 0.01)
            call(component, 'RemoveBlendable', {'InBlendableObject': instance})
    finally:
        try:
            call(mesh, 'SetOverlayMaterial', {'NewOverlayMaterial': overlay_before})
        except Exception as error:
            print(f'  overlay restore failed: {error}')
        for name, value in (('WipeFrom', 1), ('WipeTo', 1), ('WipeDur', 1), ('WipeT0', 0)):
            try:
                collection(name, value)
            except Exception:
                pass
        if camera is not None:
            view(ctx['pawn'])
            call(camera, 'K2_DestroyActor')
        for component_made in made:
            try:
                call(component_made, 'K2_DestroyComponent', {'Object': component_made})
            except Exception as error:
                print(f'  destroy failed: {error}')
    note('pak', 'restored', True)


def step_labels():
    """One call for the camera pose, and whether CINE's own projection agrees with the engine's."""
    controller = ctx['controller']
    pov = call(controller, 'GetPlayerViewPoint')
    note('labels', 'view_point', pov)
    loc, rot = vec(pov['Location']), pov['Rotation']
    sp, cp = math.sin(math.radians(rot['Pitch'])), math.cos(math.radians(rot['Pitch']))
    sy, cy = math.sin(math.radians(rot['Yaw'])), math.cos(math.radians(rot['Yaw']))
    sr, cr = math.sin(math.radians(rot['Roll'])), math.cos(math.radians(rot['Roll']))
    fwd = (cp * cy, cp * sy, sp)
    right = (sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp)
    up = (-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp)
    layout = find('/Script/UMG.Default__WidgetLayoutLibrary')

    def engine(point):
        reply = call(layout, 'ProjectWorldLocationToWidgetPosition',
                     {'PlayerController': controller, 'WorldLocation': dict(zip('XYZ', point)), 'bPlayerViewportRelative': False})
        return reply.get('ReturnValue'), reply.get('ScreenPosition')

    def along(depth, r, u):
        return tuple(loc[i] + fwd[i] * depth + right[i] * r + up[i] * u for i in range(3))

    ok0, centre = engine(along(1000, 0, 0))
    ok1, offset = engine(along(1000, 100, 60))
    note('labels', 'centre', centre)
    note('labels', 'offset', offset)
    kx, ky = (offset['X'] - centre['X']) / 0.10, (centre['Y'] - offset['Y']) / 0.06
    note('labels', 'lens', {'kx': kx, 'ky': ky})
    note('labels', 'viewport_size', ret(call(layout, 'GetViewportSize', {'WorldContextObject': ctx['pawn']})))
    note('labels', 'viewport_scale', ret(call(layout, 'GetViewportScale', {'WorldContextObject': ctx['pawn']})))
    worst = 0.0
    for socket in ('head', 'pelvis', 'hand_r', 'foot_l'):
        world = vec(ret(call(ctx['mesh'], 'GetSocketLocation', {'InSocketName': socket})))
        ok, screen = engine(world)
        d = tuple(world[i] - loc[i] for i in range(3))
        depth = sum(d[i] * fwd[i] for i in range(3))
        mine = (centre['X'] + kx * sum(d[i] * right[i] for i in range(3)) / depth,
                centre['Y'] - ky * sum(d[i] * up[i] for i in range(3)) / depth)
        error = math.dist(mine, (screen['X'], screen['Y'])) if ok else None
        worst = max(worst, error or 0)
        note('labels', socket, {'engine': screen, 'mine': mine, 'error_px': error, 'in_front': ok})
    note('labels', 'worst_error_px', worst)
    try:
        note('labels', 'frame', cssx.request(cssx.GAME, {'op': 'status'}, 10).get('result'))
    except Exception as error:
        note('labels', 'frame', str(error))


def step_cleanup():
    """After a world change the effect handles are gone: remove the player's protection by class."""
    asc = ctx['asc']
    cls = load('/Game/Sparta/Core/Effects/GE_Undetectable.GE_Undetectable_C')
    before = tag_count(asc, 'State.Undetectable')
    handle = ret(call(asc, 'BP_ApplyGameplayEffectToSelf', {'GameplayEffectClass': cls, 'Level': 0.0, 'EffectContext': {}}))
    during = tag_count(asc, 'State.Undetectable')
    removed = None
    try:
        removed = call(asc, 'RemoveActiveGameplayEffectBySourceEffect',
                       {'GameplayEffect': cls, 'InstigatorAbilitySystemComponent': asc, 'StacksToRemove': -1})
    finally:
        after = tag_count(asc, 'State.Undetectable')
        if after != before:
            call(asc, 'RemoveActiveGameplayEffect', {'Handle': handle, 'StacksToRemove': -1})
    note('cleanup', 'tag_before_during_after', [before, during, after])
    note('cleanup', 'reply', removed)
    note('cleanup', 'tag_final', tag_count(asc, 'State.Undetectable'))


def step_fight():
    """What a fight take reads on every tick."""
    pawn = ctx['pawn']
    health = get(pawn, 'HealthComponent')
    note('fight', 'player_health', ret(call(health, 'GetHealthNormalized')))
    note('fight', 'player_dead', ret(call(health, 'IsDeadOrDying')))
    manager = get(ctx['controller'], 'PlayerCameraManager')
    note('fight', 'shake_scale', ret(call(manager, 'GetGlobalCameraShakeScale')))
    note('fight', 'active_camera_actor', get(manager, 'ActiveCameraActor'))
    for tag in ('State.Parry', 'State.CustomCamera', 'State.InCutscene'):
        note('fight', tag, tag_count(ctx['asc'], tag))
    settings = ret(call(ctx['world'], 'K2_GetWorldSettings'))
    note('fight', 'cinematic_time', get(settings, 'CinematicTimeDilation'))
    lock = get(pawn, 'LockOnComponent')
    target = get(lock, 'LockedTarget') if is_obj(lock) else None
    note('fight', 'locked', is_obj(target))
    if is_obj(target):
        enemy_health = get(target, 'HealthComponent')
        note('fight', 'target_health', ret(call(enemy_health, 'GetHealthNormalized')))
        note('fight', 'target_dead', ret(call(enemy_health, 'IsDeadOrDying')))


def step_unlock():
    """Needs a lock-on. Hides the target's health bar and marker for a moment, then drops the lock."""
    lock = get(ctx['pawn'], 'LockOnComponent')
    target = get(lock, 'LockedTarget')
    if not is_obj(target):
        note('unlock', 'skipped', 'no lock-on target')
        return
    grab('unlock-0-before')
    bar = get(target, 'Enemy Health Widget')
    note('unlock', 'health_bar', is_obj(bar))
    bar_visible = None
    if is_obj(bar):
        bar_visible = ret(call(bar, 'IsVisible'))
        note('unlock', 'health_bar_visible', bar_visible)
        call(bar, 'SetVisibility', {'bNewVisibility': False, 'bPropagateToChildren': True})
    marker = get(target, 'BPC_LockOnTarget')
    widget = get(marker, 'WBP_LockOnIndicator') if is_obj(marker) else None
    note('unlock', 'indicator', is_obj(widget))
    widget_before = None
    if is_obj(widget):
        widget_before = ret(call(widget, 'GetVisibility'))
        note('unlock', 'indicator_visibility', widget_before)
        call(widget, 'SetVisibility', {'InVisibility': 2})
    time.sleep(0.8)
    grab('unlock-1-ui-hidden')
    if is_obj(bar) and bar_visible:
        call(bar, 'SetVisibility', {'bNewVisibility': True, 'bPropagateToChildren': True})
    if is_obj(widget):
        call(widget, 'SetVisibility', {'InVisibility': widget_before})
    time.sleep(0.5)
    grab('unlock-2-ui-back')
    try:
        call(lock, 'TargetSpecificCharacter', {'Target': None})
        note('unlock', 'after_target_null', get(lock, 'LockedTarget'))
    except Exception as error:
        note('unlock', 'target_null_error', str(error))
    if is_obj(get(lock, 'LockedTarget')):
        note('unlock', 'toggle', call(lock, 'ToggleTargetLock', {'TargetingTag': {'TagName': 'None'}}))
        note('unlock', 'after_toggle', get(lock, 'LockedTarget'))
    time.sleep(0.6)
    grab('unlock-3-released')


STEPS = {'labels': step_labels, 'cleanup': step_cleanup, 'fight': step_fight, 'unlock': step_unlock, 'ctx': step_ctx, 'census': step_census, 'pause': step_pause, 'hide': step_hide, 'ge': step_ge,
         'trigger': step_trigger, 'lockon': step_lockon, 'camera': step_camera, 'time': step_time,
         'shapes': step_shapes, 'skeleton': step_skeleton, 'clay': step_clay, 'wire': step_wire, 'pak': step_pak}


def main():
    wanted = sys.argv[1:]
    if not wanted or wanted == ['list']:
        print(__doc__)
        print('Steps:', ', '.join(STEPS))
        return
    if wanted == ['all']:
        wanted = list(STEPS)
    for implied in ('census', 'ctx'):   # every step needs the context; enemy steps need the census
        if implied not in wanted and (implied == 'ctx' or {'pause', 'hide', 'ge', 'trigger', 'lockon'} & set(wanted)):
            wanted.insert(0, implied)
    for name in wanted:
        print(f'== {name}')
        try:
            STEPS[name]()
        except Exception as error:
            results.setdefault(name, {})['error'] = str(error)
            print(f'  FAILED: {error}')
    path = OUT / f'results-{STAMP}.json'
    path.write_text(json.dumps(results, indent=2, default=str) + '\n')
    print(f'results: {path}')


if __name__ == '__main__':
    main()
