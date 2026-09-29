"""Observe real MainLevel selection cadence during PTZ motion and an opt-out."""
import unreal, time, json
from pathlib import Path

world = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
assert 'MainLevel' in world.get_name()
tileset = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Cesium3DTileset)[0]
budget = tileset.get_component_by_class(unreal.LivingTerrainBudgetComponent)
assert budget
ptz = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SimPTZ)[0]
initial = (ptz.pan.get_editor_property('relative_rotation').yaw, ptz.camera.get_editor_property('relative_rotation').pitch, ptz.camera.field_of_view)
enabled = budget.get_editor_property('enabled')
start = time.monotonic()
state = {'samples': [], 'moved': False, 'restored': False, 'disabled': False}

def finish(error=None):
    unreal.unregister_slate_post_tick_callback(handle)
    ptz.set_ptz(*initial)
    budget.set_editor_property('enabled', enabled)
    samples = state['samples']
    checks = {
        'stationary_views_use_idle_cadence': any(2 < s['elapsed'] < 5 and s['idle'] for s in samples),
        'camera_motion_restores_full_rate': any(5 < s['elapsed'] < 7 and not s['idle'] and s['interval'] == 0 for s in samples),
        'views_settle_after_turn': any(9 < s['elapsed'] < 12 and s['idle'] for s in samples),
        'opt_out_restores_original_interval': all(s['interval'] == 0 for s in samples if s['elapsed'] > 17),
        'multiple_selection_views_observed': all(s['views'] >= 4 for s in samples),
    }
    result = {'checks': checks, 'passed': all(checks.values()) and error is None, 'samples': samples, 'error': error}
    (Path(unreal.Paths.project_saved_dir())/'LivingWorld/cadence-live.json').write_text(json.dumps(result, indent=2))
    print('CADENCE_LIVE', result['passed'], checks, error)

def tick(dt):
    elapsed = time.monotonic()-start
    try:
        state['samples'].append({'elapsed': elapsed, 'idle': budget.using_idle_cadence,
                                 'interval': tileset.get_actor_tick_interval(), 'views': budget.observed_views})
        if elapsed >= 5 and not state['moved']:
            ptz.set_ptz(100, 50, 20)
            state['moved'] = True
        if elapsed >= 12 and not state['restored']:
            ptz.set_ptz(*initial)
            state['restored'] = True
        if elapsed >= 16 and not state['disabled']:
            budget.set_editor_property('enabled', False)
            state['disabled'] = True
        if elapsed >= 20: finish()
    except Exception as error:
        finish(repr(error))

handle = unreal.register_slate_post_tick_callback(tick)
print('CADENCE_LIVE_STARTED')
