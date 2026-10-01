"""PS5 DualSense teleoperation for the simulated RuneSim PTZ (ROS2 node `ps5_ptz_teleop`).

Reads the controller through the kernel's hid-playstation input device (no extra packages),
survives the controller sleeping/reconnecting, and publishes:
  /runesim/ptz/<id>/command  geometry_msgs/Vector3  (pan deg, tilt deg, horizontal FOV deg)
  /runesim/ptz/<id>/engage   std_msgs/String        (designate, next, track_on/off, fire, abort, clear, view, strike, smoke_screen, burn)
  /runesim_test/gamepad      std_msgs/String        (JSON connection/status, 1 Hz)
and follows /runesim/ptz/<id>/state for the camera's current pose and engagement status.

Controls: left stick pan/tilt, right stick up/down zoom, L2 held = precision aim,
D-pad = 1 degree nudges, Cross = designate, R1 = next target, Square = toggle tracking,
R2 = launch (virtual interceptor), Triangle = abort, Circle = clear, L1 = cycle view
(tripod / missile seeker / chase), Options = home view.
Engagement is simulated inside Unreal only.
"""
import fcntl
import glob
import json
import math
import os
import select
import struct
import threading
import time

import rclpy
from geometry_msgs.msg import Vector3
from rclpy.node import Node
from std_msgs.msg import String

EVENT = struct.Struct('qqHHi')  # struct input_event on 64-bit Linux
EV_KEY, EV_ABS = 0x01, 0x03
ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ, ABS_HAT0X, ABS_HAT0Y = 0, 1, 2, 3, 4, 5, 16, 17
BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST = 304, 305, 307, 308
BTN_TL, BTN_TR, BTN_SELECT, BTN_START, BTN_MODE = 310, 311, 314, 315, 316
BUTTON_NAMES = {BTN_SOUTH: 'cross', BTN_EAST: 'circle', BTN_NORTH: 'triangle', BTN_WEST: 'square',
                BTN_TL: 'l1', BTN_TR: 'r1', BTN_SELECT: 'create', BTN_START: 'options', BTN_MODE: 'ps'}
# Buttons that send an engagement command to the simulator.
ENGAGE = {'cross': 'designate', 'r1': 'next', 'triangle': 'abort', 'circle': 'clear', 'r2': 'fire', 'l1': 'view',
          # Simulated battlefield effects at the crosshair.
          'create': 'strike', 'ps': 'smoke_screen'}


def eviocgabs(axis):
    # _IOR('E', 0x40 + axis, struct input_absinfo) with a 6 x int32 payload.
    return (2 << 30) | (24 << 16) | (ord('E') << 8) | (0x40 + axis)


def shape(value, deadzone):
    """Deadzone with rescale, then a squared response for fine control near centre."""
    magnitude = abs(value)
    if magnitude <= deadzone:
        return 0.0
    scaled = min(1.0, (magnitude - deadzone) / (1.0 - deadzone))
    return math.copysign(scaled * scaled, value)


def step_pose(pose, sticks, dt, precise, limits, rates):
    """Integrate stick input into a new (pan, tilt, fov) pose. Pure function for testing."""
    pan, tilt, fov = pose
    # Angular rates scale with zoom so aiming stays controllable at narrow FOV.
    zoom_scale = max(.08, min(1.0, fov / 60.0)) * (.25 if precise else 1.0)
    pan += sticks['pan'] * rates['pan'] * zoom_scale * dt
    tilt += sticks['tilt'] * rates['tilt'] * zoom_scale * dt
    fov *= math.exp(-sticks['zoom'] * rates['zoom'] * dt)
    pan = max(-limits['pan'], min(limits['pan'], pan))
    tilt = max(limits['min_tilt'], min(limits['max_tilt'], tilt))
    fov = max(limits['min_fov'], min(limits['max_fov'], fov))
    return pan, tilt, fov


def matches_device(device_name, wanted):
    """Bluetooth names the gamepad 'DualSense Wireless Controller'; USB prefixes the vendor
    ('Sony Interactive Entertainment DualSense Wireless Controller'). The motion-sensor and
    touchpad devices end differently and never match."""
    return device_name == wanted or device_name.endswith(' ' + wanted)


