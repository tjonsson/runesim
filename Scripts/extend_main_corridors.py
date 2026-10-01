"""Extend MainLevel's reviewed ground network from terrain scans (editor, MainLevel, PIE stopped).

Stage 1 (RUNESIM_CORRIDOR_STAGE=hold, default): add Cesium authoring cameras over every route and
the full authored road so tiles refine; wait ~60 s before stage 2.
Stage 2 (RUNESIM_CORRIDOR_STAGE=scan): rebuild the full authored dirt road on the loaded collision
surface, probe the reviewed jeep at 25 cm spacing with the same runtime placement checks
(footprint, chassis fit, wheel contacts; one wheel may bridge an isolated facet), keep the longest
continuously passing span and validate it only if every probe passes after trimming. Walking-track
candidates are probed with the civilian profile the same way. Old corridors stay in the map,
unvalidated, for rollback. Evidence: Saved/LivingWorld/MainRoutes/extended-corridors.json.
"""
import json
import os
import unreal
from datetime import datetime, timezone
from pathlib import Path

STAGE = os.environ.get('RUNESIM_CORRIDOR_STAGE', 'hold')
assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'MainLevel', world.get_name()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
folder = Path(unreal.Paths.project_saved_dir()) / 'LivingWorld/MainRoutes'
authored = json.loads((folder / 'authored-road.json').read_text())
control = [unreal.Vector(*p) for p in authored['control']]
routes = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.LivingRoute)]
print('ROUTES', [(r.get_actor_label(), r.validated, r.vehicles, round(r.path.get_spline_length())) for r in routes])

if STAGE == 'hold':
    manager = unreal.CesiumCameraManager.get_default_camera_manager(world)
    for old in globals().get('corridor_cameras', []):
        manager.remove_camera(old)
    globals()['corridor_cameras'] = []
    # Overlapping close views along the whole authored road plus each existing route.
    views = [control[i] for i in range(len(control))] + [r.get_actor_bounds(False)[0] for r in routes]
    for centre in views:
        camera = unreal.CesiumCamera()
        camera.viewport_size = unreal.Vector2D(1280, 1280)
        camera.location = centre + unreal.Vector(0, 0, 4000)
        camera.rotation = unreal.Rotator(pitch=-90, yaw=0)
        camera.field_of_view_degrees = 90
        corridor_cameras.append(manager.add_camera(camera))
    print('CORRIDOR_HOLD', len(corridor_cameras))
    raise SystemExit

ignored = [a for a in actors.get_all_level_actors() if not isinstance(a, unreal.Cesium3DTileset)]


def ground(p):
    hit = unreal.SystemLibrary.line_trace_single_for_objects(world, p + unreal.Vector(0, 0, 2000), p - unreal.Vector(0, 0, 2000),
        [unreal.ObjectTypeQuery.ECC_WORLD_STATIC], True, ignored, unreal.DrawDebugTrace.NONE)
    if not hit:
        return None
    fields = unreal.GameplayStatics.get_default_object().call_method('BreakHitResult', args=(hit,))
    return fields[5] if fields[0] else None


