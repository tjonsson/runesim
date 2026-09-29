"""Start the isolated demo PTZ feed without enabling a ROS connection."""
import unreal

worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
if not worlds or 'LivingWorldDemo' not in worlds[0].get_name():
    raise RuntimeError('Play LivingWorldDemo before starting its camera')
cameras = unreal.GameplayStatics.get_all_actors_of_class(worlds[0], unreal.SimPTZ)
if len(cameras) != 1:
    raise RuntimeError('Expected exactly one demo PTZ')
camera = cameras[0]
# Bound the demo rendering workload to the sensor's requested frame rate.
unreal.SystemLibrary.execute_console_command(worlds[0], 't.MaxFPS 30')
camera.set_ptz(45, 0, 70)
print('PTZ stream started:', camera.stream.start_stream(), 'ID:', camera.stream.stream_id)
