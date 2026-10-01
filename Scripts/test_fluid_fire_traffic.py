"""PIE check: a fluid fire on the road with full traffic must not gather Living World agents as obstacles."""
import json, math, time, unreal
from pathlib import Path
prev = globals().pop('fluid_handle', None)
if prev is not None: unreal.unregister_slate_post_tick_callback(prev)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
saved = Path(unreal.Paths.project_saved_dir()) / 'LivingWorld'
st = {'t': time.monotonic()}
def tick(dt):
    ws = unreal.EditorLevelLibrary.get_pie_worlds(False)
    if not ws: return
    w = ws[0]; e = time.monotonic() - st['t']
    war = next(o for o in unreal.ObjectIterator(unreal.SimWarEffects) if o.get_outer() == w)
    if 'opts' not in st and e > 8:
        system = next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer() == w)
        o = unreal.LivingWorldOptions(); o.enabled = True; o.preset = unreal.LivingPreset.CUSTOM
        o.population = unreal.LivingPopulation.MIXED; o.traffic_density = 3; o.crowd_density = 3
        o.planes = 3; o.helicopters = 3; o.drones = 12; o.bird_flocks = 3; o.max_actors = 120; o.activity_radius_meters = 500
        system.apply_options(o); st['opts'] = True
    if 'fire' not in st and e > 30:
        road = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LivingRoute) if 'Dirt road' in a.get_actor_label())
        p = road.get_editor_property('path').get_location_at_distance_along_spline(12000, unreal.SplineCoordinateSpace.WORLD)
        st['fire'] = war.start_fire(p, 1.5, 30.)
        # Park the tripod capture close by so the fire earns fluid detail.
        capture = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.SimPTZ)[0].get_editor_property('capture')
        eye = p + unreal.Vector(-3000, -3000, 2000)
        capture.set_world_location_and_rotation(eye, unreal.MathLibrary.find_look_at_rotation(eye, p), False, False)
        capture.fov_angle = 45
        st['cars'] = len([a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LivingAgent) if a.active])
    if 'fire' in st and e > 45 and 'done' not in st:
        st['done'] = True
        st['fluid'] = war.active_fluid_fire_count()
        unreal.unregister_slate_post_tick_callback(fluid_handle)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        r = {'active_agents': st['cars'], 'fluid_fires': st['fluid']}
        (saved / 'fluid-fire-traffic.json').write_text(json.dumps(r))
        print('FLUID_TRAFFIC', json.dumps(r))
fluid_handle = unreal.register_slate_post_tick_callback(tick)
globals()['fluid_handle'] = fluid_handle
