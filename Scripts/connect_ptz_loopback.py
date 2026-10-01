"""Reconnect the playing PIE PTZ to a ROS URL (default: loopback engage harness); restore with RUNESIM_PTZ_ROS_URL.

Run twice: the first call disables ROS so the next frame closes the socket; the second call sets
the URL and re-enables ROS (the PTZ reconnects within three seconds).
"""
import os
import unreal

url = os.environ.get('RUNESIM_PTZ_ROS_URL', 'ws://127.0.0.1:9095')
world = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
ptz = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SimPTZ)[0]
if ptz.enable_ros:
    ptz.enable_ros = False
    print('PTZ_ROS_DISABLED', ptz.rosbridge_url)
else:
    previous = ptz.rosbridge_url
    ptz.rosbridge_url = url
    ptz.enable_ros = True
    print('PTZ_ROS_URL', previous, '->', url)
