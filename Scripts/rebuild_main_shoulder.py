"""Rebuild MainLevel's pedestrian shoulder as a fixed offset from the validated (smoothed) road.

Editor, MainLevel, PIE stopped. RUNESIM_SHOULDER_STAGE=hold adds refinement cameras along the road;
=scan builds, probes and saves. Offset = road half-width + 75 cm gap + shoulder half-width, on the side
of the previous shoulder. Trail ends are moved onto the new shoulder and every trail is re-probed.
The previous shoulder stays in the map, unvalidated. Evidence: MainRoutes/shoulder-rebuild.json.
"""
import json
import math
import os
import unreal
from datetime import datetime, timezone
from pathlib import Path

STAGE = os.environ.get('RUNESIM_SHOULDER_STAGE', 'hold')
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
routes = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.LivingRoute)]
road = next(r for r in routes if r.vehicles and r.validated)
old = next(r for r in routes if not r.vehicles and r.validated and 'shoulder' in r.get_actor_label())
trails = [r for r in routes if r.get_actor_label().startswith('Living Main Trail') and r.validated]
W = unreal.SplineCoordinateSpace.WORLD
length = road.path.get_spline_length()

if STAGE == 'hold':
    manager = unreal.CesiumCameraManager.get_default_camera_manager(world)
    for c in globals().get('shoulder_cameras', []):
        manager.remove_camera(c)
    globals()['shoulder_cameras'] = []
    for d in range(0, int(length) + 1, 6000):
        camera = unreal.CesiumCamera(); camera.viewport_size = unreal.Vector2D(1280, 1280)
        camera.location = road.path.get_location_at_distance_along_spline(d, W) + unreal.Vector(0, 0, 4000)
        camera.rotation = unreal.Rotator(pitch=-90, yaw=0); camera.field_of_view_degrees = 90
        shoulder_cameras.append(manager.add_camera(camera))
    print('SHOULDER_HOLD', len(shoulder_cameras))
    raise SystemExit

ignored = [a for a in actors.get_all_level_actors() if not isinstance(a, unreal.Cesium3DTileset)]


def ground(p):
    hit = unreal.SystemLibrary.line_trace_single_for_objects(world, p + unreal.Vector(0, 0, 2000), p - unreal.Vector(0, 0, 2000),
        [unreal.ObjectTypeQuery.ECC_WORLD_STATIC], True, ignored, unreal.DrawDebugTrace.NONE)
    if not hit:
        return None
    f = unreal.GameplayStatics.get_default_object().call_method('BreakHitResult', args=(hit,))
    return f[5] if f[0] else None


def probe(route, profile, agent, margin):
    rows, d = [], margin
    while d <= route.path.get_spline_length() - margin:
        rows.append((d, agent.preview_route_placement(profile, route, d)))
        d += 25
    return rows


mid = old.path.get_location_at_distance_along_spline(old.path.get_spline_length() * .5, W)
key_d = road.path.get_distance_along_spline_at_spline_input_key(road.path.find_input_key_closest_to_world_location(mid))
side = 1.0 if (mid - road.path.get_location_at_distance_along_spline(key_d, W)).dot(road.path.get_right_vector_at_distance_along_spline(key_d, W)) > 0 else -1.0
offset = road.reviewed_half_width_cm + 75.0 + old.reviewed_half_width_cm
points = []
for d in range(0, int(length), 250):
    right = road.path.get_right_vector_at_distance_along_spline(d, W); right.z = 0; right = right.normal()
    g = ground(road.path.get_location_at_distance_along_spline(d, W) + right * side * offset)
    if g:
        points.append(g)
label = 'Living Main Pedestrian shoulder (offset from scanned road)'
for r in [r for r in routes if r.get_actor_label() == label]:
    actors.destroy_actor(r)
