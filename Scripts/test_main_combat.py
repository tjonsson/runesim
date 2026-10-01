"""Live MainLevel acceptance for engagement, shoot-down, bird landing, sensor feeds, patrols and replay.

Run through ue_remote.py with the editor on MainLevel and PIE stopped; the script starts PIE,
runs ~200 s asynchronously and writes Saved/LivingWorld/main-combat.json. Engagement is
virtual (Unreal actors only); ROS and the external FC are not used by this test.
"""
import collections
import json
import time
import unreal
from pathlib import Path

OUT = Path(unreal.Paths.project_saved_dir()) / 'LivingWorld'
previous = globals().pop('main_combat_handle', None)
if previous is not None:
    unreal.unregister_slate_post_tick_callback(previous)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
# Note: an unfocused editor throttles itself, so capture_fps here is informational;
# frame-rate acceptance uses the packaged build.

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()

state = {'start': time.monotonic(), 'last': 0, 'phase': 'warmup', 'samples': [], 'engagements': [], 'downed': {},
         'crashed': set(), 'perched': set(), 'halted_soldiers': set(), 'flight_states': collections.Counter(),
         'sensor': {}, 'current': None, 'errors': []}


def world():
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    return worlds[0] if worlds else None


def finish(w, system, ptz):
    unreal.unregister_slate_post_tick_callback(main_combat_handle_ref[0])

    state['capture_fps'] = (ptz.stream.frame_number - state.get('frames0', 0)) / max(1., time.monotonic() - state['start'] - state.get('frames0_t', 0))
    if system.recording_status and 'Recording' in system.recording_status:
        system.toggle_recording()
    replay_ok = system.play_last_recording()
    replay = system.replay
    log = sorted((OUT / 'Combat').glob('Engagements_*.jsonl'))
    log_events = collections.Counter()
    for path in log:
        for line in path.read_text(encoding='utf-8').splitlines()[-2000:]:
            try:
                log_events[json.loads(line)['type']] += 1
            except Exception:
                pass
    hits = sum(1 for e in state['engagements'] if e.get('result', '').startswith(('Target destroyed', 'Hit')))
    checks = {
        'mainlevel_with_cesium': bool(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Cesium3DTileset)),
        'three_engagements_launched': ptz.launches >= 3,
        'at_least_two_hits': hits >= 2,
        'shot_down_agent_fell_and_crashed': len(state['crashed']) >= 1,
        'downed_agents_observed': len(state['downed']) >= 1,
        'seeker_feed_present': bool(ptz.seeker_camera) and ptz.seeker_camera.stream.stream_id == 'ptz-1-seeker',
        'two_sensor_feeds_carried': state['sensor'].get('carried', 0) == 2,
        'runtime_perches_created': state.get('perches', 0) > 0,
        'birds_landed_and_perched': len(state['perched']) >= 1,
        'soldiers_pause_on_patrol': len(state['halted_soldiers']) >= 1,
        'engagement_log_written': log_events.get('launch', 0) >= 3 and log_events.get('detonation', 0) + log_events.get('miss', 0) >= 1,
        'recording_replays_events': bool(replay_ok) and replay is not None and replay.event_count >= 3,
        'stream_frames_advance': state['capture_fps'] > 1,
    }
    result = {k: v for k, v in state.items() if k not in ('start',)}
    result.update({'checks': checks, 'passed': all(checks.values()), 'hits': hits, 'launches': ptz.launches,
                   'ptz_hits': ptz.hits, 'ptz_misses': ptz.misses, 'log_events': dict(log_events),
                   'crashed': sorted(state['crashed']), 'perched': sorted(state['perched']),
                   'halted_soldiers': sorted(state['halted_soldiers']), 'flight_states': dict(state['flight_states']),
                   'replay_events': replay.event_count if replay else 0, 'replay_effects': replay.effects_played if replay else 0})
    result['downed'] = {k: v for k, v in state['downed'].items()}
    ptz.execute_engagement_command('clear'); ptz.set_ptz(30, 18, 85)
    (OUT / 'main-combat.json').write_text(json.dumps(result, indent=2, default=str))
    print('MAIN_COMBAT', json.dumps(checks))


