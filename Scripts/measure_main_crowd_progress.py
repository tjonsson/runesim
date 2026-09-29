"""Measure actual corridor progress over 60 seconds, excluding pooled respawns."""
import unreal
import time
import json
from pathlib import Path


def measure_crowd_progress():
    world = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
    assert 'MainLevel' in world.get_name()
    routes = [r for r in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.LivingRoute)
              if r.validated and not r.vehicles]
    assert len(routes) == 1, 'This measurement targets the single reviewed MainLevel walking corridor'
    path = routes[0].path
    started = time.monotonic()
    state = {'last': 0, 'tracks': {}, 'trips': []}

    def close_trip(track):
        if track['seconds'] >= 20:
            state['trips'].append({'seconds': track['seconds'], 'progress_cm': abs(track['distance']-track['start_distance'])})

    def tick(dt):
        elapsed = time.monotonic()-started
        if elapsed-state['last'] < 1:
            return
        interval = elapsed-state['last']
        state['last'] = elapsed
        try:
            humans = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.LivingAgent)
                      if a.active and a.profile.kind in (unreal.LivingKind.CIVILIAN, unreal.LivingKind.SOLDIER)]
            present = {a.get_name() for a in humans}
            for name in list(state['tracks']):
                if name not in present:
                    close_trip(state['tracks'].pop(name))
            for a in humans:
                name = a.get_name()
                p = a.get_actor_location()
                distance = path.get_distance_along_spline_at_spline_input_key(path.find_input_key_closest_to_world_location(p))
                track = state['tracks'].get(name)
                if track and ((p-track['position']).length() > 600*interval or a.footsteps_played < track['steps']):
                    close_trip(track)
                    track = None
                if track is None:
                    track = {'seconds': 0, 'start_distance': distance}
                    state['tracks'][name] = track
                else:
                    track['seconds'] += interval
                track.update(position=p, distance=distance, steps=a.footsteps_played)
            if elapsed < 60:
                return
            for track in state['tracks'].values():
                close_trip(track)
            trips = state['trips']
            moving = sum(t['progress_cm'] >= 300 for t in trips)
            result = {'elapsed': elapsed, 'eligible_trips': len(trips), 'progressing_trips': moving,
                      'passed': len(trips) >= 30 and moving/len(trips) >= .8, 'trips': trips}
            unreal.unregister_slate_post_tick_callback(handle)
            (Path(unreal.Paths.project_saved_dir())/'LivingWorld/main-crowd-progress.json').write_text(json.dumps(result, indent=2))
            print('MAIN_CROWD_PROGRESS', result['passed'], moving, len(trips))
        except Exception as error:
            unreal.unregister_slate_post_tick_callback(handle)
            (Path(unreal.Paths.project_saved_dir())/'LivingWorld/main-crowd-progress.json').write_text(json.dumps({'passed': False, 'error': repr(error)}))
            raise

    handle = unreal.register_slate_post_tick_callback(tick)
    print('MAIN_CROWD_PROGRESS_STARTED')


measure_crowd_progress()
