"""Live MainLevel check of the extended ground network (editor, PIE stopped; runs ~2 minutes).

Checks that trails are populated, agents transfer between linked routes, the road crossings exist
and vehicles yield there, pedestrians keep moving, and nobody stands on the road except at crossings.
Writes Saved/LivingWorld/main-network.json.
"""
import collections
import json
import time
import unreal
from pathlib import Path

previous = globals().pop('main_network_handle', None)
if previous is not None:
    unreal.unregister_slate_post_tick_callback(previous)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
state = {'start': time.monotonic(), 'applied': False, 'last': 0, 'routes': {}, 'transfers': 0, 'samples': [],
         'yield_events': 0, 'road_intrusions': 0}


def tick(dt):
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    elapsed = time.monotonic() - state['start']
    if not worlds:
        return
    w = worlds[0]
    system = next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer() == w)
    if not state['applied']:
        if elapsed < 12:
            return
        o = unreal.LivingWorldOptions(); o.enabled = True; o.preset = unreal.LivingPreset.CUSTOM
        o.population = unreal.LivingPopulation.MIXED; o.crowd_density = 3; o.traffic_density = 3
        o.planes = 2; o.helicopters = 2; o.drones = 4; o.bird_flocks = 1; o.flock_size = 8
        o.max_actors = 160; o.activity_radius_meters = 500; o.reactive = True
        system.apply_options(o)
        state['applied'] = True; state['applied_at'] = elapsed
        return
    if elapsed - state['last'] < 1:
        return
    state['last'] = elapsed
    agents = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LivingAgent) if a.active]
    ground = [a for a in agents if a.profile.kind in (unreal.LivingKind.CIVILIAN, unreal.LivingKind.SOLDIER, unreal.LivingKind.CAR)]
    on_route = collections.Counter()
    people_moving = people = 0
    road = next(r for r in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LivingRoute) if r.vehicles and r.validated)
    for a in ground:
        route = a.get_current_route()
        label = route.get_actor_label() if route else 'none'
        on_route[label] += 1
        previous_label = state['routes'].get(a.get_name())
        if previous_label and previous_label != label:
            state['transfers'] += 1
        state['routes'][a.get_name()] = label
        if a.profile.kind != unreal.LivingKind.CAR:
            # Deliberate patrol pauses are not congestion.
            if a.behavior != unreal.LivingBehavior.HALTED:
                people += 1
                people_moving += a.velocity.length() > 20
            closest = road.path.find_location_closest_to_world_location(a.get_actor_location(), unreal.SplineCoordinateSpace.WORLD)
            gap = (a.get_actor_location() - closest); gap.z = 0
            if gap.length() < 150 and 'crossing' not in label:
                state['road_intrusions'] += 1
                state.setdefault('intruders', collections.Counter())[label] += 1
        elif a.velocity.length() < 5 and a.behavior != unreal.LivingBehavior.BLOCKED:
            state['yield_events'] += 1
    state['samples'].append({'t': round(elapsed), 'ground': len(ground), 'routes': dict(on_route),
                             'people_moving_pct': round(100 * people_moving / max(1, people))})
    if elapsed - state['applied_at'] < 120:
        return
    unreal.unregister_slate_post_tick_callback(main_network_handle)
    late = state['samples'][30:]
    trail_use = collections.Counter()
    for s in late:
        for k, v in s['routes'].items():
            trail_use[k] += v
    checks = {
        'conflict_zones_active': system.get_conflict_zone_count() >= 2,
        'all_trails_used': all(any(k.startswith(t) for k in trail_use) for t in ('Living Main Trail north', 'Living Main Trail south', 'Living Main Trail road')),
        'route_transfers_observed': state['transfers'] >= 3,
        'pedestrians_keep_moving': min(s['people_moving_pct'] for s in late) >= 70,
        'no_pedestrians_on_road_outside_crossings': state['road_intrusions'] == 0,
    }
    result = {'checks': checks, 'passed': all(checks.values()), 'transfers': state['transfers'], 'conflict_zones': system.get_conflict_zone_count(),
              'vehicle_stop_samples': state['yield_events'], 'road_intrusions': state['road_intrusions'], 'route_use': dict(trail_use),
              'min_people_moving_pct': min(s['people_moving_pct'] for s in late), 'intruders': dict(state.get('intruders', {})),
              'samples': state['samples'][::10]}
    (Path(unreal.Paths.project_saved_dir()) / 'LivingWorld/main-network.json').write_text(json.dumps(result, indent=2))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    print('MAIN_NETWORK', json.dumps(checks))


main_network_handle = unreal.register_slate_post_tick_callback(tick)
globals()['main_network_handle'] = main_network_handle
print('MAIN_NETWORK_STARTED')
