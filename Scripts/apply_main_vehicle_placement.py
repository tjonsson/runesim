"""Apply a fully supported candidate inside the originally reviewed dirt-road strip."""
import unreal,json
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
route=next(a for a in sub.get_all_level_actors() if isinstance(a,unreal.LivingRoute) and a.vehicles)
folder=Path(unreal.Paths.project_saved_dir())/'LivingWorld/MainRoutes'
candidate=next(r for r in json.loads((folder/'full-vehicle-placement.json').read_text()) if r['offset_cm']==75)
assert candidate['passed']==candidate['samples'] and not candidate['failed']
route.path.set_spline_points([unreal.Vector(*p) for p in candidate['points']],unreal.SplineCoordinateSpace.WORLD)
for i in range(len(candidate['points'])):route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
# The centimetre scan found isolated unsupported facets outside this continuous section.
# Keep the supported section, with additional room for whole-vehicle entry/exit.
points=[route.path.get_location_at_distance_along_spline(d,unreal.SplineCoordinateSpace.WORLD) for d in range(500,3701,25)]
route.path.set_spline_points(points,unreal.SplineCoordinateSpace.WORLD)
for i in range(len(points)):route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
route.reviewed_half_width_cm=125
profile=unreal.load_asset('/Game/LivingWorld/Profiles/DA_Jeep')
probe=sub.spawn_actor_from_class(unreal.LivingAgent,unreal.Vector())
failed=[];count=0
try:
    for d in range(300,int(route.path.get_spline_length())-300):
        count+=1
        if not probe.preview_route_placement(profile,route,d):failed.append({'distance_cm':d,'reason':probe.blocked_reason})
finally:sub.destroy_actor(probe)
report={'length_cm':route.path.get_spline_length(),'offset_cm':75,'reviewed_half_width_cm':125,'sample_spacing_cm':1,'samples':count,'failed':failed,'points':[[p.x,p.y,p.z] for p in points]}
(folder/'applied-vehicle-placement.json').write_text(json.dumps(report,indent=2))
assert not failed,failed
manager=unreal.CesiumCameraManager.get_default_camera_manager(world)
for camera_id in globals().pop('main_authoring_cameras',[]):manager.remove_camera(camera_id)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('APPLIED_MAIN_ROAD',{k:v for k,v in report.items() if k!='points'})
