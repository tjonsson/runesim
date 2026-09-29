import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='CesiumRouteReview'
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.SystemLibrary.quit_editor()
