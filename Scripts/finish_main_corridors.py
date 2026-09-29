"""Trim the visually authored road and create its pedestrian shoulder; validate all samples."""
import unreal,json
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
routes=[a for a in sub.get_all_level_actors() if isinstance(a,unreal.LivingRoute) and 'LivingWorld.MainCorridor' in [str(t) for t in a.tags]]
car=next(a for a in routes if a.vehicles);walker=next(a for a in routes if not a.vehicles)
assert 31000<car.path.get_spline_length()<32000,'Requires untrimmed authored road'
ignored=[a for a in sub.get_all_level_actors() if not isinstance(a,unreal.Cesium3DTileset)]
def point(d,offset):
    p=car.path.get_location_at_distance_along_spline(d,unreal.SplineCoordinateSpace.WORLD)+car.path.get_right_vector_at_distance_along_spline(d,unreal.SplineCoordinateSpace.WORLD)*offset
    hit=unreal.SystemLibrary.line_trace_single_for_objects(world,p+unreal.Vector(0,0,1000),p-unreal.Vector(0,0,1000),[unreal.ObjectTypeQuery.ECC_WORLD_STATIC],True,ignored,unreal.DrawDebugTrace.NONE)
    assert hit, 'Terrain tiles must be loaded over the full road before authoring'
    fields=unreal.GameplayStatics.get_default_object().call_method('BreakHitResult',args=(hit,))
    assert fields[0] and isinstance(fields[9],unreal.Cesium3DTileset)
    return fields[5]
geometry=[(car,[point(d,0) for d in range(6000,25001,50)],200),(walker,[point(d,-300) for d in range(9000,21001,50)],65)]
reports=[]
for route,points,width in geometry:
    route.modify();route.path.modify()
    route.path.set_spline_points(points,unreal.SplineCoordinateSpace.WORLD)
    for i in range(len(points)):route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
    route.reviewed_half_width_cm=width;route.validated=True
    route.set_actor_label('Living Main '+('Dirt road' if route.vehicles else 'Pedestrian shoulder'))
    checks=[]
    for i in range(int(route.path.get_spline_length()/25)+1):
        checks.append(bool(route.sample_ground(i*25,0,105 if route.vehicles else 30)))
    if not all(checks):route.validated=False
    route.provenance=json.dumps({'source':'Scene-author review of the rendered MainLevel dirt road, 2026-09-29','scope':'Local simulation corridor; no public access claim','half_width_cm':width,'collision_samples':len(checks),'collision_passed':sum(checks),'runtime_status':'Pending moving-agent review','osm_candidates':'Rejected due to mismatch with rendered road and vegetation'})
    reports.append({'actor':route.get_actor_label(),'validated':route.validated,'samples':len(checks),'passed':sum(checks),'length_cm':route.path.get_spline_length()})
folder=Path(unreal.Paths.project_saved_dir())/'LivingWorld/MainRoutes'
(folder/'authored-corridor-checks.json').write_text(json.dumps(reports,indent=2))
sub.set_selected_level_actors(routes)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('MAIN_AUTHORED_CHECKS',reports)
