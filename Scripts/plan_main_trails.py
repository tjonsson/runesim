"""Plan and validate cross-country walking trails in MainLevel from a terrain-grid scan.

Editor, MainLevel open, PIE stopped. Two stages (set in the editor's Python environment):
  RUNESIM_TRAIL_STAGE=hold  add Cesium refinement cameras over the scan area; wait ~60 s
  RUNESIM_TRAIL_STAGE=scan  grid-scan, plan, validate, save
Walkable cells: surface normal within 16 degrees of up and no step above 70 cm to any grid neighbour
(3 m spacing), which excludes tree canopies, walls and gullies in the photogrammetry. Trails are loops
that leave the reviewed shoulder, cross the fields and rejoin it (one crosses the road), so agents
continue through route links. Each trail must pass the runtime civilian placement probe every 25 cm.
Evidence: Saved/LivingWorld/MainRoutes/trails.json. Existing routes are untouched.
"""
import heapq
import json
import math
import os
import random
import unreal
from datetime import datetime, timezone
from pathlib import Path

STAGE = os.environ.get('RUNESIM_TRAIL_STAGE', 'hold')
STEP = 300.0
HALF_EXTENT = 15000.0
assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'MainLevel'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
routes = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.LivingRoute)]
road = next(r for r in routes if r.vehicles and r.validated)
shoulder = next(r for r in routes if not r.vehicles and r.validated and 'Trail' not in r.get_actor_label())
centre = road.path.get_location_at_distance_along_spline(road.path.get_spline_length() * .5, unreal.SplineCoordinateSpace.WORLD)

if STAGE == 'hold':
    manager = unreal.CesiumCameraManager.get_default_camera_manager(world)
    for old in globals().get('trail_cameras', []):
        manager.remove_camera(old)
    globals()['trail_cameras'] = []
    for dx in (-10000, 0, 10000):
        for dy in (-10000, 0, 10000):
            camera = unreal.CesiumCamera()
            camera.viewport_size = unreal.Vector2D(1280, 1280)
            camera.location = centre + unreal.Vector(dx, dy, 8000)
            camera.rotation = unreal.Rotator(pitch=-90, yaw=0)
            camera.field_of_view_degrees = 90
            trail_cameras.append(manager.add_camera(camera))
    print('TRAIL_HOLD', len(trail_cameras))
    raise SystemExit

ignored = [a for a in actors.get_all_level_actors() if not isinstance(a, unreal.Cesium3DTileset)]


def surface(x, y):
    p = unreal.Vector(x, y, centre.z)
    hit = unreal.SystemLibrary.line_trace_single_for_objects(world, p + unreal.Vector(0, 0, 20000), p - unreal.Vector(0, 0, 20000),
        [unreal.ObjectTypeQuery.ECC_WORLD_STATIC], True, ignored, unreal.DrawDebugTrace.NONE)
    if not hit:
        return None
    f = unreal.GameplayStatics.get_default_object().call_method('BreakHitResult', args=(hit,))
    return (f[5], f[7]) if f[0] else None  # impact point, impact normal


n = int(2 * HALF_EXTENT / STEP) + 1
grid = {}
for i in range(n):
    for j in range(n):
        s = surface(centre.x - HALF_EXTENT + i * STEP, centre.y - HALF_EXTENT + j * STEP)
        if s:
            grid[(i, j)] = s
cos_limit = math.cos(math.radians(16))
walk = set()
for (i, j), (p, normal) in grid.items():
    if normal.z < cos_limit:
        continue
    if all((i + di, j + dj) in grid and abs(grid[(i + di, j + dj)][0].z - p.z) <= 70
           for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1))):
        walk.add((i, j))
# Largest connected region.
seen, best = set(), set()
for cell in walk:
    if cell in seen:
        continue
    stack, region = [cell], set()
    seen.add(cell)
    while stack:
        c = stack.pop(); region.add(c)
        for d in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nxt = (c[0] + d[0], c[1] + d[1])
            if nxt in walk and nxt not in seen:
                seen.add(nxt); stack.append(nxt)
    if len(region) > len(best):
        best = region
walk = best


def cell_of(p):
    return (round((p.x - centre.x + HALF_EXTENT) / STEP), round((p.y - centre.y + HALF_EXTENT) / STEP))


def road_distance(p):
    q = road.path.find_location_closest_to_world_location(p, unreal.SplineCoordinateSpace.WORLD)
    return math.hypot(p.x - q.x, p.y - q.y)


road_cost = {c: (8.0 if road_distance(grid[c][0]) < 350 else 1.0) for c in walk}
# Prefer interior cells so trails keep clear of canopy and slope edges.
edge_cost = {c: 1.0 + sum((c[0] + di, c[1] + dj) not in walk for di in (-1, 0, 1) for dj in (-1, 0, 1)) * .8 for c in walk}


def astar(a, b):
    frontier, came, cost = [(0, a)], {a: None}, {a: 0}
    while frontier:
        _, c = heapq.heappop(frontier)
        if c == b:
            break
        for di in (-1, 0, 1):
            for dj in (-1, 0, 1):
                nxt = (c[0] + di, c[1] + dj)
                if (di or dj) and nxt in walk:
                    rise = abs(grid[nxt][0].z - grid[c][0].z) / 100.0
                    step = math.hypot(di, dj) * road_cost[nxt] * edge_cost[nxt] + rise * 2
                    total = cost[c] + step
                    if total < cost.get(nxt, 1e18):
                        cost[nxt], came[nxt] = total, c
                        heapq.heappush(frontier, (total + math.hypot(nxt[0] - b[0], nxt[1] - b[1]), nxt))
    if b not in came:
        return None
    path, c = [], b
    while c:
        path.append(c); c = came[c]
    return path[::-1]


