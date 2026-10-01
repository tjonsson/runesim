"""Short MainLevel PIE check: every approved aircraft profile spawns, moves and is engageable."""
import json, time, unreal
from pathlib import Path
previous = globals().pop('aircraft_mix_handle', None)
if previous is not None:
    unreal.unregister_slate_post_tick_callback(previous)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
state = {'start': time.monotonic(), 'applied': False, 'seen': {}}
def tick(dt):
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    elapsed = time.monotonic() - state['start']
    if not worlds:
        return
    w = worlds[0]
    system = next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer() == w)
    if not state['applied'] and elapsed > 10:
        o = unreal.LivingWorldOptions(); o.enabled = True; o.preset = unreal.LivingPreset.CUSTOM
        o.planes = 8; o.helicopters = 0; o.drones = 0; o.bird_flocks = 0; o.crowd_density = 0; o.traffic_density = 0
        o.max_actors = 60; o.activity_radius_meters = 500; o.combat_targets = True
        system.apply_options(o); state['applied'] = True
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LivingAgent):
        if a.active and a.profile.kind == unreal.LivingKind.PLANE:
            e = state['seen'].setdefault(a.profile.get_name(), {'count': set(), 'moving': False, 'engageable': False})
            e['count'].add(a.get_name()); e['moving'] |= a.velocity.length() > 1000
            e['engageable'] |= bool(a.target_component) and a.target_component.can_be_engaged()
    if elapsed > 60:
        unreal.unregister_slate_post_tick_callback(aircraft_mix_handle)
        result = {k: {'agents': len(v['count']), 'moving': v['moving'], 'engageable': v['engageable']} for k, v in state['seen'].items()}
        (Path(unreal.Paths.project_saved_dir()) / 'LivingWorld/main-aircraft-mix.json').write_text(json.dumps(result, indent=2))
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        print('AIRCRAFT_MIX', json.dumps(result))
aircraft_mix_handle = unreal.register_slate_post_tick_callback(tick)
globals()['aircraft_mix_handle'] = aircraft_mix_handle