class DualSense:
    """Background reader for the first input device whose name matches; reconnects automatically."""

    def __init__(self, name):
        self.name = name
        self.lock = threading.Lock()
        self.axes = {}
        self.ranges = {}
        self.pressed = []
        self.path = None
        self.running = True
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def find(self):
        for path in sorted(glob.glob('/dev/input/event*')):
            try:
                with open('/sys/class/input/%s/device/name' % os.path.basename(path)) as handle:
                    if matches_device(handle.read().strip(), self.name):
                        return path
            except OSError:
                continue
        return None

    def normalized(self, axis):
        with self.lock:
            if axis not in self.axes or axis not in self.ranges:
                return 0.0
            low, high = self.ranges[axis]
            value = self.axes[axis]
        if axis in (ABS_Z, ABS_RZ):
            return (value - low) / max(1, high - low)            # triggers 0..1
        if axis in (ABS_HAT0X, ABS_HAT0Y):
            return float(value)                                   # -1, 0, 1
        return (2.0 * (value - low) / max(1, high - low)) - 1.0   # sticks -1..1

    def take_presses(self):
        with self.lock:
            presses, self.pressed = self.pressed, []
        return presses

    def run(self):
        while self.running:
            path = self.find()
            if not path:
                self.path = None
                time.sleep(2.0)
                continue
            try:
                fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
            except OSError:
                time.sleep(2.0)
                continue
            try:
                with self.lock:
                    self.axes.clear(); self.ranges.clear(); self.pressed.clear()
                    for axis in (ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ, ABS_HAT0X, ABS_HAT0Y):
                        buffer = bytearray(24)
                        fcntl.ioctl(fd, eviocgabs(axis), buffer)
                        value, minimum, maximum = struct.unpack('iii', bytes(buffer[:12]))
                        self.axes[axis], self.ranges[axis] = value, (minimum, maximum)
                self.path = path
                pending = b''
                while self.running:
                    ready, _, _ = select.select([fd], [], [], .5)
                    if not ready:
                        continue
                    data = os.read(fd, EVENT.size * 64)
                    if not data:
                        break
                    pending += data
                    with self.lock:
                        while len(pending) >= EVENT.size:
                            _, _, kind, code, value = EVENT.unpack(pending[:EVENT.size])
                            pending = pending[EVENT.size:]
                            if kind == EV_ABS:
                                self.axes[code] = value
                            elif kind == EV_KEY and value == 1 and code in BUTTON_NAMES:
                                self.pressed.append(BUTTON_NAMES[code])
            except OSError:
                pass  # Controller slept or disconnected; rescan.
            finally:
                os.close(fd)
                self.path = None
                with self.lock:
                    self.axes.clear()


