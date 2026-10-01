"""MainLevel PIE check for the September 30 / October 1 asset batches.

Each new profile (Humvee, Sedan, Ariete, TB2, CivilianMan, Shahed-136, FPV drone) must spawn, move, and, for vehicles and
aircraft, be engageable. A borrowed scene capture photographs one agent of each profile in the live
Cesium scene: Saved/LivingWorld/NewAssets-<Profile>.png. Results: Saved/LivingWorld/main-new-assets.json.
"""
import json, time, unreal
from pathlib import Path
previous = globals().pop('new_assets_handle', None)
if previous is not None:
    unreal.unregister_slate_post_tick_callback(previous)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
NEW = {'DA_Humvee': 900, 'DA_Sedan': 850, 'DA_Ariete': 1400, 'DA_TB2': 2600, 'DA_CivilianMan': 350,
       'DA_Shahed136': 700, 'DA_FPVDrone': 160}
saved = Path(unreal.Paths.project_saved_dir()) / 'LivingWorld'
state = {'start': time.monotonic(), 'applied': False, 'seen': {}, 'shots': {}, 'capture': None}


def photograph(world, agent, distance):
    # Python cannot spawn into the PIE world, so borrow an existing scene capture (a sensor or
    # tripod camera) for one frame and restore its pose, target and source afterwards.
    if state['capture'] is None:
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
            found = actor.get_components_by_class(unreal.SceneCaptureComponent2D)
            if found:
                state['capture'] = found[0]
                break
        state['target'] = unreal.RenderingLibrary.create_render_target2d(world, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    component = state['capture']
    saved_pose = (component.get_world_location(), component.get_world_rotation())
    saved_target, saved_source, saved_fov = component.texture_target, component.capture_source, component.fov_angle
    target = agent.get_actor_location()
    forward, right, up = agent.get_actor_forward_vector(), agent.get_actor_right_vector(), agent.get_actor_up_vector()
    eye = target + forward * (distance * .7) + right * (distance * .7) + up * (distance * .35)
    component.set_world_location_and_rotation(eye, unreal.MathLibrary.find_look_at_rotation(eye, target), False, False)
    component.texture_target = state['target']
    component.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    component.fov_angle = 50
    component.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, state['target'], str(saved), f"NewAssets-{agent.profile.get_name()[3:]}.png")
    component.texture_target, component.capture_source, component.fov_angle = saved_target, saved_source, saved_fov
    component.set_world_location_and_rotation(saved_pose[0], saved_pose[1], False, False)


def tick(dt):
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    elapsed = time.monotonic() - state['start']
    if not worlds:
        return
    world = worlds[0]
    system = next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer() == world)
    if not state.get('cleared') and elapsed > 8:
        # Start from empty traffic so saved editor options (e.g. a Civilians population) cannot pre-fill the cars.
        o = unreal.LivingWorldOptions(); o.enabled = True; o.preset = unreal.LivingPreset.CUSTOM
        o.traffic_density = 0; o.crowd_density = 0; o.planes = 0
        system.apply_options(o); state['cleared'] = True
    if not state['applied'] and elapsed > 12:
        o = unreal.LivingWorldOptions(); o.enabled = True; o.preset = unreal.LivingPreset.CUSTOM
        o.population = unreal.LivingPopulation.MIXED
        o.planes = 8; o.helicopters = 0; o.drones = 16; o.bird_flocks = 0; o.crowd_density = 3; o.traffic_density = 3
        o.max_actors = 120; o.activity_radius_meters = 500; o.combat_targets = True
        system.apply_options(o); state['applied'] = True
    for agent in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.LivingAgent):
        name = agent.profile.get_name() if agent.active and agent.profile and state['applied'] else None
        if name not in NEW:
            continue
        entry = state['seen'].setdefault(name, {'agents': set(), 'moving': False, 'engageable': False})
        entry['agents'].add(agent.get_name())
        first = state.setdefault('first_seen', {}).setdefault(agent.get_name(), elapsed)
        entry['moving'] |= agent.velocity.length() > 50
        entry['engageable'] |= bool(agent.target_component) and agent.target_component.can_be_engaged()
        # Photograph once the agent is moving and the scene has had time to stream in.
        # Only agents active for a while: a freshly recycled agent can show its bind pose for a frame.
        if elapsed > 45 and elapsed - first > 5 and name not in state['shots'] and agent.velocity.length() > 50:
            photograph(world, agent, NEW[name])
            state['shots'][name] = agent.get_name()
    if elapsed > 110:
        unreal.unregister_slate_post_tick_callback(new_assets_handle)
        result = {k: {'agents': len(v['agents']), 'moving': v['moving'], 'engageable': v['engageable'], 'photo': k in state['shots']}
                  for k, v in state['seen'].items()}
        result['missing'] = [k for k in NEW if k not in state['seen']]
        (saved / 'main-new-assets.json').write_text(json.dumps(result, indent=2))
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        print('NEW_ASSETS', json.dumps(result))


new_assets_handle = unreal.register_slate_post_tick_callback(tick)
globals()['new_assets_handle'] = new_assets_handle
