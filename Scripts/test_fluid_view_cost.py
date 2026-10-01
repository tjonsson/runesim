"""PIE frame-time check: a fluid fire in the main view vs one seen only through a scene capture.

Phase A: baseline. Phase B: war-layer fire 30 m in front of the PIE camera (fluid via the pool, hidden from
captures). Phase C: a raw NS_War_FluidFire seen only by the tripod capture (parked looking at it), which is
the configuration that stalled the packaged build. Writes Saved/LivingWorld/fluid-view-cost.json.
"""
import json, time, unreal
from pathlib import Path
prev = globals().pop('cost_handle', None)
if prev is not None: unreal.unregister_slate_post_tick_callback(prev)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
saved = Path(unreal.Paths.project_saved_dir()) / 'LivingWorld'
st = {'t': time.monotonic(), 'phase': 'wait', 'dts': [], 'r': {}}
def tick(dt):
    ws = unreal.EditorLevelLibrary.get_pie_worlds(False)
    if not ws: return
    w = ws[0]; e = time.monotonic() - st['t']; r = st['r']
    war = next(o for o in unreal.ObjectIterator(unreal.SimWarEffects) if o.get_outer() == w)
    cam = unreal.GameplayStatics.get_player_camera_manager(w, 0)
    if st['phase'] == 'wait' and e > 15:
        unreal.SystemLibrary.execute_console_command(w, 't.IdleWhenNotForeground 0')
        unreal.SystemLibrary.execute_console_command(w, 't.MaxFPS 0')
        st['phase'] = 'A'; st['at'] = e; st['dts'] = []
    elif st['phase'] in ('A', 'B', 'C'):
        if e - st['at'] > 2: st['dts'].append(dt)
        if e - st['at'] > 10:
            d = sorted(st['dts']); r[st['phase']] = {'frames': len(d), 'median_ms': round(1000 * d[len(d) // 2], 1) if d else None, 'p90_ms': round(1000 * d[int(len(d) * .9)], 1) if d else None}
            if st['phase'] == 'A':
                p = cam.get_camera_location() + cam.get_camera_rotation().get_forward_vector() * 3000
                st['fire'] = war.start_fire(p, 1.5, 60.); st['phase'] = 'B'
            elif st['phase'] == 'B':
                r['B_fluid'] = war.active_fluid_fire_count(); war.stop_all_fires()
                # A raw fluid fire behind the PIE camera, seen only by the parked tripod capture.
                p = cam.get_camera_location() - cam.get_camera_rotation().get_forward_vector() * 6000
                st['raw'] = unreal.NiagaraFunctionLibrary.spawn_system_at_location(w, unreal.load_asset('/Game/LivingWorld/Effects/War/NS_War_FluidFire'), p)
                capture = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.SimPTZ)[0].get_editor_property('capture')
                eye = p + unreal.Vector(-2500, -2500, 1500)
                capture.set_world_location_and_rotation(eye, unreal.MathLibrary.find_look_at_rotation(eye, p), False, False)
                capture.fov_angle = 40; st['phase'] = 'C'
            else:
                st['raw'].destroy_component(st['raw']) if hasattr(st['raw'], 'destroy_component') else None
                unreal.unregister_slate_post_tick_callback(cost_handle)
                unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
                (saved / 'fluid-view-cost.json').write_text(json.dumps(r, indent=2)); print('FLUID_COST', json.dumps(r)); return
            st['at'] = e; st['dts'] = []
cost_handle = unreal.register_slate_post_tick_callback(tick)
globals()['cost_handle'] = cost_handle
