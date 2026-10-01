"""Offline checks for the PS5 teleop mapping (run on the Pi with ROS2 sourced)."""
import math
import sys

sys.path.insert(0, 'src/runesim_test')
from runesim_test.gamepad import shape, step_pose, eviocgabs, matches_device  # noqa: E402

LIMITS = {'pan': 170., 'min_tilt': -80., 'max_tilt': 80., 'min_fov': 5., 'max_fov': 100.}
RATES = {'pan': 90., 'tilt': 60., 'zoom': 1.2}
checks = {
    'deadzone ignores drift': shape(.05, .08) == 0.0,
    'full deflection is full rate': shape(-1.0, .08) == -1.0,
    'response is finer near centre': 0 < shape(.5, .08) < .5,
    'pan integrates': abs(step_pose((0, 0, 60), {'pan': 1, 'tilt': 0, 'zoom': 0}, 1., False, LIMITS, RATES)[0] - 90) < 1e-6,
    'zoomed-in aiming is slower': step_pose((0, 0, 10), {'pan': 1, 'tilt': 0, 'zoom': 0}, 1., False, LIMITS, RATES)[0] < 20,
    'precision mode quarters the rate': abs(step_pose((0, 0, 60), {'pan': 1, 'tilt': 0, 'zoom': 0}, 1., True, LIMITS, RATES)[0] - 22.5) < 1e-6,
    'pan clamps at the mechanical limit': step_pose((169, 0, 60), {'pan': 1, 'tilt': 0, 'zoom': 0}, 1., False, LIMITS, RATES)[0] == 170,
    'tilt clamps': step_pose((0, -79, 60), {'pan': 0, 'tilt': -1, 'zoom': 0}, 1., False, LIMITS, RATES)[1] == -80,
    'zoom in narrows FOV': step_pose((0, 0, 60), {'pan': 0, 'tilt': 0, 'zoom': 1}, 1., False, LIMITS, RATES)[2] < 60,
    'zoom clamps at 5 degrees': step_pose((0, 0, 6), {'pan': 0, 'tilt': 0, 'zoom': 1}, 5., False, LIMITS, RATES)[2] == 5,
    'EVIOCGABS(ABS_X) ioctl number': eviocgabs(0) == 0x80184540,
    'Bluetooth gamepad name matches': matches_device('DualSense Wireless Controller', 'DualSense Wireless Controller'),
    'USB gamepad name matches': matches_device('Sony Interactive Entertainment DualSense Wireless Controller', 'DualSense Wireless Controller'),
    'motion sensors do not match': not matches_device('Sony Interactive Entertainment DualSense Wireless Controller Motion Sensors', 'DualSense Wireless Controller'),
    'touchpad does not match': not matches_device('DualSense Wireless Controller Touchpad', 'DualSense Wireless Controller'),
}
for name, ok in checks.items():
    print(('PASS ' if ok else 'FAIL ') + name)
sys.exit(0 if all(checks.values()) else 1)
