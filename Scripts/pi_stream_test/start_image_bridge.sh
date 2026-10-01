#!/usr/bin/env bash
# On-demand ROS2 camera images (compressed and raw) from the simulated PTZ. Runs in the background:
#   ./start_image_bridge.sh            start (no-op if already running)
#   ./start_image_bridge.sh --restart  restart
# Logs: logs/image-bridge.log. Stop: kill -INT "$(cat logs/image-bridge.pid)".
export DISPLAY=:0
set -e
cd "$(dirname "$0")"
mkdir -p logs
if [ -f logs/image-bridge.pid ] && kill -0 "$(cat logs/image-bridge.pid)" 2>/dev/null; then
  if [ "${1:-}" != '--restart' ]; then echo 'Image bridge already running'; exit 0; fi
  kill -INT "$(cat logs/image-bridge.pid)"; sleep 2
fi
source "$HOME/Code/ros2_humble/install/setup.bash"
source install/setup.bash
export AMENT_PREFIX_PATH="$PWD/install/runesim_test:${AMENT_PREFIX_PATH:-}"
nohup "$PWD/install/runesim_test/lib/runesim_test/ptz_image_bridge" > logs/image-bridge.log 2>&1 < /dev/null &
echo $! > logs/image-bridge.pid
echo "Image bridge started (PID $!); log: logs/image-bridge.log"
