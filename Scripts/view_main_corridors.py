import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
routes=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingRoute) if 'LivingWorld.MainCorridor' in [str(t) for t in a.tags]]
for route in routes:
    width=200 if route.vehicles else 85
    for side in (-1,1):
        previous=None
        for i in range(101):
            d=route.path.get_spline_length()*i/100
            p=route.path.get_location_at_distance_along_spline(d,unreal.SplineCoordinateSpace.WORLD)+route.path.get_right_vector_at_distance_along_spline(d,unreal.SplineCoordinateSpace.WORLD)*width*side+unreal.Vector(0,0,50)
            if previous is not None:unreal.SystemLibrary.draw_debug_line(world,previous,p,unreal.LinearColor(1,0.1 if route.vehicles else 1,0,1),120,5)
            previous=p
route=next(r for r in routes if r.vehicles)
center=route.path.get_location_at_distance_along_spline(route.path.get_spline_length()/2,unreal.SplineCoordinateSpace.WORLD)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(center+unreal.Vector(0,0,22000),unreal.Rotator(pitch=-90,yaw=0))
print('CORRIDOR_CENTER',center)
