"""PIE gallery of the battlefield effects on MainLevel, photographed from a drone-like viewpoint.

Places a strike, a sprite-only fire, a fluid fire and a smoke screen on open ground near the road and
photographs each at a few moments with a borrowed scene capture (Saved/LivingWorld/Gallery-*.png).
Living World traffic and people are switched off so nothing blocks the view.
"""
import json, math, time, unreal
from pathlib import Path
prev = globals().pop('gallery_handle', None)
if prev is not None: unreal.unregister_slate_post_tick_callback(prev)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
saved = Path(unreal.Paths.project_saved_dir()) / 'LivingWorld'
st = {'t': time.monotonic(), 'events': [], 'done': set(), 'r': {}}


def ground(world, p):
    hit = unreal.SystemLibrary.line_trace_single(world, p + unreal.Vector(0, 0, 20000), p - unreal.Vector(0, 0, 20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4] if hit and hit.to_tuple()[0] else p


def photo(world, capture, target, name, distance=8000, elevation=35, azimuth=40, fov=45):
    if 'ldr' not in st:
        st['ldr'] = unreal.RenderingLibrary.create_render_target2d(world, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    a, e = math.radians(azimuth), math.radians(elevation)
    eye = target + unreal.Vector(-math.cos(e) * math.cos(a), -math.cos(e) * math.sin(a), math.sin(e)) * distance
    look = target + unreal.Vector(0, 0, 600)
    pose = (capture.get_world_location(), capture.get_world_rotation())
    old = (capture.texture_target, capture.capture_source, capture.fov_angle)
    capture.set_world_location_and_rotation(eye, unreal.MathLibrary.find_look_at_rotation(eye, look), False, False)
    capture.texture_target, capture.capture_source, capture.fov_angle = st['ldr'], unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR, fov
    capture.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, st['ldr'], str(saved), f'Gallery-{name}.png')
    capture.texture_target, capture.capture_source, capture.fov_angle = old
    capture.set_world_location_and_rotation(pose[0], pose[1], False, False)


def tick(dt):
    ws = unreal.EditorLevelLibrary.get_pie_worlds(False)
    if not ws: return
    w = ws[0]; e = time.monotonic() - st['t']
    ptz = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.SimPTZ)[0]
    capture = ptz.get_editor_property('capture')
    war = next(o for o in unreal.ObjectIterator(unreal.SimWarEffects) if o.get_outer() == w)
    if 'setup' not in st and e > 8:
        system = next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer() == w)
        o = unreal.LivingWorldOptions(); o.enabled = True; o.preset = unreal.LivingPreset.CUSTOM
        o.traffic_density = 0; o.crowd_density = 0; o.planes = 0; o.helicopters = 0; o.drones = 0; o.bird_flocks = 0
        system.apply_options(o)
        road = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.LivingRoute) if 'Dirt road' in a.get_actor_label())
        spline = road.get_editor_property('path')
        base = spline.get_location_at_distance_along_spline(15000, unreal.SplineCoordinateSpace.WORLD)
        right = spline.get_right_vector_at_distance_along_spline(15000, unreal.SplineCoordinateSpace.WORLD)
        fwd = spline.get_direction_at_distance_along_spline(15000, unreal.SplineCoordinateSpace.WORLD)
        st['spots'] = {k: ground(w, base + right * 2500 + fwd * d) for k, d in (('strike', 0), ('sprites', 6000), ('fluid', -6000), ('screen', 12000))}
        # Sprite-only and fluid fires side by side: the capture is the only viewer that matters here.
        st['setup'] = e
    if 'setup' not in st: return
    t = e - st['setup']; s = st['spots']; r = st['r']
    def once(key, when, fn):
        if key not in st['done'] and t > when:
            st['done'].add(key); fn()
    once('strike', 2, lambda: war.strike(s['strike'], 2., True))
    once('p_flash', 2.2, lambda: photo(w, capture, s['strike'], 'StrikeFlash'))
    once('p_ball', 2.7, lambda: photo(w, capture, s['strike'], 'StrikeFireball'))
    once('p_dust', 4.0, lambda: photo(w, capture, s['strike'], 'StrikeDust'))
    once('p_smoke', 9.0, lambda: photo(w, capture, s['strike'], 'StrikeSmoke', distance=12000, elevation=20))
    def sprites():
        unreal.SystemLibrary.execute_console_command(w, 'sim.fx.MaxFluidFires 0')
        st['sprite_fire'] = war.start_fire(s['sprites'], 1.5, 60.)
    once('fire_sprites', 10, sprites)
    once('p_sprites', 16, lambda: photo(w, capture, s['sprites'], 'FireSprites', distance=6000))
    def fluid():
        war.stop_fire(st['sprite_fire'])
        unreal.SystemLibrary.execute_console_command(w, 'sim.fx.MaxFluidFires 2')
        st['fluid_fire'] = war.start_fire(s['fluid'], 1.5, 60.)
    once('fire_fluid', 17, fluid)
    # The tripod capture is a registered viewer: park it at the photo viewpoint so the fire earns fluid detail.
    def park():
        a, el, dist = math.radians(40), math.radians(35), 6000
        eye = s['fluid'] + unreal.Vector(-math.cos(el) * math.cos(a), -math.cos(el) * math.sin(a), math.sin(el)) * dist
        capture.set_world_location_and_rotation(eye, unreal.MathLibrary.find_look_at_rotation(eye, s['fluid'] + unreal.Vector(0, 0, 600)), False, False)
        capture.fov_angle = 45
    once('aim_fluid', 17.2, park)
    once('p_fluid', 25, lambda: (r.__setitem__('fluid_active', war.active_fluid_fire_count()), photo(w, capture, s['fluid'], 'FireFluid', distance=6000)))
    once('screen', 26, lambda: war.smoke_screen(s['screen'], 1.5, 45.))
    once('p_screen', 36, lambda: photo(w, capture, s['screen'], 'SmokeScreen', distance=14000, elevation=20))
    if t > 37:
        unreal.unregister_slate_post_tick_callback(gallery_handle)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        (saved / 'war-gallery.json').write_text(json.dumps(r, indent=2))
        print('GALLERY', json.dumps(r))


gallery_handle = unreal.register_slate_post_tick_callback(tick)
globals()['gallery_handle'] = gallery_handle
