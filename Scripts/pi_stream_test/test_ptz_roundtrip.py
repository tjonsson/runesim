"""Verify actual ROS2 -> rosbridge -> Unreal PTZ -> ROS2 state round trips.

Moves only the simulated PTZ. Always restores its initial angles before exit.
"""
import json
import math
from pathlib import Path
import time
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Vector3
from std_msgs.msg import String


def main():
    rclpy.init()
    node = Node('runesim_ptz_roundtrip_test')
    latest = None
    received_at = 0.0
    samples = []

    def receive(message):
        nonlocal latest, received_at
        try:
            value = json.loads(message.data)
            if all(math.isfinite(float(value[k])) for k in ('pan_deg', 'tilt_deg', 'horizontal_fov_deg', 'frame', 'capture_time')):
                latest = value
                received_at = time.monotonic()
                samples.append(value)
        except (ValueError, KeyError, TypeError):
            pass

    publisher = node.create_publisher(Vector3, '/runesim/ptz/ptz_1/command', 1)
    subscription = node.create_subscription(String, '/runesim/ptz/ptz_1/state', receive, 10)
    report = {'checks': [], 'passed': False}
    initial = None

    def wait_until(predicate, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            rclpy.spin_once(node, timeout_sec=.1)
            if predicate():
                return True
        return False

    def command_and_wait(command, expected):
        start_frame = latest['frame'] if latest else -1
        publisher.publish(Vector3(x=command[0], y=command[1], z=command[2]))
        return wait_until(lambda: latest is not None and latest['frame'] > start_frame
            and time.monotonic() - received_at < 1.0
            and all(abs(latest[k] - v) < .6 for k, v in zip(
                ('pan_deg', 'tilt_deg', 'horizontal_fov_deg'), expected)), 12)

    try:
        assert wait_until(lambda: latest is not None and publisher.get_subscription_count() > 0, 20), 'No ROS PTZ state/subscriber'
        initial = tuple(float(latest[k]) for k in ('pan_deg', 'tilt_deg', 'horizontal_fov_deg'))
        for name, command, expected in [
            ('pan_tilt_zoom', (-30., 12., 45.), (-30., 12., 45.)),
            ('reverse_motion', (40., -5., 75.), (40., -5., 75.)),
            ('finite_command_clamps', (200., -100., 2.), (170., -80., 5.)),
        ]:
            passed = command_and_wait(command, expected)
            report['checks'].append({'name': name, 'passed': passed, 'state': latest})
            assert passed, name + ' did not converge'
        report['passed'] = True
    except Exception as error:
        report['error'] = str(error)
    finally:
        if initial:
            restored = command_and_wait(initial, initial)
            report['restored_initial_pose'] = restored
            report['passed'] = report['passed'] and restored
        report['state_messages'] = len(samples)
        report['frames_monotonic'] = all(a['frame'] <= b['frame'] for a, b in zip(samples, samples[1:]))
        report['capture_times_monotonic'] = all(a['capture_time'] <= b['capture_time'] for a, b in zip(samples, samples[1:]))
        report['passed'] = report['passed'] and report['frames_monotonic'] and report['capture_times_monotonic']
        output = Path(__file__).resolve().parent / 'logs/ptz-roundtrip.json'
        output.parent.mkdir(exist_ok=True)
        output.write_text(json.dumps(report, indent=2))
        print(json.dumps(report, indent=2), flush=True)
        node.destroy_node()
        rclpy.shutdown()
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
