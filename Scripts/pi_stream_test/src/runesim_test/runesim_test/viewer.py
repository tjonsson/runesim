import json
import os
from pathlib import Path
import signal
import subprocess
import time

import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class Viewer(Node):
    def __init__(self):
        super().__init__('runesim_stream_viewer')
        url = self.declare_parameter('stream_url',
            'http://192.168.18.8/?StreamerId=ptz-1&AutoConnect=true&AutoPlayVideo=true&StartVideoMuted=true').value
        self.url = str(url)
        self.root = Path(os.environ.get('RUNESIM_TEST_ROOT', Path.cwd()))
        self.root.joinpath('logs').mkdir(exist_ok=True)
        self.root.joinpath('logs/viewer-node.pid').write_text(str(os.getpid()))
        self.log = self.root.joinpath('logs/chromium.log').open('a')
        self.reconnect = bool(self.declare_parameter('auto_reconnect', True).value)
        self.software_video = bool(self.declare_parameter('software_video', False).value)
        self.restart_count = 0
        self.last_verified = time.monotonic()
        self.browser = None
        self.start_browser()
        self.publisher = self.create_publisher(String, '/runesim_test/stream_status', 10)
        self.timer = self.create_timer(2.0, self.publish_status)
        self.get_logger().info('Visible camera viewer: ' + self.url)

    def start_browser(self):
        env = dict(os.environ, DISPLAY=':0', XAUTHORITY=str(Path.home()/'.Xauthority'))
        # A separate profile keeps this test independent of the user's browser session.
        command = [
            'chromium', '--ozone-platform=x11',
            '--user-data-dir=' + str(self.root/'browser-profile'),
            '--no-first-run', '--no-default-browser-check',
            '--autoplay-policy=no-user-gesture-required',
            '--proxy-bypass-list=192.168.18.8;localhost;127.0.0.1',
            '--remote-debugging-address=127.0.0.1', '--remote-debugging-port=9222',
            '--start-maximized', '--app=' + self.url]
        if self.software_video:
            # Diagnostic comparison for Pi Chromium video-buffer errors; not a proven fix.
            # Browser isolation and security settings remain at their defaults.
            command += ['--disable-accelerated-video-decode', '--disable-gpu-memory-buffer-video-frames']
        self.browser = subprocess.Popen(command, env=env,
            stdout=self.log, stderr=subprocess.STDOUT, start_new_session=True)
        self.browser_started = time.time()
        self.last_verified = time.monotonic()

    def publish_status(self):
        data = {'url': self.url, 'display': ':0', 'browser_running': self.browser.poll() is None,
                'time_unix': time.time(), 'video_verified': False,
                'reconnect_count': self.restart_count, 'software_video': self.software_video}
        fresh_measurement = False
        status = self.root/'logs/browser-status.json'
        try:
            browser = json.loads(status.read_text())
            age = time.time() - browser['time_unix']
            fresh_measurement = 0 <= age < 10 and browser['time_unix'] >= self.browser_started
            data.update(browser=browser, metrics_age_seconds=age,
                        video_verified=fresh_measurement and browser.get('frames_advanced', False))
        except (OSError, ValueError, KeyError):
            pass
        self.publisher.publish(String(data=json.dumps(data)))
        if data['video_verified']:
            self.last_verified = time.monotonic()
        elif self.reconnect and time.monotonic() - self.last_verified > 30 and (fresh_measurement or self.browser.poll() is not None):
            # Restart only our dedicated viewer process group. Never alter the
            # user's ordinary Chromium session. A missing monitor alone is not a failure.
            self.get_logger().warning('Video stalled; reconnecting the dedicated test viewer')
            if self.stop_browser():
                self.restart_count += 1
                self.start_browser()
            else:
                self.last_verified = time.monotonic()

    def stop_browser(self):
        if self.browser and self.browser.poll() is None:
            os.killpg(self.browser.pid, signal.SIGTERM)
            try: self.browser.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.get_logger().error('Viewer did not stop; refusing to launch a duplicate')
                return False
        return True

    def close(self):
        self.stop_browser()
        self.log.close()


def main():
    rclpy.init()
    node = Viewer()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.close()
        node.destroy_node()
        if rclpy.ok(): rclpy.shutdown()
