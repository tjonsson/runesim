"""Save the demo's primary PTZ for unattended packaged streaming on the test LAN."""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='LivingWorldDemo'
camera=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SimPTZ)[0]
camera.modify(); camera.stream.modify()
camera.stream.set_editor_property('auto_start',True)
camera.stream.set_editor_property('signalling_url','ws://127.0.0.1:8888')
camera.set_editor_property('rosbridge_url','ws://192.168.18.9:9090/runesim')
camera.set_editor_property('enable_ros',True)
camera.set_ptz(45,15,85)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('DEMO_STREAM_CONFIGURED',camera.get_path_name())
