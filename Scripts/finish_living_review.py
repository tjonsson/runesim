import unreal
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actors.get_all_level_actors():
    if actor.get_actor_label().startswith('LivingReview_'):actors.destroy_actor(actor)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world,'Automation RunTests RuneSim.LivingWorld')
