"""MainLevel PIE check: military traffic (jeep, HMMWV, rigid Ariete) keeps queue spacing.

Samples every 0.5 s for 60 s after traffic settles and counts vehicle pairs whose centres are closer
than 80% of the sum of their half-lengths (mesh length along the direction of travel).
Result: Saved/LivingWorld/main-vehicle-spacing.json.
"""
import json, time, unreal
from pathlib import Path
prev = globals().pop('spacing_handle', None)
if prev is not None: unreal.unregister_slate_post_tick_callback(prev)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
HALF = {'DA_Jeep': 205, 'DA_Humvee': 247, 'DA_Ariete': 472, 'DA_Sedan': 235}
st = {'t': time.monotonic(), 'phase': 0, 'next': 0, 'samples': 0, 'overlaps': 0, 'worst': None, 'profiles': set()}
def tick(dt):
    ws = unreal.EditorLevelLibrary.get_pie_worlds(False)
    if not ws: return
    w = ws[0]; e = time.monotonic() - st['t']
    s = next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer() == w)
    if st['phase'] == 0 and e > 8:
        o = unreal.LivingWorldOptions(); o.enabled = True; o.preset = unreal.LivingPreset.CUSTOM
        o.population = unreal.LivingPopulation.MILITARY; o.traffic_density = 3; o.crowd_density = 0
        o.planes = 0; o.helicopters = 0; o.drones = 0; o.bird_flocks = 0; o.max_actors = 60; o.activity_radius_meters = 500
        s.apply_options(o); st['phase'] = 1
    if st['phase'] == 1 and e > 40 and e > st['next']:
        st['next'] = e + .5; st['samples'] += 1
        cars = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LivingAgent)
                if a.active and a.profile and a.profile.get_name() in HALF]
        for i, a in enumerate(cars):
            st['profiles'].add(a.profile.get_name())
            for b in cars[i + 1:]:
                d = a.get_actor_location().distance(b.get_actor_location())
                limit = .8 * (HALF[a.profile.get_name()] + HALF[b.profile.get_name()])
                if d < limit:
                    st['overlaps'] += 1
                    if st['worst'] is None or d / limit < st['worst'][0]:
                        st['worst'] = (round(d / limit, 2), a.profile.get_name(), b.profile.get_name())
    if st['phase'] == 1 and e > 100:
        unreal.unregister_slate_post_tick_callback(spacing_handle)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        result = {'samples': st['samples'], 'overlapping_pair_samples': st['overlaps'], 'worst': st['worst'], 'profiles': sorted(st['profiles'])}
        (Path(unreal.Paths.project_saved_dir()) / 'LivingWorld/main-vehicle-spacing.json').write_text(json.dumps(result, indent=2))
        print('VEHICLE_SPACING', json.dumps(result))
spacing_handle = unreal.register_slate_post_tick_callback(tick)
globals()['spacing_handle'] = spacing_handle
