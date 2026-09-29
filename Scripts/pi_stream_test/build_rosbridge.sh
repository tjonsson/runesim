#!/usr/bin/env bash
export DISPLAY=:0
set -e
cd "$(dirname "$0")"
source "$HOME/Code/ros2_humble/install/setup.bash"
mkdir -p bridge_ws/src logs
bridge_revision=4734f01de7867525141121452d2db0582ea63d2d
if [ ! -d bridge_ws/src/rosbridge_suite/.git ]; then
  git init bridge_ws/src/rosbridge_suite
  git -C bridge_ws/src/rosbridge_suite remote add origin https://github.com/RobotWebTools/rosbridge_suite.git
  git -C bridge_ws/src/rosbridge_suite fetch --depth 1 origin "$bridge_revision"
  git -C bridge_ws/src/rosbridge_suite checkout --detach "$bridge_revision"
fi
test "$(git -C bridge_ws/src/rosbridge_suite rev-parse HEAD)" = "$bridge_revision" || { echo 'Existing rosbridge checkout differs from tested revision; preserved without changes.' >&2; exit 1; }
# Keep dependencies isolated from the working ROS source install and Raven venv.
python3 -m venv --system-site-packages .bridge_venv
.bridge_venv/bin/python -m pip install 'tornado==6.5.2' 'pymongo==4.14.1' 'cbor2==5.7.1' 'ujson==5.11.0'
git -C bridge_ws/src/rosbridge_suite rev-parse HEAD > logs/rosbridge-source.txt
cd bridge_ws
colcon build --packages-up-to rosbridge_server --cmake-args -DBUILD_TESTING=OFF --executor sequential
