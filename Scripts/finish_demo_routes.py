"""Enable reviewed lane width and rounded circuits only on the authored flat demo."""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='LivingWorldDemo'
for route in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingRoute):
    assert route.validated and 'Hand-authored' in route.provenance
    route.modify()
    route.set_editor_property('reviewed_half_width_cm',400. if route.vehicles else 200.)
    for i in range(route.path.get_number_of_spline_points()):
        route.path.set_spline_point_type(i,unreal.SplinePointType.CURVE)
    for i in range(100):
        distance=route.path.get_spline_length()*i/100
        for side in (-1,0,1):
            result=route.sample_ground(distance,side*(250. if route.vehicles else 100.),120. if route.vehicles else 35.)
            assert result,(route.get_name(),i,side)
    print('REVIEWED_CORRIDOR',route.get_actor_label(),route.reviewed_half_width_cm)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
