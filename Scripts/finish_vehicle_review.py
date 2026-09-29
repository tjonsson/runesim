"""Restore camera settings after the review, with PIE already stopped."""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
for actor,ros,auto in globals().get('vehicle_review_cameras',[]):
    actor.enable_ros=ros
    actor.stream.auto_start=auto
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in globals().get('vehicle_review_visuals',[]):
    if unreal.SystemLibrary.is_valid(actor): actors.destroy_actor(actor)
globals()['vehicle_review_visuals']=[]
globals().pop('living_review_start_stream',None)
globals()['vehicle_review_cameras']=[]
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('VEHICLE_REVIEW_RESTORED')
