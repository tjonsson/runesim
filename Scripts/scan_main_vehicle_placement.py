"""Probe full jeep placements on the reviewed road and nearby offsets; restore all scene edits."""
import unreal,json,math
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
route=next(a for a in sub.get_all_level_actors() if isinstance(a,unreal.LivingRoute) and a.vehicles)
profile=unreal.load_asset('/Game/LivingWorld/Profiles/DA_Jeep')
original_width=route.reviewed_half_width_cm
original=[route.path.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD) for i in range(route.path.get_number_of_spline_points())]
rights=[route.path.get_right_vector_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD) for i in range(len(original))]
ignored=[a for a in sub.get_all_level_actors() if not isinstance(a,unreal.Cesium3DTileset)]
probe=sub.spawn_actor_from_class(unreal.LivingAgent,unreal.Vector());ignored.append(probe)
rows=[]
try:
    for offset in (-25,25,75):
        route.reviewed_half_width_cm=original_width-abs(offset)
        points=[]
        for p,right in zip(original,rights):
            p=p+right*offset
            hit=unreal.SystemLibrary.line_trace_single_for_objects(world,p+unreal.Vector(0,0,1000),p-unreal.Vector(0,0,1000),[unreal.ObjectTypeQuery.ECC_WORLD_STATIC],True,ignored,unreal.DrawDebugTrace.NONE)
            assert hit,'Terrain must be loaded'
            f=unreal.GameplayStatics.get_default_object().call_method('BreakHitResult',args=(hit,))
            assert f[0];points.append(f[5])
        route.path.set_spline_points(points,unreal.SplineCoordinateSpace.WORLD)
        for i in range(len(points)):route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
        checks=[]
        for d in range(300,int(route.path.get_spline_length())-300,5):
            ok=probe.preview_route_placement(profile,route,d)
            checks.append({'distance_cm':d,'passed':ok,'reason':probe.blocked_reason if not ok else ''})
        rows.append({'offset_cm':offset,'samples':len(checks),'passed':sum(c['passed'] for c in checks),'failed':[c for c in checks if not c['passed']],'points':[[p.x,p.y,p.z] for p in points]})
finally:
    route.reviewed_half_width_cm=original_width
    route.path.set_spline_points(original,unreal.SplineCoordinateSpace.WORLD)
    for i in range(len(original)):route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
    sub.destroy_actor(probe)
folder=Path(unreal.Paths.project_saved_dir())/'LivingWorld/MainRoutes'
(folder/'full-vehicle-placement.json').write_text(json.dumps(rows,indent=2))
print('FULL_VEHICLE_SCAN',[{k:v for k,v in r.items() if k!='points'} for r in rows])
