"""MainLevel PIE check of the battlefield effects through the tripod's engagement commands.

Aims the tripod at the ground 150-700 m away, then issues the same commands ROS /engage accepts:
strike, burn and smoke_screen. Photographs the tripod view (flash, smoke column, flames with fluid fire,
smoke screen) to Saved/LivingWorld/War-*.png and records fire/fluid counts in main-war-effects.json.
"""
import json, time, unreal
from pathlib import Path
prev = globals().pop('war_handle', None)
if prev is not None: unreal.unregister_slate_post_tick_callback(prev)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
saved = Path(unreal.Paths.project_saved_dir()) / 'LivingWorld'
st = {'t': time.monotonic(), 'step': 0, 'at': 0., 'result': {}, 'pan': None}


def photo(world, ptz, name):
    # Borrow the tripod capture with an LDR target (the stream's own target is HDR), then restore it.
    capture = ptz.get_editor_property('capture')
    if 'ldr' not in st:
        st['ldr'] = unreal.RenderingLibrary.create_render_target2d(world, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    target, source = capture.texture_target, capture.capture_source
    capture.texture_target = st['ldr']; capture.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    capture.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, st['ldr'], str(saved), f'War-{name}.png')
    capture.texture_target, capture.capture_source = target, source


def point_at(ptz, target, fov):
    """Steer pan/tilt until the tripod camera looks at target (a few fixed-point iterations)."""
    import math
    camera = ptz.get_editor_property('camera')
    pan, tilt = 0., 0.
    for _ in range(25):
        ptz.set_ptz(pan, tilt, fov)
        f = camera.get_forward_vector(); d = (target - camera.get_world_location()); d = d / d.length()
        yaw = math.degrees(math.atan2(d.y, d.x) - math.atan2(f.y, f.x)); yaw = (yaw + 180) % 360 - 180
        pitch = math.degrees(math.asin(max(-1, min(1, d.z))) - math.asin(max(-1, min(1, f.z))))
        pan += yaw; tilt += pitch
        if abs(yaw) < .05 and abs(pitch) < .05: break
    return pan, tilt


def tick(dt):
    ws = unreal.EditorLevelLibrary.get_pie_worlds(False)
    if not ws: return
    w = ws[0]; e = time.monotonic() - st['t']
    ptzs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.SimPTZ)
    if not ptzs: return
    ptz = ptzs[0]
    war = next(o for o in unreal.ObjectIterator(unreal.SimWarEffects) if o.get_outer() == w)
    r = st['result']
    if st['step'] == 0 and 8 < e and not st.get('quiet'):
        # A clean view: no traffic, people or aircraft between the tripod and the effects.
        system = next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer() == w)
        o = unreal.LivingWorldOptions(); o.enabled = True; o.preset = unreal.LivingPreset.CUSTOM
        o.traffic_density = 0; o.crowd_density = 0; o.planes = 0; o.helicopters = 0; o.drones = 0; o.bird_flocks = 0
        system.apply_options(o); st['quiet'] = True
    if st['step'] == 0 and e > 15:
        # Keep the tripod's normal heading and tilt down until the crosshair meets ground 150-800 m away.
        best = None
        for pan in (25, -25, 35, -35, 15, -15, 0):  # off the road axis: traffic passes right by the tripod
            for tilt in [-t * .5 for t in range(2, 50)]:
                ptz.set_ptz(pan, tilt, 14)
                hit = ptz.aim_point()  # Python returns the out location, or None when nothing is hit.
                if hit is not None:
                    d = hit.distance(ptz.get_actor_location()) / 100
                    if 120 < d < 400:
                        best = (pan, tilt, d)
                    break
            if best: break
        r['aim'] = best
        if not best:
            st['step'] = 99; return
        st['pan'] = best
        ptz.set_ptz(best[0], best[1], 14)
        st['step'] = 1; st['at'] = e
    elif st['step'] == 1 and e > st['at'] + 2:
        r['strike'] = ptz.execute_engagement_command('strike')
        st['step'] = 2; st['at'] = e
    elif st['step'] == 2 and e > st['at'] + .15:
        photo(w, ptz, 'StrikeFlash'); st['step'] = 21
    elif st['step'] == 21 and e > st['at'] + .6:
        photo(w, ptz, 'StrikeFireball'); st['step'] = 22
    elif st['step'] == 22 and e > st['at'] + 1.5:
        photo(w, ptz, 'StrikeDust'); st['step'] = 3
    elif st['step'] == 3 and e > st['at'] + 4:
        photo(w, ptz, 'StrikeSmoke')
        r['fires_after_strike'] = war.active_fire_count()
        pan, tilt, _ = st['pan']
        ptz.set_ptz(pan + 2.5, tilt, 14)
        r['burn'] = ptz.execute_engagement_command('burn')
        ptz.set_ptz(pan + 1, tilt, 14)
        st['step'] = 4; st['at'] = e
    elif st['step'] == 4 and e > st['at'] + 6:
        photo(w, ptz, 'Fire')
        r['fires'] = war.active_fire_count(); r['fluid_fires'] = war.active_fluid_fire_count()
        st['step'] = 41; st['at'] = e
        unreal.SystemLibrary.execute_console_command(w, 'sim.fx.MaxFluidFires 0')
    elif st['step'] == 41 and e > st['at'] + 1.5:
        photo(w, ptz, 'FireSprites')
        r['fluid_fires_budget_zero'] = war.active_fluid_fire_count()
        unreal.SystemLibrary.execute_console_command(w, 'sim.fx.MaxFluidFires 2')
        pan, tilt, _ = st['pan']
        ptz.set_ptz(pan - 3, tilt, 14)
        r['smoke_screen'] = ptz.execute_engagement_command('smoke_screen')
        ptz.set_ptz(pan - 2, tilt, 20)
        st['step'] = 5; st['at'] = e
    elif st['step'] == 5 and e > st['at'] + 8:
        photo(w, ptz, 'SmokeScreen')
        # Zoom out: small on screen, the fluid detail should hand back to sprites.
        ptz.set_ptz(st['pan'][0] + 60, 20, 60)
        st['step'] = 6; st['at'] = e
    elif st['step'] >= 6 and e > st['at'] + 2 or st['step'] == 99:
        r['fluid_fires_looking_away'] = war.active_fluid_fire_count() if st['step'] != 99 else None
        unreal.unregister_slate_post_tick_callback(war_handle)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        (saved / 'main-war-effects.json').write_text(json.dumps(r, indent=2))
        print('WAR_EFFECTS_TEST', json.dumps(r))


war_handle = unreal.register_slate_post_tick_callback(tick)
globals()['war_handle'] = war_handle