def chaikin(points, rounds=2):
    for _ in range(rounds):
        out = [points[0]]
        for a, b in zip(points, points[1:]):
            out += [a * .75 + b * .25, a * .25 + b * .75]
        points = out + [points[-1]]
    return points


rng = random.Random(4242)
length = shoulder.path.get_spline_length()
civilian = unreal.load_asset('/Game/LivingWorld/Profiles/DA_Civilian')
probe_agent = actors.spawn_actor_from_class(unreal.LivingAgent, unreal.Vector())
report = {'scanned_utc': datetime.now(timezone.utc).isoformat(), 'grid_step_cm': STEP, 'cells': len(grid), 'walkable_region_cells': len(walk),
          'area_m2': round(len(walk) * (STEP / 100) ** 2), 'trails': []}
plans = [('Living Main Trail north loop', .82, .97, False), ('Living Main Trail south loop', .5, .8, False), ('Living Main Trail road crossing', .3, .65, True)]
only = os.environ.get('RUNESIM_TRAIL_ONLY')
if only:
    plans = [plan for plan in plans if plan[0] == only]
for old in [r for r in routes if r.get_actor_label() in [plan[0] for plan in plans]]:
    actors.destroy_actor(old)
attempts = int(os.environ.get('RUNESIM_TRAIL_ATTEMPTS', '12'))
try:
    for label, a_frac, b_frac, cross in plans:
        accepted = None
        reasons = []
        for attempt in range(attempts):
            a = shoulder.path.get_location_at_distance_along_spline(length * (a_frac + rng.uniform(-.05, .05)), unreal.SplineCoordinateSpace.WORLD)
            b = shoulder.path.get_location_at_distance_along_spline(length * (b_frac + rng.uniform(-.05, .05)), unreal.SplineCoordinateSpace.WORLD)
            # Waypoint on the far side of the shoulder from the road, or across the road for the crossing trail.
            q = road.path.find_location_closest_to_world_location((a + b) * .5, unreal.SplineCoordinateSpace.WORLD)
            away = (a + b) * .5 - q; away.z = 0; away = away.normal()
            reach = rng.uniform(4000, 9000)
            mid = (a + b) * .5 + away * (-reach if cross else reach) + unreal.Vector(rng.uniform(-2000, 2000), rng.uniform(-2000, 2000), 0)
            ca, cb, cm = cell_of(a), cell_of(b), cell_of(mid)
            if cm not in walk:
                candidates = sorted(walk, key=lambda c: (c[0] - cm[0]) ** 2 + (c[1] - cm[1]) ** 2)[:1]
                cm = candidates[0] if candidates else None
            if ca not in walk or cb not in walk or cm is None:
                reasons.append('endpoint not walkable'); continue
            out, back = astar(ca, cm), astar(cm, cb)
            if not out or not back:
                reasons.append('no path'); continue
            cells = out + back[1:]
            points = [a] + [grid[c][0] for c in cells[1:-1]] + [b]
            points = chaikin(points)
            route = actors.spawn_actor_from_class(unreal.LivingRoute, a)
            route.set_actor_label(label)
            route.path.set_spline_points(points, unreal.SplineCoordinateSpace.WORLD)
            dense = [route.path.get_location_at_distance_along_spline(d, unreal.SplineCoordinateSpace.WORLD) for d in range(0, int(route.path.get_spline_length()), 250)]
            projected = [surface(p.x, p.y) for p in dense]
            if not all(projected):
                actors.destroy_actor(route); reasons.append('missing terrain'); continue
            ground = [s[0] for s in projected[1:-1]]
            route.path.set_spline_points([a] + ground + [b], unreal.SplineCoordinateSpace.WORLD)
            for k in range(route.path.get_number_of_spline_points()):
                route.path.set_spline_point_type(k, unreal.SplinePointType.LINEAR)
            route.vehicles = False; route.retire_at_ends = True; route.validated = True
            route.reviewed_half_width_cm = 100.0; route.max_slope_degrees = shoulder.max_slope_degrees
            route.surface = unreal.LivingSurface.GRASS
            margin = civilian.collision_radius_cm + 100
            probes = 0; failed = []
            d = margin
            while d <= route.path.get_spline_length() - margin:
                probes += 1
                if not probe_agent.preview_route_placement(civilian, route, d):
                    failed.append((round(d), probe_agent.blocked_reason))
                    if len(failed) > 3:
                        break
                d += 25
            if failed:
                actors.destroy_actor(route)
                reasons.append('probe: ' + failed[0][1])
                continue
            route.provenance = (f'Terrain-grid plan ({STEP / 100:.0f} m cells, slope <=16 deg, step <=70 cm) over MainLevel Cesium collision; '
                                f'civilian placement probed every 25 cm, {probes}/{probes} passed; linked to the reviewed shoulder at both ends; '
                                + report['scanned_utc'][:10])
            accepted = {'label': label, 'length_m': round(route.path.get_spline_length() / 100, 1), 'probes': probes,
                        'attempt': attempt + 1, 'crosses_road': cross}
            break
        report['trails'].append(accepted or {'label': label, 'accepted': False, 'reasons': reasons})
finally:
    actors.destroy_actor(probe_agent)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
out = Path(unreal.Paths.project_saved_dir()) / 'LivingWorld/MainRoutes/trails.json'
out.write_text(json.dumps(report, indent=2))
print('TRAILS_PLANNED', json.dumps(report))
