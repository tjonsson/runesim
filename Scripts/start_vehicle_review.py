"""Run the demo without competing for the packaged camera's streamer ID."""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='LivingWorldDemo'
globals()['vehicle_review_cameras']=[]
globals()['living_review_start_stream']=False
for actor in actors.get_all_level_actors():
    if isinstance(actor,unreal.SimPTZ):
        vehicle_review_cameras.append((actor,actor.enable_ros,actor.stream.auto_start))
        actor.enable_ros=False
        actor.stream.auto_start=False
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_play_simulate()
print('VEHICLE_REVIEW_STARTED')
