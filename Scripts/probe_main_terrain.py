"""Sample MainLevel's actual streamed collision without saving scene changes."""
import unreal,json
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
ignored=[a for a in actors if not isinstance(a,unreal.Cesium3DTileset)]
points=[]
for x in range(-30000,30001,5000):
    for y in range(-30000,30001,5000):
        hit=unreal.SystemLibrary.line_trace_single_for_objects(world,unreal.Vector(x,y,100000),unreal.Vector(x,y,-200000),[unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1],True,ignored,unreal.DrawDebugTrace.NONE)
        if not hit:continue
        fields=unreal.GameplayStatics.get_default_object().call_method('BreakHitResult',args=(hit,))
        if fields[0]:points.append({'xy':[x,y],'location':[fields[5].x,fields[5].y,fields[5].z],'normal':[fields[7].x,fields[7].y,fields[7].z]})
out=Path(unreal.Paths.project_saved_dir())/'LivingWorld/main-terrain-probe.json'
out.write_text(json.dumps(points,indent=2))
print('MAIN_TERRAIN_PROBE',len(points),'of 169',min((p['location'][2] for p in points),default=None),max((p['location'][2] for p in points),default=None))
globals()['main_original_editor_camera']=unreal.EditorLevelLibrary.get_level_viewport_camera_info()
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(0,0,50000),unreal.Rotator(pitch=-90,yaw=0))
