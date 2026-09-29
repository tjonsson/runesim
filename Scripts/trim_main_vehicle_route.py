"""Keep the longest fully supported portion; never relax collision limits."""
import unreal,json
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
route=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingRoute) if a.vehicles and 'LivingWorld.MainCorridor' in [str(t) for t in a.tags])
route.validated=True;route.reviewed_half_width_cm=200
valid=[]
for i in range(int(route.path.get_spline_length()/25)+1):valid.append(bool(route.sample_ground(i*25,0,105)))
runs=[];start=None
for i,passed in enumerate(valid+[False]):
    if passed and start is None:start=i
    if not passed and start is not None:runs.append((start,i-1));start=None
first,last=max(runs,key=lambda r:r[1]-r[0]);first+=10;last-=10
assert (last-first)*25>=6000,'No useful supported road section'
points=[route.path.get_location_at_distance_along_spline(i*25,unreal.SplineCoordinateSpace.WORLD) for i in range(first,last+1)]
route.modify();route.path.modify();route.path.set_spline_points(points,unreal.SplineCoordinateSpace.WORLD)
for i in range(len(points)):route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
checks=[bool(route.sample_ground(i*25,0,105)) for i in range(int(route.path.get_spline_length()/25)+1)]
route.validated=all(checks)
report={'previous_samples':len(valid),'previous_failed_cm':[i*25 for i,p in enumerate(valid) if not p],'retained_from_cm':first*25,'retained_to_cm':last*25,'length_cm':route.path.get_spline_length(),'samples':len(checks),'passed':sum(checks),'validated':route.validated}
provenance=json.loads(route.provenance);provenance['trim_review']=report;route.provenance=json.dumps(provenance)
(Path(unreal.Paths.project_saved_dir())/'LivingWorld/MainRoutes/vehicle-trim.json').write_text(json.dumps(report,indent=2))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('VEHICLE_TRIM',report)
