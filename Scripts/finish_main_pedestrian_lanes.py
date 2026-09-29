"""Review two pedestrian lanes on the visible road shoulder using loaded tile support."""
import unreal,json
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
route=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingRoute) if not a.vehicles and 'LivingWorld.MainCorridor' in [str(t) for t in a.tags])
old=route.reviewed_half_width_cm;route.reviewed_half_width_cm=100
checks=[bool(route.sample_ground(i*25,side*50,30)) for i in range(int(route.path.get_spline_length()/25)+1) for side in (-1,0,1)]
if not all(checks):
    route.reviewed_half_width_cm=old
    raise RuntimeError(f'Lane support failed: {sum(checks)}/{len(checks)}')
route.modify()
data=json.loads(route.provenance);data['half_width_cm']=100;data['lane_support_samples']=len(checks);route.provenance=json.dumps(data)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('MAIN_PEDESTRIAN_LANES',len(checks))
