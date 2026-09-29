#!/usr/bin/env bash
export DISPLAY=:0
set -e
cd "$(dirname "$0")"
source "$HOME/Code/ros2_humble/install/setup.bash"
source bridge_ws/install/setup.bash
source .bridge_venv/bin/activate
# This test bridge only exposes the two simulated camera topics on the Pi LAN IP.
# Run the websocket node directly: no general rosapi introspection server.
exec python3 bridge_ws/install/rosbridge_server/lib/rosbridge_server/rosbridge_websocket \
  --ros-args -p address:=192.168.18.9 -p port:=9090 -p url_path:=/runesim \
  -p topics_glob:="\"['/runesim/ptz/ptz_1/command','/runesim/ptz/ptz_1/state']\"" \
  -p topics_pub_glob:="\"['/runesim/ptz/ptz_1/state']\"" \
  -p topics_sub_glob:="\"['/runesim/ptz/ptz_1/command']\"" \
  -p services_glob:="\"['/runesim_test/no_services']\"" \
  -p params_glob:="\"['/runesim_test/no_params']\"" \
  -p max_message_size:=16384 -p incoming_queue_size:=16 -p write_queue_size:=16 \
  -p websocket_ping_interval:=15.0 -p websocket_ping_timeout:=15.0
