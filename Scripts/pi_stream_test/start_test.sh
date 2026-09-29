#!/usr/bin/env bash
export DISPLAY=:0
set -e
cd "$(dirname "$0")"
export RUNESIM_TEST_ROOT="$PWD"
source "$HOME/Code/ros2_humble/install/setup.bash"
source install/setup.bash
# This host's colcon Python builder omits the ament-prefix environment hook.
export AMENT_PREFIX_PATH="$PWD/install/runesim_test:${AMENT_PREFIX_PATH:-}"
exec "$PWD/install/runesim_test/lib/runesim_test/stream_viewer" "$@"
