"""Place the simulated PTZ beside MainLevel's authored road and enable its LAN feed."""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=sub.get_all_level_actors()
assert not any(isinstance(a,unreal.SimPTZ) for a in actors),'Main PTZ already exists'
road=next(a for a in actors if isinstance(a,unreal.LivingRoute) and a.vehicles)
d=road.path.get_spline_length()*.45
p=road.path.get_location_at_distance_along_spline(d,unreal.SplineCoordinateSpace.WORLD)+road.path.get_right_vector_at_distance_along_spline(d,unreal.SplineCoordinateSpace.WORLD)*600
hit=unreal.SystemLibrary.line_trace_single_for_objects(world,p+unreal.Vector(0,0,10000),p-unreal.Vector(0,0,10000),[unreal.ObjectTypeQuery.ECC_WORLD_STATIC],True,[a for a in actors if not isinstance(a,unreal.Cesium3DTileset)],unreal.DrawDebugTrace.NONE)
assert hit,'Load terrain first'
fields=unreal.GameplayStatics.get_default_object().call_method('BreakHitResult',args=(hit,))
assert fields[0] and isinstance(fields[9],unreal.Cesium3DTileset)
camera=sub.spawn_actor_from_class(unreal.SimPTZ,fields[5]+unreal.Vector(0,0,3))
camera.set_actor_label('Living Main PTZ');camera.tags=['LivingWorld.MainCamera']
camera.camera_id='ptz-1';camera.stream.auto_start=True
camera.stream.signalling_url='ws://127.0.0.1:8888'
camera.rosbridge_url='ws://192.168.18.9:9090/runesim';camera.enable_ros=True
camera.set_ptz(30,18,85)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('MAIN_PTZ',camera.get_actor_location())
