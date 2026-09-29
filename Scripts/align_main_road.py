"""Author the reviewed visible dirt-road section from a recorded MainLevel viewport.

The OSM candidate did not align with the rendered road. These observed screen
points are projected onto actual tile collision, then sampled every metre.
This script requires the exact recorded camera and viewport, not arbitrary views.
"""
import unreal, json, math
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
route=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.LivingRoute) and a.vehicles and 'LivingWorld.MainCorridor' in [str(t) for t in a.tags])
camera,rotation=unreal.EditorLevelLibrary.get_level_viewport_camera_info()
assert (camera-unreal.Vector(46.422720,269.587201,-6261.739536)).length()<1
assert abs(rotation.pitch+90)<.01
ignored=[a for a in actors.get_all_level_actors() if not isinstance(a,unreal.Cesium3DTileset)]
def trace(a,b):
    hit=unreal.SystemLibrary.line_trace_single_for_objects(world,a,b,[unreal.ObjectTypeQuery.ECC_WORLD_STATIC],True,ignored,unreal.DrawDebugTrace.NONE)
    assert hit
    fields=unreal.GameplayStatics.get_default_object().call_method('BreakHitResult',args=(hit,))
    assert fields[0] and isinstance(fields[9],unreal.Cesium3DTileset)
    return fields[5]
pixels=[(388,299),(483,344),(572,396),(629,437),(659,476),(690,509),(760,554),(824,600),(851,650)]
control=[]
for x,y in pixels:
    direction=unreal.Vector(-(y-462.5)/402,(x-655)/402,-1)
    control.append(trace(camera,camera+direction*100000))
ground=[]
for a,b in zip(control,control[1:]):
    n=max(1,math.ceil((b-a).length()/100))
    for i in range(n):
        p=a+(b-a)*(i/n)
        ground.append(trace(p+unreal.Vector(0,0,2000),p-unreal.Vector(0,0,2000)))
ground.append(control[-1])
route.modify();route.path.modify();route.validated=False;route.reviewed_half_width_cm=0
route.path.set_spline_points(ground,unreal.SplineCoordinateSpace.WORLD)
for i in range(len(ground)):route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
route.set_actor_label('Living Main Dirt road candidate')
route.provenance=json.dumps({'source':'Manually reviewed Google Photorealistic 3D Tiles viewport in MainLevel, 2026-09-29','osm_candidate_rejected':'1047212312 did not align with rendered road','scope':'Local authored simulation corridor; no legal/public access determination','review_status':'Awaiting complete support and moving-wheel checks','viewport_pixels':pixels})
actors.set_selected_level_actors([route])
folder=Path(unreal.Paths.project_saved_dir())/'LivingWorld/MainRoutes'
(folder/'authored-road.json').write_text(json.dumps({'camera':[camera.x,camera.y,camera.z],'viewport':[253,137,1057,788],'control':[[p.x,p.y,p.z] for p in control],'length_cm':route.path.get_spline_length()},indent=2))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('AUTHORED_ROAD',len(ground),route.path.get_spline_length())