def tick(dt):
    try:
        w = world()
        elapsed = time.monotonic() - state['start']
        if not w:
            if elapsed > 120:
                unreal.unregister_slate_post_tick_callback(main_combat_handle_ref[0]); state['errors'].append('PIE never started')
            return
        if elapsed - state['last'] < .5:
            return
        state['last'] = elapsed
        system = next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer() == w)
        ptz = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.SimPTZ)[0]
        if state['phase'] == 'warmup':
            if elapsed < 15:
                return
            options = unreal.LivingWorldOptions()
            options.enabled = True; options.preset = unreal.LivingPreset.CUSTOM; options.population = unreal.LivingPopulation.MIXED
            options.planes = 4; options.helicopters = 4; options.drones = 10; options.bird_flocks = 4; options.flock_size = 10
            options.crowd_density = 1; options.traffic_density = 1; options.max_actors = 120; options.activity_radius_meters = 500
            options.sensor_streams = 2; options.combat_targets = True; options.runtime_perches = True; options.reactive = True
            system.apply_options(options)
            unreal.SystemLibrary.execute_console_command(w, 't.MaxFPS 30')
            state['frames0'] = ptz.stream.frame_number; state['frames0_t'] = elapsed
            state['phase'] = 'populate'; state['phase_start'] = elapsed
            return
        agents = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LivingAgent) if a.active]
        for a in agents:
            name = a.get_name()
            if a.behavior == unreal.LivingBehavior.DOWNED and name not in state['downed']:
                state['downed'][name] = {'kind': str(a.profile.kind), 't': round(elapsed, 1), 'z': a.get_actor_location().z}
            if a.profile.kind == unreal.LivingKind.BIRD:
                state['flight_states'][str(a.flight_state)] += 1
                if a.flight_state == unreal.LivingFlightState.PERCHED:
                    state['perched'].add(name)
            if a.profile.kind == unreal.LivingKind.SOLDIER and a.behavior == unreal.LivingBehavior.HALTED:
                state['halted_soldiers'].add(name)
        active_names = {a.get_name() for a in agents}
        for name, info in state['downed'].items():
            if name not in active_names and name not in state['crashed']:
                state['crashed'].add(name)
        state['perches'] = system.get_runtime_perch_count()
        cams = system.get_sensor_cameras()
        state['sensor'] = {'count': len(cams), 'ids': [c.stream.stream_id for c in cams], 'carried': sum(1 for c in cams if c.carrier)}
        if state['phase'] == 'populate':
            if elapsed - state['phase_start'] < 20:
                return
            system.toggle_recording()
            state['phase'] = 'engage'; state['engagement_start'] = elapsed
        if state['phase'] == 'engage':
            current = state['current']
            if current is None:
                if len(state['engagements']) >= 4 or elapsed - state['engagement_start'] > 150:
                    state['phase'] = 'observe'; state['observe_start'] = elapsed
                    return
                ok = ptz.designate_target(False)
                target = ptz.designated_target
                if not ok or not target:
                    state['engagements'].append({'t': round(elapsed, 1), 'result': 'no target: ' + ptz.engagement_status})
                    return
                ptz.execute_engagement_command('track_on')
                state['current'] = {'t': round(elapsed, 1), 'target': target.get_name(),
                                    'kind': str(target.profile.kind) if isinstance(target, unreal.LivingAgent) else 'other',
                                    'status': ptz.engagement_status, 'launch_at': elapsed + 3}
                return
            if 'launched' not in current and elapsed >= current['launch_at']:
                projectile = ptz.launch_interceptor()
                current['launched'] = bool(projectile)
                current['launch_status'] = ptz.engagement_status
                current['launch_t'] = round(elapsed, 1)
                if not projectile:
                    current['result'] = 'launch refused: ' + ptz.engagement_status
                    state['engagements'].append(current); state['current'] = None
                return
            if current.get('launched') and ptz.get_interceptors_in_flight() == 0:
                current['result'] = ptz.engagement_status
                current['resolved_t'] = round(elapsed, 1)
                state['engagements'].append(current); state['current'] = None
                ptz.execute_engagement_command('clear')
                return
            if current.get('launched') and elapsed - current['launch_t'] > 30:
                current['result'] = 'timeout: ' + ptz.engagement_status
                state['engagements'].append(current); state['current'] = None
            return
        if state['phase'] == 'observe':
            if elapsed - state['observe_start'] < 70:
                return
            system.toggle_recording()
            finish(w, system, ptz)
    except Exception as error:
        unreal.unregister_slate_post_tick_callback(main_combat_handle_ref[0])
        (OUT / 'main-combat-error.txt').write_text(repr(error))
        raise


main_combat_handle_ref = [None]
main_combat_handle_ref[0] = unreal.register_slate_post_tick_callback(tick)
globals()['main_combat_handle'] = main_combat_handle_ref[0]
print('MAIN_COMBAT_STARTED')