class Teleop(Node):
    def __init__(self):
        super().__init__('ps5_ptz_teleop')
        camera = str(self.declare_parameter('camera', 'ptz_1').value)
        self.prefix = '/runesim/ptz/' + camera
        self.device = DualSense(str(self.declare_parameter('device_name', 'DualSense Wireless Controller').value))
        self.deadzone = float(self.declare_parameter('deadzone', .08).value)
        self.rates = {'pan': float(self.declare_parameter('pan_rate_deg', 90.).value),
                      'tilt': float(self.declare_parameter('tilt_rate_deg', 60.).value),
                      'zoom': float(self.declare_parameter('zoom_rate', 1.2).value)}
        self.limits = {'pan': 170., 'min_tilt': -80., 'max_tilt': 80., 'min_fov': 5., 'max_fov': 100.}
        self.command_pub = self.create_publisher(Vector3, self.prefix + '/command', 10)
        self.engage_pub = self.create_publisher(String, self.prefix + '/engage', 10)
        self.status_pub = self.create_publisher(String, '/runesim_test/gamepad', 10)
        self.create_subscription(String, self.prefix + '/state', self.on_state, 10)
        self.state = None
        self.pose = None
        self.home = None
        self.last_command = 0.0
        self.tracking = False
        self.r2_down = False
        self.engagement = ''
        self.last_action = ''
        self.was_connected = False
        self.last_tick = time.monotonic()
        self.create_timer(1 / 30, self.tick)
        self.create_timer(1.0, self.publish_status)
        self.get_logger().info('Waiting for "%s"; controlling %s' % (self.device.name, self.prefix))

    def on_state(self, message):
        try:
            self.state = json.loads(message.data)
        except ValueError:
            return
        actual = (self.state['pan_deg'], self.state['tilt_deg'], self.state['horizontal_fov_deg'])
        engagement = self.state.get('engagement', {})
        tracking = bool(engagement.get('tracking'))
        # Follow the camera when this controller is idle, or when tracking just ended and the
        # simulator restored the operator's view.
        if self.pose is None or (not tracking and self.tracking) or time.monotonic() - self.last_command > 2.0:
            self.pose = actual
        if self.home is None:
            self.home = actual
        self.tracking = tracking
        view = engagement.get('view', '')
        if view and view != getattr(self, 'view', view):
            self.get_logger().info('View: ' + view)
        self.view = view
        status = engagement.get('status', '')
        if status and status != self.engagement:
            self.get_logger().info('Engagement: ' + status)
        self.engagement = status

    def engage(self, command):
        self.engage_pub.publish(String(data=command))
        self.last_action = command
        self.get_logger().info('Sent ' + command)

    def send_pose(self, pose):
        self.pose = pose
        self.last_command = time.monotonic()
        self.command_pub.publish(Vector3(x=float(pose[0]), y=float(pose[1]), z=float(pose[2])))

    def tick(self):
        now = time.monotonic()
        dt, self.last_tick = min(.1, now - self.last_tick), now
        connected = self.device.path is not None
        if connected != self.was_connected:
            self.get_logger().info('Controller ' + ('connected at ' + self.device.path if connected else 'disconnected'))
            self.was_connected = connected
        if not connected:
            return
        presses = self.device.take_presses()
        r2 = self.device.normalized(ABS_RZ)
        if r2 > .8 and not self.r2_down:
            presses.append('r2')
        self.r2_down = r2 > .5  # Hysteresis: release below half travel re-arms the trigger.
        for button in presses:
            if button in ENGAGE:
                self.engage(ENGAGE[button])
            elif button == 'square':
                self.engage('track_off' if self.tracking else 'track_on')
            elif button == 'options' and self.home is not None:
                if self.tracking:
                    self.engage('track_off')
                self.send_pose(self.home)
                self.last_action = 'home'
        if self.pose is None or self.tracking:
            return  # No state yet, or the simulator is steering the camera.
        sticks = {'pan': shape(self.device.normalized(ABS_X), self.deadzone),
                  'tilt': -shape(self.device.normalized(ABS_Y), self.deadzone),
                  'zoom': -shape(self.device.normalized(ABS_RY), self.deadzone)}
        hat_x, hat_y = self.device.normalized(ABS_HAT0X), self.device.normalized(ABS_HAT0Y)
        nudge = (hat_x != 0 or hat_y != 0) and not getattr(self, 'hat_held', False)
        self.hat_held = hat_x != 0 or hat_y != 0
        if not any(sticks.values()) and not nudge:
            return
        pose = step_pose(self.pose, sticks, dt, self.device.normalized(ABS_Z) > .5, self.limits, self.rates)
        if nudge:
            pose = step_pose((pose[0] + hat_x, pose[1] - hat_y, pose[2]), {'pan': 0, 'tilt': 0, 'zoom': 0},
                             0, False, self.limits, self.rates)
        self.send_pose(pose)

    def publish_status(self):
        self.status_pub.publish(String(data=json.dumps({
            'connected': self.device.path is not None, 'device': self.device.path, 'camera': self.prefix,
            'pose': self.pose, 'tracking': self.tracking, 'engagement': self.engagement,
            'last_action': self.last_action, 'time_unix': time.time()})))


def main():
    rclpy.init()
    node = Teleop()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.device.running = False
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == '__main__':
    main()
