"""Verify that vehicles exit an open one-way corridor instead of making a U-turn."""
import unreal
import time
import json
from pathlib import Path

def run_test():
    world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
    assert 'LivingWorldDemo' in world.get_name()
    route=next(r for r in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingRoute) if r.get_editor_property('vehicles'))
    cars=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent) if a.active and a.profile.kind==unreal.LivingKind.CAR]
    assert cars
    points=[route.path.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.LOCAL) for i in range(route.path.get_number_of_spline_points())]
    closed=route.path.is_closed_loop();one_way=route.get_editor_property('one_way')
    route.path.set_spline_points([unreal.Vector(),unreal.Vector(100,0,0)],unreal.SplineCoordinateSpace.LOCAL)
    for i in range(2):route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
    route.path.set_closed_loop(False);route.set_editor_property('one_way',True)
    started=time.monotonic()
    def tick(dt):
        elapsed=time.monotonic()-started
        if all(not car.active for car in cars) or elapsed>3:
            result={'passed':all(not car.active for car in cars),'vehicles':len(cars),'elapsed_seconds':elapsed}
            route.path.set_spline_points(points,unreal.SplineCoordinateSpace.LOCAL)
            for i in range(len(points)):route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
            route.path.set_closed_loop(closed);route.set_editor_property('one_way',one_way)
            unreal.unregister_slate_post_tick_callback(handle)
            (Path(unreal.Paths.project_saved_dir())/'LivingWorld/one-way-route.json').write_text(json.dumps(result,indent=2))
            print('ONE_WAY_RESULT',result)
    handle=unreal.register_slate_post_tick_callback(tick)

run_test()
