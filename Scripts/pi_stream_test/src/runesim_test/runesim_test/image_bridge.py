"""On-demand ROS2 images from the simulated RuneSim cameras (ROS2 node `ptz_image_bridge`).

The simulator publishes JPEG frames on /runesim/ptz/<id>/image/compressed through rosbridge,
but rosbridge cannot tell it whether anyone is listening. This node watches the ROS graph and
publishes the wanted rate on /runesim/ptz/<id>/image_demand (std_msgs/Float32, 0 = off) every
second, so the simulator only encodes frames while they are subscribed.

While images are demanded it also monitors frame age: the websocket link shares Wi-Fi with the
WebRTC video, and frames sent faster than it can carry queue up. When the delay above the
link's baseline grows past half a second the requested rate is reduced; it recovers gradually.

It serves /runesim/ptz/<id>/image_raw (sensor_msgs/Image, bgr8), decoded from the JPEG stream,
only while that topic has subscribers.
"""
import time

import numpy as np
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import CompressedImage, Image
from std_msgs.msg import Float32

try:
    import cv2
except ImportError:  # Raw images need OpenCV; compressed images do not.
    cv2 = None


class CameraBridge:
    def __init__(self, node, camera, rate):
        self.node = node
        self.prefix = '/runesim/ptz/' + camera
        self.target = rate
        self.rate = rate
        self.demand = node.create_publisher(Float32, self.prefix + '/image_demand', 1)
        self.raw = node.create_publisher(Image, self.prefix + '/image_raw', 2)
        self.monitor = None
        self.ages = []
        self.baseline = None
        self.baseline_time = 0.0
        self.wanted = None

    def update(self):
        raw_listeners = self.node.count_subscribers(self.prefix + '/image_raw')
        # Our own monitoring subscription is not an external listener.
        listeners = self.node.count_subscribers(self.prefix + '/image/compressed') - (1 if self.monitor else 0)
        active = listeners > 0 or raw_listeners > 0
        if active and self.monitor is None:
            self.monitor = self.node.create_subscription(CompressedImage, self.prefix + '/image/compressed', self.received, 2)
            self.rate, self.ages, self.baseline = self.target, [], None
        elif not active and self.monitor is not None:
            self.node.destroy_subscription(self.monitor)
            self.monitor = None
        if active and self.ages:
            # Frame age includes clock offset between hosts; the smallest recent age is the baseline.
            now = time.monotonic()
            if self.baseline is None or now - self.baseline_time > 30:
                self.baseline, self.baseline_time = min(self.ages), now
            self.baseline = min(self.baseline, min(self.ages))
            lag = sorted(self.ages)[len(self.ages) // 2] - self.baseline
            if lag > .5:
                self.rate = max(1.0, self.rate * .7)
            elif lag < .2:
                self.rate = min(self.target, self.rate + .5)
            self.ages = []
        wanted = round(self.rate, 1) if active else 0.0
        if (wanted > 0) != bool(self.wanted) or (wanted and abs(wanted - self.wanted) >= 1):
            self.node.get_logger().info('%s images %s' % (self.prefix, ('at %.1f Hz' % wanted) if wanted else 'off'))
            self.wanted = wanted
        self.demand.publish(Float32(data=float(wanted)))

    def received(self, message):
        self.ages.append(time.time() - (message.header.stamp.sec + message.header.stamp.nanosec * 1e-9))
        if cv2 is None or not self.node.count_subscribers(self.prefix + '/image_raw'):
            return
        frame = cv2.imdecode(np.frombuffer(bytes(message.data), dtype=np.uint8), cv2.IMREAD_COLOR)
        if frame is None:
            return
        image = Image()
        image.header = message.header
        image.height, image.width = frame.shape[:2]
        image.encoding = 'bgr8'
        image.is_bigendian = 0
        image.step = image.width * 3
        image.data = frame.tobytes()
        self.raw.publish(image)


class ImageBridge(Node):
    def __init__(self):
        super().__init__('ptz_image_bridge')
        cameras = list(self.declare_parameter('cameras', ['ptz_1']).value)
        rate = float(self.declare_parameter('rate_hz', 10.0).value)
        self.bridges = [CameraBridge(self, camera, rate) for camera in cameras]
        self.create_timer(1.0, lambda: [bridge.update() for bridge in self.bridges])
        self.get_logger().info('Image demand for %s at up to %.1f Hz%s' % (
            ', '.join(cameras), rate, '' if cv2 else ' (no OpenCV: image_raw disabled)'))


def main():
    rclpy.init()
    node = ImageBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == '__main__':
    main()
