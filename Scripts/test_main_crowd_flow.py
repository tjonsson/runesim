"""Three-minute dense-crowd acceptance in actual MainLevel; restores settings/view."""
import unreal
import time
import json
import collections
from pathlib import Path


def run_crowd_flow():
    world = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
    assert 'MainLevel' in world.get_name()
    system = next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer() == world)
    previous = system.get_options()
    options = system.get_options()
    options.crowd_density = 3
    options.traffic_density = 3
    options.max_actors = 120
    options.planes = 3
    options.helicopters = 3
    options.drones = 17
    options.bird_flocks = 3
    options.flock_size = 12
    options.population = unreal.LivingPopulation.MIXED
    options.enabled = True
    system.apply_options(options)
    unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS 30')
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    original_view = controller.get_view_target()
    ptz = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SimPTZ)[0]
    original_ptz = (ptz.pan.get_editor_property('relative_rotation').yaw,
                    ptz.camera.get_editor_property('relative_rotation').pitch, ptz.camera.field_of_view)
    output = Path(unreal.Paths.project_saved_dir()) / 'LivingWorld'
    started = time.monotonic()
    state = {'samples': [], 'last': 0, 'stationary': {}, 'footsteps': {}, 'closest_gap_cm': 1e9,
             'turned': False, 'moving_people': set(), 'start_frames': ptz.stream.frame_number}

    def finish(error=None):
        unreal.unregister_slate_post_tick_callback(handle)
        ptz.set_ptz(*original_ptz)
        controller.set_view_target_with_blend(original_view)
        recent = [s for s in state['samples'] if s['elapsed'] >= 150]
        checks = {
            'dense_population_observed': any(s['people'] >= 50 for s in state['samples']),
            'majority_keep_moving_late': bool(recent) and sum(s['moving'] / max(1, s['people']) for s in recent) / len(recent) >= .6,
            'no_sustained_mass_queue': bool(recent) and all(s['stopped_30s'] <= s['people'] * .1 for s in recent),
            'people_keep_collision_clearance': state['closest_gap_cm'] >= -1,
            'both_human_profiles_move': len(state['moving_people']) == 2,
            'camera_away_observed': any(s['camera_pitch'] > 60 for s in recent),
            'stream_advances': ptz.stream.is_connected() and ptz.stream.frame_number - state['start_frames'] > 1000,
        }
        report = {'checks': checks, 'passed': error is None and all(checks.values()), 'error': error,
                  'samples': state['samples'], 'closest_gap_cm': state['closest_gap_cm']}
        system.apply_options(previous)
        (output / 'main-crowd-flow.json').write_text(json.dumps(report, indent=2))
        print('MAIN_CROWD_FLOW', report['passed'], checks, error)

    def tick(dt):
        elapsed = time.monotonic() - started
        if elapsed < 20 or elapsed - state['last'] < 1:
            return
        state['last'] = elapsed
        try:
            humans = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.LivingAgent)
                      if a.active and a.profile.kind in (unreal.LivingKind.CIVILIAN, unreal.LivingKind.SOLDIER)]
            active_names = {a.get_name() for a in humans}
            state['stationary'] = {k: v for k, v in state['stationary'].items() if k in active_names}
            moving = 0
            for a in humans:
                name = a.get_name()
                if a.footsteps_played < state['footsteps'].get(name, 0):
                    state['stationary'].pop(name, None)  # A pooled actor began a new route trip.
                state['footsteps'][name] = a.footsteps_played
                if a.velocity.length() > 30:
                    moving += 1
                    state['moving_people'].add(a.profile.get_name())
                    state['stationary'].pop(name, None)
                else:
                    state['stationary'].setdefault(name, elapsed)
            positions = [(a.get_actor_location(), a.profile.collision_radius_cm) for a in humans]
            for i, (p, r) in enumerate(positions):
                for q, s in positions[i+1:]:
                    state['closest_gap_cm'] = min(state['closest_gap_cm'], (p-q).length()-r-s)
            sample = {'elapsed': elapsed, 'people': len(humans), 'moving': moving,
                      'stopped_30s': sum(elapsed - t >= 30 for t in state['stationary'].values()),
                      'blocked': dict(collections.Counter(a.blocked_reason for a in humans if a.blocked_reason)),
                      'camera_pitch': unreal.GameplayStatics.get_player_camera_manager(world, 0).get_camera_rotation().pitch}
            state['samples'].append(sample)
            (output / 'main-crowd-flow-progress.json').write_text(json.dumps(sample, indent=2))
            if elapsed > 90 and not state['turned']:
                controller.set_view_target_with_blend(ptz)
                ptz.set_ptz(160, 80, 85)
                state['turned'] = True
            if elapsed >= 180:
                finish()
        except Exception as error:
            finish(repr(error))

    handle = unreal.register_slate_post_tick_callback(tick)
    print('MAIN_CROWD_FLOW_STARTED')


run_crowd_flow()