def densify(points, step=500.):
    out = []
    for a, b in zip(points, points[1:]):
        d = (b - a).length()
        n = max(1, int(d // step))
        out += [a + (b - a) * (i / n) for i in range(n)]
    return out + [points[-1]]


def probe(route, profile, agent, margin, step=25):
    length = route.path.get_spline_length()
    rows = []
    d = margin
    while d <= length - margin:
        ok = agent.preview_route_placement(profile, route, d)
        rows.append((d, ok, '' if ok else agent.blocked_reason))
        d += step
    return rows


def longest(rows):
    best, run = (0, -1), None
    for i, (_, ok, _) in enumerate(rows):
        if ok:
            run = i if run is None else run
            if i - run > best[1] - best[0]:
                best = (run, i)
        else:
            run = None
    return best


def set_points(route, points):
    route.path.set_spline_points(points, unreal.SplineCoordinateSpace.WORLD)
    for i in range(len(points)):
        route.path.set_spline_point_type(i, unreal.SplinePointType.LINEAR)


report = {'scanned_utc': datetime.now(timezone.utc).isoformat(), 'method': 'runtime placement probes on loaded Cesium collision, 25 cm spacing',
          'routes': []}
jeep = unreal.load_asset('/Game/LivingWorld/Profiles/DA_Jeep')
civilian = unreal.load_asset('/Game/LivingWorld/Profiles/DA_Civilian')
agent = actors.spawn_actor_from_class(unreal.LivingAgent, unreal.Vector())
ignored.append(agent)
existing_road = next(r for r in routes if r.vehicles and r.validated)
try:
    # 1. Full-length dirt road for vehicles.
    label = 'Living Main Dirt road (scanned full length)'
    for old in [r for r in routes if r.get_actor_label() == label]:
        actors.destroy_actor(old)
    anchors = [ground(p) for p in control]
    assert all(anchors), 'Terrain must be loaded along the whole road (run the hold stage first)'
    road = actors.spawn_actor_from_class(unreal.LivingRoute, anchors[0])
    road.set_actor_label(label)
    # Smooth centreline through the authored control points (no polyline corners for the
    # wheelbase to cut), then re-project dense samples onto the collision surface.
    road.path.set_spline_points(anchors, unreal.SplineCoordinateSpace.WORLD)
    for i in range(len(anchors)):
        road.path.set_spline_point_type(i, unreal.SplinePointType.CURVE)
    smooth = [road.path.get_location_at_distance_along_spline(d, unreal.SplineCoordinateSpace.WORLD)
              for d in range(0, int(road.path.get_spline_length()), 250)]
    projected = [ground(p) for p in smooth]
    assert all(projected), 'Terrain must be loaded along the smoothed road'
    set_points(road, projected)
    road.vehicles = True; road.retire_at_ends = True; road.validated = True
    road.reviewed_half_width_cm = existing_road.reviewed_half_width_cm
    road.max_slope_degrees = existing_road.max_slope_degrees
    road.max_bridged_wheels = 1
    road.surface = unreal.LivingSurface.DIRT
    margin = jeep.collision_radius_cm + 250
    rows = probe(road, jeep, agent, margin)
    start, end = longest(rows)
    span = (rows[end][0] - rows[start][0]) if end >= start else 0
    entry = {'label': label, 'samples': len(rows), 'passed': sum(r[1] for r in rows), 'failed': [(r[0], r[2]) for r in rows if not r[1]][:50],
             'longest_run_cm': [rows[start][0], rows[end][0]] if end >= start else None}
    road.validated = False
    for attempt in range(4):
        if span < 10000:
            break
        # Trim to the passing span, keeping the jeep's entry/exit margin inside it; re-probe the
        # trimmed geometry and repeat until every probe passes.
        lo, hi = rows[start][0] - margin + 25, rows[end][0] + margin - 25
        keep = [road.path.get_location_at_distance_along_spline(d, unreal.SplineCoordinateSpace.WORLD) for d in range(int(lo), int(hi), 250)]
        keep.append(road.path.get_location_at_distance_along_spline(hi, unreal.SplineCoordinateSpace.WORLD))
        set_points(road, keep)
        road.validated = True
        rows = probe(road, jeep, agent, margin)
        entry.update({'trim_attempts': attempt + 1, 'trimmed_length_cm': road.path.get_spline_length(), 'confirm_samples': len(rows),
                      'confirm_passed': sum(r[1] for r in rows)})
        if entry['confirm_passed'] == entry['confirm_samples']:
            break
        road.validated = False
        start, end = longest(rows)
        span = (rows[end][0] - rows[start][0]) if end >= start else 0
    road.provenance = ('Authored dirt-road centreline (authored-road.json) re-projected to MainLevel Cesium collision; '
                       f"jeep placement probed every 25 cm; {entry.get('confirm_passed', 0)}/{entry.get('confirm_samples', 0)} passed after trimming; "
                       'one wheel may bridge an isolated unsupported facet; scanned ' + report['scanned_utc'][:10])
    entry['validated'] = road.validated
    if road.validated:
        existing_road.validated = False
        existing_road.set_actor_label(existing_road.get_actor_label() + ' (superseded)')
        entry['superseded'] = existing_road.get_actor_label()
    report['routes'].append(entry)

    # 2. Pedestrian shoulder along the full road, at the reviewed shoulder's side offset.
    shoulder = next((r for r in routes if not r.vehicles and r.validated and 'scanned' not in r.get_actor_label()), None)
    if shoulder is None:
        raise StopIteration('shoulder already extended')
    dense = densify(control, 100.)
    mid = shoulder.path.get_location_at_distance_along_spline(shoulder.path.get_spline_length() * .5, unreal.SplineCoordinateSpace.WORLD)
    k = min(range(len(dense) - 1), key=lambda i: (dense[i] - mid).length())
    tangent = (dense[k + 1] - dense[k]); tangent.z = 0; tangent = tangent.normal()
    right = unreal.Vector(-tangent.y, tangent.x, 0)
    offset = (mid - dense[k]).dot(right)
    label = 'Living Main Pedestrian shoulder (scanned full length)'
    for old in [r for r in routes if r.get_actor_label() == label]:
        actors.destroy_actor(old)
    offset_points = []
    for i, p in enumerate(dense):
        a, b = dense[max(0, i - 1)], dense[min(len(dense) - 1, i + 1)]
        t = b - a; t.z = 0; t = t.normal()
        offset_points.append(p + unreal.Vector(-t.y, t.x, 0) * offset)
    projected = [ground(p) for p in offset_points[::3]]
    walk = actors.spawn_actor_from_class(unreal.LivingRoute, projected[0] or offset_points[0])
    walk.set_actor_label(label)
    projected = [p for p in projected if p]
    set_points(walk, projected)
    walk.vehicles = False; walk.retire_at_ends = True; walk.validated = True
    walk.reviewed_half_width_cm = shoulder.reviewed_half_width_cm
    walk.max_slope_degrees = shoulder.max_slope_degrees
    walk.surface = unreal.LivingSurface.DIRT
    margin = civilian.collision_radius_cm + 100
    rows = probe(walk, civilian, agent, margin)
    start, end = longest(rows)
    span = (rows[end][0] - rows[start][0]) if end >= start else 0
    entry = {'label': label, 'offset_cm': offset, 'samples': len(rows), 'passed': sum(r[1] for r in rows),
             'failed': [(r[0], r[2]) for r in rows if not r[1]][:50], 'longest_run_cm': [rows[start][0], rows[end][0]] if end >= start else None}
    if span > shoulder.path.get_spline_length() + 2000:
        lo, hi = rows[start][0] - margin + 25, rows[end][0] + margin - 25
        keep = [walk.path.get_location_at_distance_along_spline(d, unreal.SplineCoordinateSpace.WORLD) for d in range(int(lo), int(hi), 250)]
        keep.append(walk.path.get_location_at_distance_along_spline(hi, unreal.SplineCoordinateSpace.WORLD))
        set_points(walk, keep)
        confirm = probe(walk, civilian, agent, margin)
        entry.update({'trimmed_length_cm': walk.path.get_spline_length(), 'confirm_samples': len(confirm), 'confirm_passed': sum(r[1] for r in confirm)})
        walk.validated = entry['confirm_passed'] == entry['confirm_samples']
    else:
        walk.validated = False
    walk.provenance = ('Reviewed shoulder offset applied along the authored dirt road and re-projected to MainLevel Cesium collision; '
                       f"civilian placement probed every 25 cm; {entry.get('confirm_passed', 0)}/{entry.get('confirm_samples', 0)} passed after trimming; scanned " + report['scanned_utc'][:10])
    entry['validated'] = walk.validated
    if walk.validated:
        shoulder.validated = False
        shoulder.set_actor_label(shoulder.get_actor_label() + ' (superseded)')
        entry['superseded'] = shoulder.get_actor_label()
    report['routes'].append(entry)
except StopIteration as done:
    report['routes'].append({'label': 'shoulder', 'skipped': str(done)})
finally:
    actors.destroy_actor(agent)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(folder / 'extended-corridors.json').write_text(json.dumps(report, indent=2))
print('CORRIDORS_EXTENDED', json.dumps([{k: v for k, v in r.items() if k != 'failed'} for r in report['routes']]))
