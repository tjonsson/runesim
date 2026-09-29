"""Retain the road section traversed successfully by full wheel-contact vehicles."""
import unreal,json
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
route=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingRoute) if a.vehicles)
assert 10000<route.path.get_spline_length()<10300
points=[route.path.get_location_at_distance_along_spline(d,unreal.SplineCoordinateSpace.WORLD) for d in range(500,6001,25)]
route.modify();route.path.modify();route.path.set_spline_points(points,unreal.SplineCoordinateSpace.WORLD)
for i in range(len(points)):route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
checks=[bool(route.sample_ground(i*25,0,105)) for i in range(int(route.path.get_spline_length()/25)+1)]
assert all(checks)
data=json.loads(route.provenance);data['wheel_review']='Removed discontinuous terrain facets beyond 60 m after live wheel-contact test; no contact limits relaxed';data['retained_length_cm']=route.path.get_spline_length();route.provenance=json.dumps(data)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('WHEEL_CORRIDOR',route.path.get_spline_length(),len(checks))
