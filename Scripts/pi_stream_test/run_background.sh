#!/usr/bin/env bash
export DISPLAY=:0
set -e
cd "$(dirname "$0")"
mkdir -p logs
if [ -f logs/viewer-node.pid ]; then
  viewer_pid=$(cat logs/viewer-node.pid)
  if kill -0 "$viewer_pid" 2>/dev/null; then
    case "$(tr '\0' ' ' < /proc/"$viewer_pid"/cmdline)" in
      *runesim_test*stream_viewer*) ;;
      *) echo 'PID belongs to another process; left untouched' >&2; exit 1;;
    esac
    if [ "${1:-}" != '--restart' ]; then echo 'Viewer already running'; exit 0; fi
    kill -INT "$viewer_pid"
    for attempt in $(seq 1 20); do
      kill -0 "$viewer_pid" 2>/dev/null || break
      sleep .5
    done
    if kill -0 "$viewer_pid" 2>/dev/null; then echo 'Viewer has not stopped; no duplicate launched' >&2; exit 1; fi
  fi
fi
if [ "${1:-}" = '--restart' ]; then shift; fi
nohup bash ./start_test.sh "$@" > logs/viewer.log 2>&1 < /dev/null &
echo $! > logs/viewer.pid
echo 'Viewer launched on DISPLAY=:0'
