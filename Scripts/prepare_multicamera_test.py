"""Add a temporary second camera. Run finish_living_review.py after ending PIE."""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='LivingWorldDemo'
camera=actors.spawn_actor_from_class(unreal.SimPTZ,unreal.Vector(0,0,1500))
camera.set_actor_label('LivingReview_SecondCamera')
camera.camera_id='air-2'
camera.stream.set_editor_property('auto_start',True)
camera.set_ptz(90,-10,85)
# Deliberately not transient: transient editor actors are not copied into PIE.
# Do not save this test actor in the demo.
print('SECOND_CAMERA_READY')
