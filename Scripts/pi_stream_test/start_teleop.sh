#!/usr/bin/env bash
# PS5 DualSense teleoperation of the simulated PTZ over ROS2. Runs in the background:
#   ./start_teleop.sh            start (no-op if already running)
#   ./start_teleop.sh --restart  restart
# Logs: logs/teleop.log. Stop: kill -INT "$(cat logs/teleop.pid)".
export DISPLAY=:0
set -e
cd "$(dirname "$0")"
mkdir -p logs
if [ -f logs/teleop.pid ] && kill -0 "$(cat logs/teleop.pid)" 2>/dev/null; then
  if [ "${1:-}" != '--restart' ]; then echo 'Teleop already running'; exit 0; fi
  kill -INT "$(cat logs/teleop.pid)"; sleep 2
fi
source "$HOME/Code/ros2_humble/install/setup.bash"
source install/setup.bash
export AMENT_PREFIX_PATH="$PWD/install/runesim_test:${AMENT_PREFIX_PATH:-}"
nohup "$PWD/install/runesim_test/lib/runesim_test/ps5_ptz_teleop" > logs/teleop.log 2>&1 < /dev/null &
echo $! > logs/teleop.pid
echo "PS5 teleop started (PID $!); log: logs/teleop.log"
