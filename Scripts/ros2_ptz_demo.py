"""ROS2 operator example. Requires rclpy and a running rosbridge_server.

Unreal: SimPTZ CameraId=ptz-1, EnableROS=true, RosbridgeUrl=ws://<ROS host>:9090.
Only drives the simulated camera. X=pan degrees, Y=tilt degrees, Z=horizontal FOV.
"""
import argparse
import re
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Vector3
from std_msgs.msg import String

parser=argparse.ArgumentParser()
parser.add_argument('--id',default='ptz-1')
parser.add_argument('--pan',type=float,default=45)
parser.add_argument('--tilt',type=float,default=15)
parser.add_argument('--fov',type=float,default=60)
args=parser.parse_args()
camera_token = re.sub(r'[^a-zA-Z0-9_]', '_', args.id) or 'camera'
if camera_token[0].isdigit(): camera_token = 'camera_' + camera_token
rclpy.init()
node=Node('runesim_ptz_operator')
publisher=node.create_publisher(Vector3,f'/runesim/ptz/{camera_token}/command',1)
subscription=node.create_subscription(String,f'/runesim/ptz/{camera_token}/state',lambda msg:node.get_logger().info(msg.data),1)
timer=node.create_timer(1.0,lambda:publisher.publish(Vector3(x=args.pan,y=args.tilt,z=args.fov)))
try: rclpy.spin(node)
except KeyboardInterrupt: pass
finally:
    node.destroy_node(); rclpy.shutdown()