shoulder = actors.spawn_actor_from_class(unreal.LivingRoute, points[0])
shoulder.set_actor_label(label)
shoulder.path.set_spline_points(points, W)
for k in range(len(points)):
    shoulder.path.set_spline_point_type(k, unreal.SplinePointType.LINEAR)
shoulder.vehicles = False; shoulder.retire_at_ends = True; shoulder.validated = True
shoulder.reviewed_half_width_cm = old.reviewed_half_width_cm; shoulder.max_slope_degrees = old.max_slope_degrees
shoulder.surface = unreal.LivingSurface.DIRT
civilian = unreal.load_asset('/Game/LivingWorld/Profiles/DA_Civilian')
agent = actors.spawn_actor_from_class(unreal.LivingAgent, unreal.Vector())
margin = civilian.collision_radius_cm + 100
report = {'scanned_utc': datetime.now(timezone.utc).isoformat(), 'offset_cm': offset, 'side': side}
try:
    rows = probe(shoulder, civilian, agent, margin)
    # Keep the longest continuously passing span.
    best, start = (0, -1), None
    for i, (_, ok) in enumerate(rows):
        if ok:
            start = i if start is None else start
            if i - start > best[1] - best[0]:
                best = (start, i)
        else:
            start = None
    lo, hi = rows[best[0]][0] - margin + 25, rows[best[1]][0] + margin - 25
    keep = [shoulder.path.get_location_at_distance_along_spline(d, W) for d in range(int(lo), int(hi), 250)] + [shoulder.path.get_location_at_distance_along_spline(hi, W)]
    shoulder.path.set_spline_points(keep, W)
    for k in range(len(keep)):
        shoulder.path.set_spline_point_type(k, unreal.SplinePointType.LINEAR)
    confirm = probe(shoulder, civilian, agent, margin)
    spacing = []
    for d in range(0, int(shoulder.path.get_spline_length()), 250):
        p = shoulder.path.get_location_at_distance_along_spline(d, W)
        q = road.path.find_location_closest_to_world_location(p, W)
        spacing.append(math.hypot(p.x - q.x, p.y - q.y))
    shoulder.validated = all(ok for _, ok in confirm) and min(spacing) > offset - 30
    report['shoulder'] = {'length_m': round(shoulder.path.get_spline_length() / 100, 1), 'probes': len(confirm),
                          'passed': sum(ok for _, ok in confirm), 'min_road_spacing_cm': round(min(spacing)), 'validated': shoulder.validated}
    if shoulder.validated:
        shoulder.provenance = (f'Offset {offset:.0f} cm from the validated scanned road centreline, projected to MainLevel Cesium collision; '
                               f"civilian placement probed every 25 cm, {report['shoulder']['passed']}/{report['shoulder']['probes']} passed; "
                               + report['scanned_utc'][:10])
        old.validated = False
        old.set_actor_label(old.get_actor_label() + ' (superseded)')
        report['trails'] = []
        for trail in trails:
            count = trail.path.get_number_of_spline_points()
            pts = [trail.path.get_location_at_spline_point(k, W) for k in range(count)]
            for k in (0, count - 1):
                pts[k] = shoulder.path.find_location_closest_to_world_location(pts[k], W)
            trail.path.set_spline_points(pts, W)
            for k in range(count):
                trail.path.set_spline_point_type(k, unreal.SplinePointType.LINEAR)
            result = probe(trail, civilian, agent, margin)
            trail.validated = all(ok for _, ok in result)
            report['trails'].append({'label': trail.get_actor_label(), 'probes': len(result), 'passed': sum(ok for _, ok in result),
                                     'validated': trail.validated})
    else:
        actors.destroy_actor(shoulder)
finally:
    actors.destroy_actor(agent)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(Path(unreal.Paths.project_saved_dir()) / 'LivingWorld/MainRoutes/shoulder-rebuild.json').write_text(json.dumps(report, indent=2))
print('SHOULDER_REBUILT', json.dumps(report))
