"""Enable simulated PTZ ROS2 control in the running demo using the tested Pi."""
import unreal
worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
assert len(worlds) == 1 and 'LivingWorldDemo' in worlds[0].get_name()
ptz = unreal.GameplayStatics.get_all_actors_of_class(worlds[0], unreal.SimPTZ)[0]
ptz.set_editor_property('rosbridge_url', 'ws://192.168.18.9:9090/runesim')
ptz.set_editor_property('enable_ros', True)
print('PI_ROS_PTZ_ENABLED', ptz.get_editor_property('camera_id'))
