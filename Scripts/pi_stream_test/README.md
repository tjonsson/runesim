# RuneSim Pi streaming test

Runs a visible Chromium WebRTC viewer on `DISPLAY=:0` and publishes JSON status on
`/runesim_test/stream_status` (`std_msgs/msg/String`). Video is displayed through
WebRTC; this package does not publish `sensor_msgs/Image` frames.

ROS2 Humble: `~/Code/ros2_humble/install/setup.bash`.

```bash
export DISPLAY=:0
cd /home/optimus/Code/runesim_test
source ~/Code/ros2_humble/install/setup.bash
colcon build --packages-select runesim_test --symlink-install
./start_test.sh
```

The RuneSim Windows host must run its demo and PTZ streamer `ptz-1`. Default URL:
`http://192.168.18.8/?StreamerId=ptz-1&AutoConnect=true&AutoPlayVideo=true&StartVideoMuted=true`.
Override it using `./start_test.sh --ros-args -p stream_url:='http://HOST/?StreamerId=ptz-1&AutoConnect=true&AutoPlayVideo=true'`.

The separate `browser-profile` is only for this test. Browser diagnostics bind to
loopback port 9222. No password is stored. Press Ctrl+C to stop a foreground run.
For the background test, `kill -INT "$(cat logs/viewer-node.pid)"` stops the ROS viewer.
The monitoring process records its PID in `logs/monitor.pid`.

To run the optional video monitor in a separate terminal:

```bash
python3 -m venv .test_venv
.test_venv/bin/python -m pip install websocket-client==1.9.0
.test_venv/bin/python monitor_browser.py
```

```bash
source ~/Code/ros2_humble/install/setup.bash
ros2 topic echo /runesim_test/stream_status --once
```

`logs/browser-status.json` contains video dimensions, presented/dropped frame counts,
decode counts and playback progress. Where available, `inbound_video` contains Epic's
WebRTC receive/decode statistics and their sample time. `observed_fps` measures presented
frames; it is null on the first sample or after a counter/session reset.
`video_verified` is false until fresh advancing presented frames are observed.

The viewer restarts only its own Chromium process after 30 seconds without
verified progress when fresh diagnostics confirm a stall, or when Chromium exits.
Missing monitor data alone does not trigger a restart. Set the ROS parameter
`auto_reconnect:=false` to disable this watchdog. Status includes `reconnect_count`.
Use `./run_background.sh --restart` to restart the test viewer after updating it;
the helper verifies the recorded process before stopping it.

For diagnostic comparison only, `./run_background.sh --restart --ros-args -p software_video:=true`
disables accelerated video decoding and GPU-memory-buffer video frames. The September 29
trial still stalled; this is **not a confirmed fix**. The default is false, and the Pi
was restored to that default after the trial. Run `./run_background.sh --restart` to restore it.

For simulated PTZ control, `build_rosbridge.sh` builds a pinned official Humble
rosbridge revision in an isolated workspace. `start_rosbridge.sh` exposes the
camera command/state topics on the test LAN, and `test_ptz_roundtrip.py` checks
pan, tilt, FOV, limits and fresh frames before restoring the initial pose.
The test bridge uses 15-second WebSocket heartbeat/timeout settings to retire
dead sessions after network interruptions.
See `docs/pi-stream-test.md` in the RuneSim repository for setup and results.

For a sustained acceptance check while the viewer and monitor run:

```bash
python3 test_stream_stability.py --seconds 600
```

Use `--report cuda-stability --minimum-median-fps 28 --seconds 1200` for a separately named acceptance report with a median presented-FPS floor. Report names are restricted to simple basenames inside `logs`; previous reports with other names remain intact. The JSON also records presentation-frame drops. A passing short run does not establish multi-hour endurance.

On Windows, `Scripts/measure_stream_host.ps1 -Seconds 1200 -Report cuda-host` records the packaged process identity, private memory, working set and CPU time, plus device-wide NVIDIA GPU memory/utilization when `nvidia-smi` is available. It refuses to overwrite an existing report and fails its process-continuity check if the original game exits. GPU memory belongs to the whole device and cannot be attributed solely to the simulator.

It writes `logs/stability-report.json` and fails on stale/stalled samples, page or
counter resets, or additional WebRTC freezes. It does not reconnect the viewer.
The Unreal project caps adaptive WebRTC video at 6 Mbit/s for the tested 720p feed;
higher-resolution workloads may need a different cap.

The tested Pi's Wi-Fi profile now disables power saving. `recover_wifi.sh` is an
explicit manual recovery for its Broadcom/WCC adapter if the same adapter failure
recurs. It reloads only those Wi-Fi modules and reconnects the existing profile;
SSH and video briefly disconnect. It never forces module removal or reboots the Pi.
This is not an automatic or periodic repair. It requires the existing passwordless
sudo permission for these operations. See the test guide before using it.

## PS5 DualSense teleoperation

`ps5_ptz_teleop` drives the simulated PTZ from a DualSense paired over Bluetooth or plugged in by USB. It reads the kernel's `hid-playstation` input device directly (no extra packages) and waits and reconnects when the controller sleeps. If a Bluetooth connection fails in the kernel (`dmesg` shows `Failed to retrieve DualSense firmware info` or `calibration info`), press the PS button again or plug in a USB cable. The pose follows `/runesim/ptz/ptz_1/state`.

```bash
./start_teleop.sh              # background; ./start_teleop.sh --restart after rebuilding
tail -f logs/teleop.log
ros2 topic echo /runesim_test/gamepad
```

| Control | Action |
|---|---|
| Left stick | Pan / tilt (slower when zoomed in) |
| Right stick up/down | Zoom in / out |
| L2 (hold) | Precision aim (quarter speed) |
| D-pad | 1° nudge |
| Cross | Designate target |
| R1 | Next target |
| Square | Toggle tracking |
| R2 (full press) | Launch interceptor (virtual) |
| Triangle | Abort interceptors |
| Circle | Clear designation |
| L1 | Cycle view: tripod / missile seeker / chase |
| Options | Return to the view at start |
| Create | Simulated artillery strike at the crosshair (blast, fire, smoke) |
| PS | Smoke screen at the crosshair |

While the simulator is tracking a target, it steers the camera and the sticks are ignored; press Square to take control back. `python3 test_gamepad_logic.py` checks the mapping offline.

## Camera images on ROS2

The simulator publishes JPEG frames of the `ptz-1` view on `/runesim/ptz/ptz_1/image/compressed` only while they are wanted. `ptz_image_bridge` watches for subscribers and requests up to 10 Hz, easing off if frames queue. It also serves decoded `bgr8` frames on `/runesim/ptz/ptz_1/image_raw`.

```bash
./start_image_bridge.sh        # background; --restart after rebuilding
ros2 topic hz /runesim/ptz/ptz_1/image/compressed
```
